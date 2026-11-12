#pragma once

#include "Hydro.h"
#include "Data/AnimalDefinition.h"
#include "Data/ObjectDefinition.h"
#include "Data/PeopleDefinition.h"
#include "Data/TerrainTypeDefinition.h"
#include "Rules/BrushRules.h"
#include "Rules/CreekRules.h"
#include "Rules/GroundwaterRules.h"
#include "Rules/HeightRules.h"
#include "Rules/MountainRules.h"
#include "Rules/SoilRules.h"
#include "Rules/TerrainGenRules.h"
#include "State/HearthlandsSession.h"
#include "Views/AnimalsView.h"
#include "Views/ObjectsView.h"
#include "Views/ReliefView.h"
#include "Views/SettlersView.h"
#include "Views/TerrainView.h"

#include <vector>

namespace Hydro
{
	// The world scene's director on the Terrain entity (a Plane running tile_blend.frag): makes the run's map and heights when
	// there are none, then keeps them drawn: the ground (overlays), the raised land, the objects, the animals and settlers.
	HY_SCRIPT()
	class WorldScript : public ScriptableEntity
	{
	public:
		static constexpr uint64_t DefaultTerrainTypes = 4392505292887402202ull; // Assets/Data/TerrainTypes.hytable
		static constexpr uint64_t DefaultTerrainArt = 4186076384102379045ull;   // Assets/Textures/TerrainAtlas.png
		static constexpr uint64_t DefaultObjectTypes = 9091122517145490043ull;  // Assets/Data/Objects.hytable
		static constexpr uint64_t DefaultAnimals = 8152222171692113247ull;      // Assets/Data/Animals.hytable
		static constexpr uint64_t DefaultPeoples = 3987286432867960408ull;      // Assets/Data/Peoples.hytable

		HY_PROPERTY(Asset(DataTable, TerrainTypeDefinition), Tooltip("The terrain types generation picks from and the shader draws"))
		AssetHandle TerrainTypes = DefaultTerrainTypes;
		HY_PROPERTY(Asset(DataTable, ObjectDefinition), Tooltip("The objects' sprites and sizes"))
		AssetHandle ObjectTypes = DefaultObjectTypes;
		HY_PROPERTY(Asset(DataTable, AnimalDefinition), Tooltip("The wild animals' looks (SimulationScript keeps their numbers)"))
		AssetHandle Animals = DefaultAnimals;
		HY_PROPERTY(Asset(DataTable, PeopleDefinition), Tooltip("The settlers' looks (SimulationScript keeps their settlements)"))
		AssetHandle Peoples = DefaultPeoples;
		HY_PROPERTY(Asset(Texture2D), Tooltip("The terrain atlas (scripts/art/make_terrain_atlas.py); none draws flat colours"))
		AssetHandle TerrainArt = DefaultTerrainArt;
		HY_PROPERTY(Range(-1, 255), Tooltip("Every tile starts as this terrain id (1 = dirt); -1 generates the world from Seed"))
		int BaseTileId = 1;
		HY_PROPERTY(Range(0, 1000000), Tooltip("The same seed makes the same world"))
		int Seed = 1;
		HY_PROPERTY(Range(8, 1024), Tooltip("Tiles across"))
		int Width = 512;
		HY_PROPERTY(Range(8, 1024), Tooltip("Tiles down"))
		int Height = 512;
		HY_PROPERTY(Range(0, 255), Tooltip("The tile id drawn all around the map (3 = water)"))
		int OutsideTileId = 3;
		HY_PROPERTY(Range(0, 128), Tooltip("About how many tiles of that sea round off the map's edge; 0 keeps it square"))
		float CoastWidth = 24.0f;
		HY_PROPERTY(Range(0, 255), Tooltip("The tile id of the beach along the coast (0 = sand)"))
		int BeachTileId = 0;
		HY_PROPERTY(Range(0, 32), Tooltip("About how many tiles of beach line the coast; 0 for none"))
		float BeachWidth = 3.0f;
		HY_PROPERTY(Range(0.05, 2), Tooltip("Metres a level of height stands: 8 art pixels at 32 a tile from 30 degrees = 0.2887"))
		float LevelHeight = 0.2887f;
		HY_PROPERTY(Range(0, 1), Tooltip("Generated worlds: the height noise above which the land rises"))
		float HillsFrom = 0.55f;
		HY_PROPERTY(Range(1, 100), Tooltip("Generated worlds: levels per unit of height noise above HillsFrom"))
		float LevelsPerHeight = 28.0f;
		HY_PROPERTY(Range(0, 10), Tooltip("Dry creeks a new world gets per 100 x 100 tiles of land; digging a head opens its spring"))
		float Creeks = 1.2f;
		HY_PROPERTY(Range(4, 256), Tooltip("Tiles across a patch of groundwater"))
		float GroundwaterScale = 40.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("How plainly the surface shows groundwater at the start: 0 not, 1 plainly"))
		float ClueClarity = 0.25f;
		HY_PROPERTY(Range(0, 1), Tooltip("Groundwater (0 to 1) from which the surface shows it at the start"))
		float ClueFrom = 0.78f;

	protected:
		void OnCreate() override
		{
			const DataTableView<TerrainTypeDefinition> table = Data::GetTable<TerrainTypeDefinition>(TerrainTypes);
			for (const auto& [id, type] : table.Rows())
			{
				m_Types.push_back(type);
			}
			if (m_Types.empty())
			{
				HY_WARN("WorldScript: no terrain types (TerrainTypes table missing or empty)");
			}

			HearthlandsSession& session = HearthlandsSession::Get();
			TileMap& map = session.Map;
			session.LevelHeight = LevelHeight;
			const bool made = map.GetWidth() == 0;
			if (made && BaseTileId >= 0)
			{
				map.Replace(TileMap(Width, Height, (uint8_t)(BaseTileId > 255 ? 255 : BaseTileId)));
				HY_INFO("WorldScript: made a {} x {} world of tile {}", Width, Height, BaseTileId);
			}
			else if (made)
			{
				TerrainGenRules::Settings settings;
				settings.Seed = (uint32_t)Seed;
				settings.Width = Width;
				settings.Height = Height;
				map.Replace(TerrainGenRules::Generate(settings, m_Types));
				HY_INFO("WorldScript: generated a {} x {} world from seed {}", Width, Height, Seed);
			}
			if (made)
			{
				const uint8_t sea = (uint8_t)OutsideTileId;
				const TileMap coast = TerrainGenRules::ShapeCoast(map, CoastWidth, (uint32_t)Seed, sea);
				map.Replace(TerrainGenRules::AddBeach(coast, BeachWidth, (uint32_t)Seed, sea, (uint8_t)BeachTileId));
				session.Land = TerrainGenRules::LandOf(map, sea);
				session.Rng = Random((uint64_t)Seed * 2654435761ull + 1);
				MakeHeights(session, BaseTileId < 0);
				session.ClueClarity = ClueClarity;
				session.ClueFrom = ClueFrom;
			}
			if (session.Groundwater.size() != map.GetTiles().size())
			{
				GroundwaterRules::Settings groundwater;
				groundwater.Scale = GroundwaterScale;
				session.Groundwater = GroundwaterRules::Generate(map.GetWidth(), map.GetHeight(), (uint32_t)Seed, groundwater);
			}
			if (made && Creeks > 0.0f)
			{
				CreekRules::Settings creeks;
				creeks.PerTenThousand = Creeks;
				Random random((uint64_t)Seed * 40503ull + 7);
				const std::vector<CreekRules::Creek> carved = CreekRules::Carve(map, session.Land, &session.Heights,
					session.Groundwater, CreekRules::FromTypes(m_Types, creeks), random);
				session.DryCreeks.clear();
				for (const CreekRules::Creek& creek : carved)
				{
					session.DryCreeks.insert(session.DryCreeks.end(), creek.Bed.begin(), creek.Bed.end());
				}
				HY_INFO("WorldScript: {} dry creeks (dig a head to open its spring)", carved.size());
			}
			if (session.UnlockedTiles.empty())
			{
				session.UnlockedTiles = BrushRules::StartingBrushes(m_Types);
			}

			m_View.Bind(GetEntity());
			std::vector<ObjectDefinition> objects;
			const DataTableView<ObjectDefinition> objectTable = Data::GetTable<ObjectDefinition>(ObjectTypes);
			for (const auto& [id, object] : objectTable.Rows())
			{
				objects.push_back(object);
			}
			m_Objects.Bind(*GetEntity().GetScene(), objects);
			m_Relief.Bind(*GetEntity().GetScene(), GetEntity(), m_Types);
			std::vector<AnimalDefinition> animals;
			const DataTableView<AnimalDefinition> animalTable = Data::GetTable<AnimalDefinition>(Animals);
			for (const auto& [id, animal] : animalTable.Rows())
			{
				animals.push_back(animal);
			}
			m_Animals.Bind(*GetEntity().GetScene(), animals);
			std::vector<PeopleDefinition> peoples;
			const DataTableView<PeopleDefinition> peopleTable = Data::GetTable<PeopleDefinition>(Peoples);
			for (const auto& [id, people] : peopleTable.Rows())
			{
				peoples.push_back(people);
			}
			m_Settlers.Bind(*GetEntity().GetScene(), peoples, objects);
			m_View.Present(map, m_Types, (uint8_t)OutsideTileId, TerrainArt, nullptr, false, 0.0f, &session.Heights);
		}

		void OnUpdate(float deltaTime) override
		{
			HearthlandsSession& writable = HearthlandsSession::Get();
			GameTimings& timings = writable.Timings;
			const HearthlandsSession& session = writable;
			UpdateClues(session);
			const bool deep = session.ShowGroundwater && session.Groundwater.size() == session.Map.GetTiles().size();
			const bool soil = deep == false && session.ShowSoil && UpdateSoilView(session, deltaTime);
			const std::vector<uint8_t>* shown = deep ? &session.Groundwater : (soil ? &m_SoilView : &session.Water.Moisture);
			const SeasonState& season = session.Season;
			{
				const TimedScope timed(timings, TimedPart::TerrainView);
				m_View.Present(session.Map, m_Types, (uint8_t)OutsideTileId, TerrainArt, shown, session.ShowMoisture, deltaTime,
					&session.Heights, &m_Clues, m_CluesRevision, soil, deep);
				m_View.SetSeason(season.Tint, season.GroundSnow, season.Ice, season.Fade);
			}
			{
				const TimedScope timed(timings, TimedPart::ReliefView);
				m_Relief.Present(session.Heights, session.Map, GetWorldPosition(), session.LevelHeight, deltaTime, season.Tint);
			}
			{
				const TimedScope timed(timings, TimedPart::ObjectsView);
				m_Objects.Present(session.Objects, session.Map.GetWidth(), session.Map.GetHeight(), GetWorldPosition(), deltaTime,
					&session.Heights, session.LevelHeight, season.LeafTint);
			}
			AnimalsView::Inputs animals;
			animals.Wildlife = &session.Wildlife;
			animals.Map = &session.Map;
			animals.Water = &session.Water;
			animals.Heights = &session.Heights;
			animals.LevelHeight = session.LevelHeight;
			animals.Centre = GetWorldPosition();
			animals.DeltaTime = deltaTime;
			animals.Running = session.Clock.Speed > 0;
			animals.Burrowed = season.Burrowed;
			animals.Sight = session.Talents.Get(TalentEffect::AnimalSight);
			animals.Painting = session.Painting;
			animals.Pointer = session.PointerTile;
			animals.BrushRadius = session.Brush.Radius;
			{
				const TimedScope timed(timings, TimedPart::AnimalsView);
				m_Animals.Present(animals);
			}
			SettlersView::Inputs settlers;
			settlers.Settlements = &session.Settlements;
			settlers.Map = &session.Map;
			settlers.Water = &session.Water;
			settlers.Objects = &session.Objects;
			settlers.Heights = &session.Heights;
			settlers.LevelHeight = session.LevelHeight;
			settlers.Centre = GetWorldPosition();
			settlers.DeltaTime = deltaTime;
			settlers.Running = session.Clock.Speed > 0;
			{
				const TimedScope timed(timings, TimedPart::SettlersView);
				m_Settlers.Present(settlers);
			}
			const std::string line = timings.Frame(deltaTime);
			if (line.empty() == false)
			{
				HY_INFO("{}", line); // where the time goes: Oxygen's console, every few seconds
			}
		}

		void OnDestroy() override
		{
			m_View.Release();
			m_Objects.Release();
			m_Animals.Release();
			m_Settlers.Release();
			m_Relief.Release();
		}

	private:
		// The groundwater the surface shows (GroundwaterRules::Clue, the session's clue settings as talents move them), again
		// when it or they change.
		void UpdateClues(const HearthlandsSession& session)
		{
			const float clarity = session.ClueClarity + session.Talents.Get(TalentEffect::ClueClarity);
			const float from = session.ClueFrom - session.Talents.Get(TalentEffect::ClueFrom);
			const bool same = m_Clues.size() == session.Groundwater.size() && clarity == m_ClueClarity && from == m_ClueFrom;
			if (same)
			{
				return;
			}
			m_Clues.resize(session.Groundwater.size());
			for (size_t i = 0; i < m_Clues.size(); i++)
			{
				m_Clues[i] = GroundwaterRules::Clue(session.Groundwater[i], clarity, from);
			}
			m_ClueClarity = clarity;
			m_ClueFrom = from;
			m_CluesRevision++;
		}

		// The soil's quality per tile for the soil overlay, again at most twice a second while it changes. False without soil.
		bool UpdateSoilView(const HearthlandsSession& session, float deltaTime)
		{
			if (session.Soil.Fertility.size() != session.Map.GetTiles().size())
			{
				return false;
			}
			if (m_SoilTypes != m_Types.size())
			{
				m_SoilSettings = SoilRules::FromTypes(m_Types, SoilRules::Settings{});
				m_SoilTypes = m_Types.size();
			}
			m_SinceSoilView += deltaTime;
			const bool due = session.Soil.Revision != m_SoilRevision && m_SinceSoilView >= 0.5f;
			if (m_SoilView.size() != session.Soil.Fertility.size() || due)
			{
				SoilRules::Qualities(session.Soil, m_SoilSettings, m_SoilView);
				m_SoilRevision = session.Soil.Revision;
				m_SinceSoilView = 0.0f;
			}
			return true;
		}

		// The new map's heights: flat, or (generated) from the height noise on the land that isn't water or beach, slopes
		// slumped to HeightRules' MaxStep and rock and snow settled on them.
		void MakeHeights(HearthlandsSession& session, bool generated)
		{
			const TileMap& map = session.Map;
			if (generated == false)
			{
				session.Heights.Replace(HeightMap(map.GetWidth(), map.GetHeight()));
				return;
			}
			HeightRules::Settings slopes;
			std::vector<uint8_t> keep(session.Land);
			for (size_t i = 0; i < keep.size(); i++)
			{
				const uint8_t tile = map.GetTiles()[i];
				const bool low = (int)tile == OutsideTileId || (int)tile == BeachTileId || tile == slopes.WaterTileId;
				keep[i] = low ? (uint8_t)0 : keep[i];
			}
			TerrainGenRules::Settings settings;
			settings.Seed = (uint32_t)Seed;
			settings.Width = Width;
			settings.Height = Height;
			session.Heights.Replace(TerrainGenRules::Levels(settings, HillsFrom, LevelsPerHeight, slopes.MaxLevel, keep));
			std::vector<uint32_t> raised;
			for (uint32_t i = 0; i < (uint32_t)session.Heights.GetLevels().size(); i++)
			{
				if (session.Heights.GetLevels()[i] > 0)
				{
					raised.push_back(i);
				}
			}
			HeightRules::Slump(session.Heights, map, session.Land, raised, true, slopes);
			session.Heights.TakeChanges();
			MountainRules::Settle(session.Map, session.Heights, MountainRules::FromTypes(m_Types, MountainRules::Settings{}));
		}

		std::vector<TerrainTypeDefinition> m_Types;
		TerrainView m_View;
		ObjectsView m_Objects;
		AnimalsView m_Animals;
		SettlersView m_Settlers;
		ReliefView m_Relief;
		std::vector<uint8_t> m_Clues; // per tile 0 to 63 (UpdateClues)
		float m_ClueClarity = -1.0f;
		float m_ClueFrom = -1.0f;
		uint64_t m_CluesRevision = 0;
		SoilRules::Settings m_SoilSettings; // from the types (the soil overlay's quality)
		size_t m_SoilTypes = ~(size_t)0;
		std::vector<uint8_t> m_SoilView;    // per tile the soil's quality, 0 to 255
		uint64_t m_SoilRevision = ~0ull;
		float m_SinceSoilView = 0.0f;
	};
}
