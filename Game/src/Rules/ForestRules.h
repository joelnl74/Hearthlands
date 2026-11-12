#pragma once

#include "Data/ObjectDefinition.h"
#include "Data/TerrainTypeDefinition.h"
#include "State/Hydrology.h"
#include "State/ObjectLayer.h"
#include "State/TileMap.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// Forests away from rivers: plants' roots reach the hidden groundwater (Roots: painted grass holds over it, trees grow
	// on it), and a canopy keeps its trees' water in the living ground around it (Shade); bare ground only with a talent.
	class ForestRules
	{
	public:
		struct Settings
		{
			int Reach = 2;          // tiles around a tile whose trees shade it (2: the 5 x 5 tiles; trees stand 2 apart)
			float FullCover = 3.0f; // Canopy (summed over those tiles) that shades a tile fully
			float Keep = 0.9f;      // share of the trees' own moisture a fully shaded tile keeps
			std::array<uint8_t, 256> Living{}; // per tile id: 1 where the shade keeps the ground moist (grass)
			std::array<float, 256> Roots{};    // per tile id: share of the groundwater its plants reach (terrain rules)
			bool ShadeBare = false;            // the shade keeps bare ground (dirt) moist too: forests creep out (a talent)
			bool OldGrowth = false;            // trees may grow into Ancient stages (the Old woods talent) ...
			float AncientCanopy = 7.0f;        // ... where the Canopy within Reach adds up to this (inside closed woods)
		};

		// The types' living ground and roots (TerrainTypeDefinition Living, Roots) on top of `base`.
		static Settings FromTypes(const std::vector<TerrainTypeDefinition>& types, Settings base)
		{
			for (const TerrainTypeDefinition& type : types)
			{
				if (type.TileId >= 0 && type.TileId <= 255)
				{
					base.Living[(size_t)type.TileId] = type.Living ? 1 : 0;
					base.Roots[(size_t)type.TileId] = type.Roots < 0.0f ? 0.0f : (type.Roots > 1.0f ? 1.0f : type.Roots);
				}
			}
			return base;
		}

		// What shades the land: the object layer and its rows. Empty (no layer) shades nothing.
		struct Canopy
		{
			const ObjectLayer* Layer = nullptr;
			const std::vector<ObjectDefinition>* Objects = nullptr;
			const std::vector<int>* ById = nullptr;
			Settings Forest;

			bool IsEmpty() const
			{
				return Layer == nullptr || Objects == nullptr || ById == nullptr || Layer->GetIds().empty();
			}

			// How covered (x, y) is, 0 open to 1 under a closed canopy: the Canopy of the objects on it and around it.
			float Cover(const TileMap& map, int x, int y) const
			{
				return Share(Total(map, x, y));
			}

			// The Canopy of the objects on (x, y) and within Reach of it, added up.
			float Total(const TileMap& map, int x, int y) const
			{
				float canopy = 0.0f;
				float moisture = 0.0f;
				Sum(map, nullptr, x, y, canopy, moisture);
				return canopy;
			}

			// The moisture (0 to 1) the canopy keeps at (x, y): Keep x cover x the mean moisture of the trees shading it.
			float Shade(const TileMap& map, const Hydrology& water, int x, int y) const
			{
				float canopy = 0.0f;
				float moisture = 0.0f;
				Sum(map, &water, x, y, canopy, moisture);
				return canopy > 0.0f ? Forest.Keep * Share(canopy) * moisture / canopy : 0.0f;
			}

			// The moisture (0 to 1) the terrain rules see at (x, y) besides the ground's: what the tile type's roots draw
			// from the groundwater and, on living ground (or any with ShadeBare), the canopy's shade.
			float Ground(const TileMap& map, const Hydrology& water, int x, int y) const
			{
				const size_t index = (size_t)y * (size_t)map.GetWidth() + (size_t)x;
				const uint8_t tile = map.GetTiles()[index];
				const float deep = index < water.Groundwater.size() ? Forest.Roots[tile] * (float)water.Groundwater[index] / 255.0f
					: 0.0f;
				const float shade = Forest.Living[tile] != 0 || Forest.ShadeBare ? Shade(map, water, x, y) : 0.0f;
				return shade > deep ? shade : deep;
			}

		private:
			float Share(float canopy) const
			{
				const float cover = Forest.FullCover > 0.0f ? canopy / Forest.FullCover : 0.0f;
				return cover < 1.0f ? cover : 1.0f;
			}

			// The Canopy within Reach and (water given) the moisture of each tree weighted by it.
			void Sum(const TileMap& map, const Hydrology* water, int x, int y, float& canopy, float& moisture) const
			{
				if (IsEmpty())
				{
					return;
				}
				for (int oy = -Forest.Reach; oy <= Forest.Reach; oy++)
				{
					for (int ox = -Forest.Reach; ox <= Forest.Reach; ox++)
					{
						if (map.InBounds(x + ox, y + oy) == false)
						{
							continue;
						}
						const size_t index = (size_t)(y + oy) * (size_t)map.GetWidth() + (size_t)(x + ox);
						const uint8_t id = Layer->Get(index);
						const int row = id != 0 && id < ById->size() ? (*ById)[id] : -1;
						if (row < 0 || (*Objects)[(size_t)row].Canopy <= 0.0f)
						{
							continue;
						}
						const ObjectDefinition& object = (*Objects)[(size_t)row];
						canopy += object.Canopy;
						moisture += water != nullptr ? object.Canopy * Rooted(object, *water, index) : 0.0f;
					}
				}
			}
		};

		// The moisture (0 to 1) an object has at a tile: the ground's, or what its roots draw from the groundwater if more.
		static float Rooted(const ObjectDefinition& object, const Hydrology& water, size_t index)
		{
			const float surface = index < water.Moisture.size() ? (float)water.Moisture[index] / 255.0f : 0.0f;
			const float deep = index < water.Groundwater.size() ? object.Roots * (float)water.Groundwater[index] / 255.0f : 0.0f;
			return deep > surface ? deep : surface;
		}

		// How much likelier a plant sprouts under this cover (0 open, 1 closed): 1 + ShadeEffect x (2 x cover - 1), never
		// below 0. Shade-loving saplings (oaks, ferns) + come up under trees, pioneers (birch, pine) - in the open.
		static float ShadeFactor(float shadeEffect, float cover)
		{
			const float factor = 1.0f + shadeEffect * (2.0f * cover - 1.0f);
			return factor > 0.0f ? factor : 0.0f;
		}
	};
}
