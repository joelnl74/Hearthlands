#pragma once

#include "Data/TerrainRuleDefinition.h"
#include "Data/TerrainTypeDefinition.h"
#include "State/Hydrology.h"
#include "State/Random.h"
#include "State/TileMap.h"

#include <cstdint>
#include <vector>

namespace Hydro
{
	// The land changing by itself (TerrainRuleDefinition rows) and what that unlocks. Each tick looks at a random sample
	// of tiles, so growth creeps instead of sweeping; the rows are data, so new types bring their own behaviour.
	class GrowthRules
	{
	public:
		// Whether a row's conditions hold for the tile (x, y): its id, its moisture and its neighbours.
		static bool Matches(const TerrainRuleDefinition& rule, const TileMap& map, const Hydrology& water, int x, int y)
		{
			const size_t index = (size_t)y * (size_t)map.GetWidth() + (size_t)x;
			if ((int)map.GetTiles()[index] != rule.FromTileId)
			{
				return false;
			}
			const float moisture = index < water.Moisture.size() ? (float)water.Moisture[index] / 255.0f : 0.0f;
			if (moisture < rule.MinMoisture - 1e-4f || moisture > rule.MaxMoisture + 1e-4f)
			{
				return false;
			}
			return rule.NeighbourTileId < 0 || CountNeighbours(map, x, y, (uint8_t)rule.NeighbourTileId) >= rule.MinNeighbours;
		}

		// Looks at `samples` random tiles on the land (land[i] != 0; empty = all): each matching row rolls its Chance in
		// turn until one changes the tile. Returns how many tiles changed.
		static int Step(TileMap& map, const Hydrology& water, const std::vector<uint8_t>& land,
			const std::vector<TerrainRuleDefinition>& rules, int samples, Random& random)
		{
			const uint32_t count = (uint32_t)map.GetTiles().size();
			int changed = 0;
			for (int i = 0; i < samples && count > 0; i++)
			{
				const uint32_t index = random.Below(count);
				if (land.empty() == false && land[index] == 0)
				{
					continue;
				}
				const int x = (int)(index % (uint32_t)map.GetWidth());
				const int y = (int)(index / (uint32_t)map.GetWidth());
				for (const TerrainRuleDefinition& rule : rules)
				{
					if (Matches(rule, map, water, x, y) && random.Float() < rule.Chance)
					{
						map.Set(x, y, (uint8_t)rule.ToTileId);
						changed++;
						break;
					}
				}
			}
			return changed;
		}

		// The types to unlock now: locked ones (not in unlocked) whose UnlockTileId the map holds UnlockCount of, by id.
		static std::vector<uint8_t> NewUnlocks(const std::vector<TerrainTypeDefinition>& types, const std::vector<uint8_t>& unlocked,
			const TileMap& map)
		{
			std::vector<uint8_t> found;
			for (const TerrainTypeDefinition& type : types)
			{
				if (type.UnlockTileId < 0 || type.UnlockTileId > 255 || type.TileId < 0 || type.TileId > 255)
				{
					continue;
				}
				bool open = false;
				for (uint8_t tileId : unlocked)
				{
					open = open || tileId == (uint8_t)type.TileId;
				}
				if (open == false && map.GetCount((uint8_t)type.UnlockTileId) >= (uint32_t)(type.UnlockCount > 0 ? type.UnlockCount : 0))
				{
					found.push_back((uint8_t)type.TileId);
				}
			}
			return found;
		}

		// Ticks due by now at ticksPerSecond after `seconds` of world time, given how many ran; at most maxTicks.
		static int TicksDue(double seconds, uint64_t ticksRun, float ticksPerSecond, int maxTicks)
		{
			const double due = seconds * (double)ticksPerSecond - (double)ticksRun;
			const int whole = due > 0.0 ? (int)due : 0;
			return whole < maxTicks ? whole : maxTicks;
		}

		static int CountNeighbours(const TileMap& map, int x, int y, uint8_t tileId)
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
	};
}
