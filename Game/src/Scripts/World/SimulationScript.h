#pragma once

#include "Hydro.h"
#include "Data/TerrainRuleDefinition.h"
#include "Data/TerrainTypeDefinition.h"
#include "Rules/GrowthRules.h"
#include "Rules/WaterRules.h"
#include "State/HearthlandsSession.h"

#include <vector>

namespace Hydro
{
	// The world's time, on the Terrain entity: advances the clock by its speed and runs the ticks due, each moving the water
	// (WaterRules) and growing the land (GrowthRules, rows from TerrainRules); now and then it unlocks new brushes.
	HY_SCRIPT()
	class SimulationScript : public ScriptableEntity
	{
	public:
		static constexpr uint64_t DefaultTerrainTypes = 4392505292887402202ull; // Assets/Data/TerrainTypes.hytable
		static constexpr uint64_t DefaultTerrainRules = 9052849889970328576ull; // Assets/Data/TerrainRules.hytable

		HY_PROPERTY(Asset(DataTable, TerrainTypeDefinition), Tooltip("Roles (water, riverbed) and unlocks"))
		AssetHandle TerrainTypes = DefaultTerrainTypes;
		HY_PROPERTY(Asset(DataTable, TerrainRuleDefinition), Tooltip("How land changes by itself"))
		AssetHandle TerrainRules = DefaultTerrainRules;
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
			WaterRules::Settings settings;
			settings.SourceHead = SourceHead;
			settings.Loss = Loss;
			settings.MoistureReach = MoistureReach;
			m_Settings = WaterRules::FromTypes(m_Types, settings);
		}

		void OnUpdate(float deltaTime) override
		{
			HearthlandsSession& session = HearthlandsSession::Get();
			if (session.Map.GetWidth() == 0)
			{
				return;
			}
			if (session.Water.Head.size() != session.Map.GetTiles().size())
			{
				session.Water = WaterRules::Start(session.Map, session.Land, m_Settings);
				session.Map.TakeChanges(); // the new map is the start, not a change
			}

			SimulationClock& clock = session.Clock;
			clock.Seconds += (double)deltaTime * (double)clock.Speed;
			const int run = GrowthRules::TicksDue(clock.Seconds, clock.Ticks, TicksPerSecond, MaxTicksPerFrame);
			for (int i = 0; i < run; i++)
			{
				WaterRules::Step(session.Map, session.Water, session.Land, m_Settings);
				GrowthRules::Step(session.Map, session.Water, session.Land, m_Rules, SamplesPerTick, session.Rng);
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
		WaterRules::Settings m_Settings;
	};
}
