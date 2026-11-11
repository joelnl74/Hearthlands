#pragma once

#include "Hydro.h"
#include "Data/ObjectDefinition.h"
#include "Data/TerrainTypeDefinition.h"
#include "Rules/BrushRules.h"
#include "Rules/GroundwaterRules.h"
#include "Rules/HeightRules.h"
#include "Rules/MountainRules.h"
#include "Rules/TerrainGenRules.h"
#include "State/HearthlandsSession.h"
#include "Views/ObjectsView.h"
#include "Views/ReliefView.h"
#include "Views/TerrainView.h"

#include <vector>

namespace Hydro
{
	// The world scene's director on the Terrain entity (a Plane running tile_blend.frag): makes the run's map and heights when
	// there are none, then keeps them drawn: the ground (moisture overlay), the raised land as stepped blocks, the objects.
	HY_SCRIPT()
	class WorldScript : public ScriptableEntity
	{
	public:
		static constexpr uint64_t DefaultTerrainTypes = 4392505292887402202ull; // Assets/Data/TerrainTypes.hytable
		static constexpr uint64_t DefaultTerrainArt = 4186076384102379045ull;   // Assets/Textures/TerrainAtlas.png
		static constexpr uint64_t DefaultObjectTypes = 9091122517145490043ull;  // Assets/Data/Objects.hytable

		HY_PROPERTY(Asset(DataTable, TerrainTypeDefinition), Tooltip("The terrain types generation picks from and the shader draws"))
		AssetHandle TerrainTypes = DefaultTerrainTypes;
		HY_PROPERTY(Asset(DataTable, ObjectDefinition), Tooltip("The objects' sprites and sizes"))
		AssetHandle ObjectTypes = DefaultObjectTypes;
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
			m_View.Present(map, m_Types, (uint8_t)OutsideTileId, TerrainArt, nullptr, false, 0.0f, &session.Heights);
		}

		void OnUpdate(float deltaTime) override
		{
			const HearthlandsSession& session = HearthlandsSession::Get();
			UpdateClues(session);
			m_View.Present(session.Map, m_Types, (uint8_t)OutsideTileId, TerrainArt, &session.Water.Moisture, session.ShowMoisture,
				deltaTime, &session.Heights, &m_Clues, m_CluesRevision);
			m_Relief.Present(session.Heights, session.Map, GetWorldPosition(), session.LevelHeight, deltaTime);
			m_Objects.Present(session.Objects, session.Map.GetWidth(), session.Map.GetHeight(), GetWorldPosition(), deltaTime,
				&session.Heights, session.LevelHeight);
		}

		void OnDestroy() override
		{
			m_View.Release();
			m_Objects.Release();
			m_Relief.Release();
		}

	private:
		// The groundwater the surface shows (GroundwaterRules::Clue), again when it or the clue settings change.
		void UpdateClues(const HearthlandsSession& session)
		{
			const bool same = m_Clues.size() == session.Groundwater.size() && session.ClueClarity == m_ClueClarity
				&& session.ClueFrom == m_ClueFrom;
			if (same)
			{
				return;
			}
			m_Clues.resize(session.Groundwater.size());
			for (size_t i = 0; i < m_Clues.size(); i++)
			{
				m_Clues[i] = GroundwaterRules::Clue(session.Groundwater[i], session.ClueClarity, session.ClueFrom);
			}
			m_ClueClarity = session.ClueClarity;
			m_ClueFrom = session.ClueFrom;
			m_CluesRevision++;
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
		ReliefView m_Relief;
		std::vector<uint8_t> m_Clues; // per tile 0 to 63 (UpdateClues)
		float m_ClueClarity = -1.0f;
		float m_ClueFrom = -1.0f;
		uint64_t m_CluesRevision = 0;
	};
}
