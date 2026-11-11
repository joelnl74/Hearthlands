#pragma once

#include "Hydro.h"
#include "Data/ObjectDefinition.h"
#include "Data/TerrainRuleDefinition.h"
#include "Data/TerrainTypeDefinition.h"
#include "Rules/GrowthRules.h"
#include "Rules/MountainRules.h"
#include "Rules/ObjectRules.h"
#include "Rules/WaterRules.h"
#include "State/HearthlandsSession.h"

#include <vector>

namespace Hydro
{
	// The world's time on the Terrain entity: runs the ticks due, each settling rock, snow and rain on changed heights, moving
	// the water (meltwater from the mountains), growing the land and its objects; now and then it unlocks brushes.
	HY_SCRIPT()
	class SimulationScript : public ScriptableEntity
	{
	public:
		static constexpr uint64_t DefaultTerrainTypes = 4392505292887402202ull; // Assets/Data/TerrainTypes.hytable
		static constexpr uint64_t DefaultTerrainRules = 9052849889970328576ull; // Assets/Data/TerrainRules.hytable
		static constexpr uint64_t DefaultObjectTypes = 9091122517145490043ull;  // Assets/Data/Objects.hytable

		HY_PROPERTY(Asset(DataTable, TerrainTypeDefinition), Tooltip("Roles (water, riverbed) and unlocks"))
		AssetHandle TerrainTypes = DefaultTerrainTypes;
		HY_PROPERTY(Asset(DataTable, TerrainRuleDefinition), Tooltip("How land changes by itself"))
		AssetHandle TerrainRules = DefaultTerrainRules;
		HY_PROPERTY(Asset(DataTable, ObjectDefinition), Tooltip("What grows and stands on the tiles"))
		AssetHandle ObjectTypes = DefaultObjectTypes;
		HY_PROPERTY(Range(1, 20), Tooltip("Ticks per world second (at 1x)"))
		float TicksPerSecond = 4.0f;
		HY_PROPERTY(Range(0, 200000), Tooltip("Random tiles the terrain rules look at per tick"))
		int SamplesPerTick = 20000;
		HY_PROPERTY(Range(1, 200), Tooltip("How far a one-tile-wide channel carries water from the sea, in tiles"))
		float SourceHead = 24.0f;
		HY_PROPERTY(Range(0.1, 10), Tooltip("Head lost per tile in a narrow channel; wide ones lose less"))
		float Loss = 1.0f;
		HY_PROPERTY(Range(1, 32), Tooltip("Tiles over which moisture fades from fresh water"))
		int MoistureReach = 6;
		HY_PROPERTY(Range(1, 200), Tooltip("How far a one-tile-wide channel carries meltwater from a mountain's foot, in tiles"))
		float MeltHead = 24.0f;
		HY_PROPERTY(Range(1, 64), Tooltip("Tiles of raised land below the snow that meltwater runs down (and wets)"))
		int MeltReach = 16;
		HY_PROPERTY(Range(0, 1), Tooltip("Moisture (0 to 1) meltwater gives the land right below the snow, fading over MeltReach"))
		float MeltMoisture = 0.7f;
		HY_PROPERTY(Range(0, 0.25), Tooltip("Moisture each level gives all raised land (rain); 0: only snow and rivers wet hills"))
		float RainPerLevel = 0.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("Groundwater (0 to 1) from which a dug riverbed springs"))
		float SpringFrom = 0.78f;
		HY_PROPERTY(Range(1, 200), Tooltip("A spring's head at full groundwater (tiles of narrow channel it fills); weaker ones less"))
		float SpringHead = 20.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("Moisture full groundwater gives the land above it"))
		float GroundwaterMoisture = 0.12f;

	protected:
		void OnCreate() override
		{
			const DataTableView<TerrainTypeDefinition> types = Data::GetTable<TerrainTypeDefinition>(TerrainTypes);
			for (const auto& [id, type] : types.Rows())
			{
				m_Types.push_back(type);
			}
			const DataTableView<TerrainRuleDefinition> rules = Data::GetTable<TerrainRuleDefinition>(TerrainRules);
			for (const auto& [id, rule] : rules.Rows())
			{
				m_Rules.push_back(rule);
			}
			const DataTableView<ObjectDefinition> objects = Data::GetTable<ObjectDefinition>(ObjectTypes);
			for (const auto& [id, object] : objects.Rows())
			{
				m_Objects.push_back(object);
			}
			m_ObjectsById = ObjectRules::IndexById(m_Objects);
			WaterRules::Settings settings;
			settings.SourceHead = SourceHead;
			settings.Loss = Loss;
			settings.MoistureReach = MoistureReach;
			settings.MeltHead = MeltHead;
			settings.RainPerLevel = RainPerLevel;
			settings.MeltMoisture = MeltMoisture;
			settings.Groundwater.SpringFrom = SpringFrom;
			settings.Groundwater.SpringHead = SpringHead;
			settings.Groundwater.MinSpringHead = SpringHead * 0.3f;
			settings.Groundwater.Moisture = GroundwaterMoisture;
			m_Settings = WaterRules::FromTypes(m_Types, settings);
			MountainRules::Settings mountains;
			mountains.MeltReach = MeltReach;
			m_Mountains = MountainRules::FromTypes(m_Types, mountains);
		}

		void OnUpdate(float deltaTime) override
		{
			HearthlandsSession& session = HearthlandsSession::Get();
			if (session.Map.GetWidth() == 0)
			{
				return;
			}
			if (session.Heights.GetLevels().size() != session.Map.GetTiles().size())
			{
				session.Heights.Replace(HeightMap(session.Map.GetWidth(), session.Map.GetHeight())); // flat, if nothing made any
			}
			if (session.Water.Head.size() != session.Map.GetTiles().size())
			{
				MountainRules::Settle(session.Map, session.Heights, m_Mountains); // rock and snow on a generated world's hills
				session.Water = WaterRules::Start(session.Map, session.Land, m_Settings, &session.Heights, &session.Groundwater);
				WaterRules::UpdateMelt(session.Map, session.Water, MountainRules::Meltwater(session.Map, session.Heights, m_Mountains));
				session.Map.TakeChanges(); // the new map is the start, not a change
				session.Heights.TakeChanges();
				session.Objects.Reset(session.Map.GetTiles().size());
				ObjectRules::Scatter(session.Objects, session.Map, session.Water, m_Objects, session.Rng, &session.Heights);
			}

			SimulationClock& clock = session.Clock;
			clock.Seconds += (double)deltaTime * (double)clock.Speed;
			const int run = GrowthRules::TicksDue(clock.Seconds, clock.Ticks, TicksPerSecond, MaxTicksPerFrame);
			const float visitsPerDay = (float)SamplesPerTick * TicksPerSecond * (float)SimulationClock::SecondsPerDay
				/ (float)session.Map.GetTiles().size();
			for (int i = 0; i < run; i++)
			{
				const std::vector<uint32_t> raised = session.Heights.TakeChanges(); // the brush's, since the last tick
				if (raised.empty() == false)
				{
					MountainRules::SettleTiles(session.Map, session.Heights, raised, m_Mountains); // the brush settles too: a no-op then
					WaterRules::UpdateRain(session.Map, session.Water, session.Heights, raised, m_Settings);
					session.MountainsChanged = true;
				}
				const WaterRules::TickReport water = WaterRules::Step(session.Map, session.Water, session.Land, m_Settings);
				ObjectRules::RemoveUnfit(session.Objects, session.Map, session.Water, m_Objects, m_ObjectsById, water.Changes);
				if (session.MountainsChanged && (clock.Ticks + (uint64_t)i) % TicksPerSettle == 0)
				{
					// Meltwater looks at the whole map: at most every other tick. Its changes wake the water for the next tick.
					WaterRules::UpdateMelt(session.Map, session.Water, MountainRules::Meltwater(session.Map, session.Heights, m_Mountains));
					session.MountainsChanged = false;
				}
				GrowthRules::Step(session.Map, session.Water, session.Land, m_Rules, SamplesPerTick, session.Rng);
				ObjectRules::Step(session.Objects, session.Map, session.Water, session.Land, m_Objects, m_ObjectsById, SamplesPerTick,
					visitsPerDay, session.Rng, &session.Heights);
				if ((clock.Ticks + (uint64_t)i) % 4 == 0)
				{
					Unlock(session);
				}
			}
			// Ticks beyond the cap are dropped, not owed: a slow frame slows the world instead of stalling it.
			clock.Ticks = (uint64_t)(clock.Seconds * (double)TicksPerSecond);
		}

	private:
		static constexpr int MaxTicksPerFrame = 8;
		static constexpr int TicksPerSettle = 2; // meltwater looks at the whole map: at most every other tick

		void Unlock(HearthlandsSession& session) const
		{
			for (uint8_t tileId : GrowthRules::NewUnlocks(m_Types, session.UnlockedTiles, session.Map))
			{
				session.UnlockedTiles.push_back(tileId); // after the others, so the keys of the earlier brushes stay
				HY_INFO("Unlocked brush: tile {}", (int)tileId);
			}
		}

		std::vector<TerrainTypeDefinition> m_Types;
		std::vector<TerrainRuleDefinition> m_Rules;
		std::vector<ObjectDefinition> m_Objects;
		std::vector<int> m_ObjectsById;
		WaterRules::Settings m_Settings;
		MountainRules::Settings m_Mountains;
	};
}
