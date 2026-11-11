#pragma once

#include "Data/ObjectDefinition.h"
#include "State/HeightMap.h"
#include "State/Hydrology.h"
#include "State/ObjectLayer.h"
#include "State/Random.h"
#include "State/TileMap.h"

#include <cstdint>
#include <vector>

namespace Hydro
{
	// The object layer's life: objects sprout on tiles that suit them (ObjectDefinition), spread to their neighbours, grow
	// through their stages and die where they no longer fit. Like GrowthRules it looks at a random sample of tiles a tick.
	class ObjectRules
	{
	public:
		struct TickReport
		{
			int Sprouted = 0;
			int Grew = 0;
			int Died = 0;
		};

		// The row index of each object id (-1 for none), so rules look a row up by id.
		static std::vector<int> IndexById(const std::vector<ObjectDefinition>& objects)
		{
			std::vector<int> byId(256, -1);
			for (size_t i = 0; i < objects.size(); i++)
			{
				if (objects[i].ObjectId > 0 && objects[i].ObjectId <= 255)
				{
					byId[(size_t)objects[i].ObjectId] = (int)i;
				}
			}
			return byId;
		}

		// Whether an object can live on the tile (x, y): its tile, its height (heights: nullptr flat), its moisture, its
		// neighbouring tiles.
		static bool Fits(const ObjectDefinition& object, const TileMap& map, const Hydrology& water, int x, int y,
			const HeightMap* heights = nullptr)
		{
			const int level = heights != nullptr ? heights->Get(x, y) : 0;
			if (level < object.MinLevel || level > object.MaxLevel)
			{
				return false;
			}
			const size_t index = (size_t)y * (size_t)map.GetWidth() + (size_t)x;
			const uint8_t tile = map.GetTiles()[index];
			const bool anyLand = object.GrowsOnTileId < 0 && tile != water.WaterTileId && tile != water.BedTileId;
			if (anyLand == false && (int)tile != object.GrowsOnTileId)
			{
				return false;
			}
			const float moisture = index < water.Moisture.size() ? (float)water.Moisture[index] / 255.0f : 0.0f;
			if (moisture < object.MinMoisture - 1e-4f || moisture > object.MaxMoisture + 1e-4f)
			{
				return false;
			}
			return object.NeighbourTileId < 0 || CountTiles(map, x, y, (uint8_t)object.NeighbourTileId) >= object.MinNeighbours;
		}

		// Places objects with a StartDensity on the tiles that suit them (rocks), for a new world.
		static int Scatter(ObjectLayer& layer, const TileMap& map, const Hydrology& water, const std::vector<ObjectDefinition>& objects,
			Random& random, const HeightMap* heights = nullptr)
		{
			int placed = 0;
			for (int y = 0; y < map.GetHeight(); y++)
			{
				for (int x = 0; x < map.GetWidth(); x++)
				{
					const size_t index = (size_t)y * (size_t)map.GetWidth() + (size_t)x;
					for (const ObjectDefinition& object : objects)
					{
						if (object.StartDensity > 0.0f && layer.Get(index) == 0 && Fits(object, map, water, x, y, heights)
							&& random.Float() < object.StartDensity)
						{
							layer.Set(index, (uint8_t)object.ObjectId);
							placed++;
						}
					}
				}
			}
			return placed;
		}

		// Looks at `samples` random land tiles: empty ones may sprout or be spread to (HeightFactor higher up), objects grow a
		// stage (1 / (GrowDays x visitsPerDay) a look, so GrowDays holds on average) or die where they no longer fit.
		static TickReport Step(ObjectLayer& layer, const TileMap& map, const Hydrology& water, const std::vector<uint8_t>& land,
			const std::vector<ObjectDefinition>& objects, const std::vector<int>& byId, int samples, float visitsPerDay,
			Random& random, const HeightMap* heights = nullptr)
		{
			TickReport report;
			const uint32_t count = (uint32_t)map.GetTiles().size();
			for (int i = 0; i < samples && count > 0; i++)
			{
				const uint32_t index = random.Below(count);
				if (land.empty() == false && land[index] == 0)
				{
					continue;
				}
				const int x = (int)(index % (uint32_t)map.GetWidth());
				const int y = (int)(index / (uint32_t)map.GetWidth());
				const uint8_t current = layer.Get(index);
				if (current == 0)
				{
					for (const ObjectDefinition& object : objects)
					{
						const bool active = object.SproutChance > 0.0f || object.SpreadChance > 0.0f;
						if (active == false || Fits(object, map, water, x, y, heights) == false
							|| Crowded(object, layer, map, objects, byId, x, y))
						{
							continue;
						}
						float chance = object.SproutChance;
						const int reach = object.MinSpacing + 1;
						if (object.SpreadFromObjectId > 0 && CountObjects(layer, map, x, y, (uint8_t)object.SpreadFromObjectId, reach) > 0)
						{
							chance += object.SpreadChance;
						}
						chance *= HeightFactor(object, heights != nullptr ? heights->Get(x, y) : 0);
						if (chance > 0.0f && random.Float() < chance)
						{
							layer.Set(index, (uint8_t)object.ObjectId);
							report.Sprouted++;
							break;
						}
					}
					continue;
				}

				const int row = byId[current];
				if (row < 0 || Fits(objects[(size_t)row], map, water, x, y, heights) == false)
				{
					layer.Set(index, 0);
					report.Died++;
					continue;
				}
				const ObjectDefinition& object = objects[(size_t)row];
				const bool grows = object.GrowsIntoObjectId > 0 && object.GrowDays > 0.0f && visitsPerDay > 0.0f;
				if (grows && random.Float() < 1.0f / (object.GrowDays * visitsPerDay))
				{
					layer.Set(index, (uint8_t)object.GrowsIntoObjectId);
					report.Grew++;
				}
			}
			return report;
		}

		// Removes, at once, the objects on changed tiles (TileMap::TakeChanges) whose tile they can no longer stand on (a
		// bush dug away, a tree under a new river). Moisture is left to Step, so a drying tile loses its plants slowly.
		static int RemoveUnfit(ObjectLayer& layer, const TileMap& map, const Hydrology& water, const std::vector<ObjectDefinition>& objects,
			const std::vector<int>& byId, const std::vector<uint32_t>& changed)
		{
			int removed = 0;
			for (uint32_t index : changed)
			{
				const uint8_t current = layer.Get(index);
				if (current == 0)
				{
					continue;
				}
				const int row = byId[current];
				const uint8_t tile = map.GetTiles()[index];
				const bool fits = row >= 0
					&& (objects[(size_t)row].GrowsOnTileId < 0 ? tile != water.WaterTileId && tile != water.BedTileId
															   : (int)tile == objects[(size_t)row].GrowsOnTileId);
				if (fits == false)
				{
					layer.Set(index, 0);
					removed++;
				}
			}
			return removed;
		}

		// How much likelier the object sprouts and spreads at this level: 1 + ChancePerLevel x level.
		static float HeightFactor(const ObjectDefinition& object, int level)
		{
			const float factor = 1.0f + object.ChancePerLevel * (float)level;
			return factor > 0.0f ? factor : 0.0f;
		}

		static int CountTiles(const TileMap& map, int x, int y, uint8_t tileId)
		{
			int found = 0;
			for (int oy = -1; oy <= 1; oy++)
			{
				for (int ox = -1; ox <= 1; ox++)
				{
					if ((ox != 0 || oy != 0) && map.InBounds(x + ox, y + oy) && map.Get(x + ox, y + oy) == tileId)
					{
						found++;
					}
				}
			}
			return found;
		}

		// Whether an object of its kind or bigger stands within MinSpacing tiles of (x, y), not counting the tile itself.
		static bool Crowded(const ObjectDefinition& object, const ObjectLayer& layer, const TileMap& map,
			const std::vector<ObjectDefinition>& objects, const std::vector<int>& byId, int x, int y)
		{
			for (int oy = -object.MinSpacing; oy <= object.MinSpacing; oy++)
			{
				for (int ox = -object.MinSpacing; ox <= object.MinSpacing; ox++)
				{
					if ((ox == 0 && oy == 0) || map.InBounds(x + ox, y + oy) == false)
					{
						continue;
					}
					const uint8_t id = layer.Get((size_t)(y + oy) * (size_t)map.GetWidth() + (size_t)(x + ox));
					const int row = id != 0 ? byId[id] : -1;
					if (row >= 0 && (uint8_t)objects[(size_t)row].Kind >= (uint8_t)object.Kind)
					{
						return true;
					}
				}
			}
			return false;
		}

		// Objects within `radius` tiles of (x, y), not counting it: those with this id, or (objectId 0) any.
		static int CountObjects(const ObjectLayer& layer, const TileMap& map, int x, int y, uint8_t objectId, int radius)
		{
			int found = 0;
			for (int oy = -radius; oy <= radius; oy++)
			{
				for (int ox = -radius; ox <= radius; ox++)
				{
					if ((ox == 0 && oy == 0) || map.InBounds(x + ox, y + oy) == false)
					{
						continue;
					}
					const uint8_t id = layer.Get((size_t)(y + oy) * (size_t)map.GetWidth() + (size_t)(x + ox));
					found += (objectId == 0 ? id != 0 : id == objectId) ? 1 : 0;
				}
			}
			return found;
		}
	};
}
