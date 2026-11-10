#pragma once

#include "Hydro.h"
#include "Data/ObjectDefinition.h"
#include "Data/TerrainTypeDefinition.h"
#include "Rules/BrushRules.h"
#include "Rules/TerrainGenRules.h"
#include "State/HearthlandsSession.h"
#include "Views/ObjectsView.h"
#include "Views/TerrainView.h"

#include <vector>

namespace Hydro
{
	// The world scene's director, on the Terrain entity (a Plane running tile_blend.frag): makes the run's map when there is
	// none (one base type or generated, then coast, beach, land, brushes), then keeps it drawn (with the moisture overlay).
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
		HY_PROPERTY(Asset(Texture2D), Tooltip("One 16 x 16 tile per terrain id, in id order; none draws flat colours"))
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
			m_View.Present(map, m_Types, (uint8_t)OutsideTileId, TerrainArt, nullptr, 0.0f);
		}

		void OnUpdate(float deltaTime) override
		{
			const HearthlandsSession& session = HearthlandsSession::Get();
			const std::vector<uint8_t>* moisture = session.ShowMoisture ? &session.Water.Moisture : nullptr;
			m_View.Present(session.Map, m_Types, (uint8_t)OutsideTileId, TerrainArt, moisture, deltaTime);
			m_Objects.Present(session.Objects, session.Map.GetWidth(), session.Map.GetHeight(), GetWorldPosition(), deltaTime);
		}

		void OnDestroy() override
		{
			m_View.Release();
			m_Objects.Release();
		}

	private:
		std::vector<TerrainTypeDefinition> m_Types;
		TerrainView m_View;
		ObjectsView m_Objects;
	};
}
