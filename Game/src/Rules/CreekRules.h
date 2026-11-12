#pragma once

#include "Data/TerrainTypeDefinition.h"
#include "State/HeightMap.h"
#include "State/Random.h"
#include "State/TileMap.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace Hydro
{
	// Dry creeks for a new world: from where the hidden groundwater is strongest, away from other water, a short narrow
	// riverbed winds over the low ground. Its head stays land: digging it opens the spring that fills the creek. Pure.
	class CreekRules
	{
	public:
		struct Settings
		{
			float PerTenThousand = 1.2f; // creeks per 100 x 100 tiles of land, about
			float SpringFrom = 0.8f;     // groundwater (0 to 1) a spring needs
			int MinLength = 6;           // riverbed tiles after the spring, at least ...
			int MaxLength = 14;          // ... and at most
			int WaterDistance = 10;      // tiles a spring keeps from any water, riverbed or rock
			int Spacing = 28;            // tiles between springs
			uint8_t WaterTileId = 3;
			uint8_t BedTileId = 5;
			std::array<uint8_t, 256> Carvable{}; // per tile id: 1 where a creek may run (the Land types)
		};

		struct Creek
		{
			int SpringX = 0;           // its head: land over strong groundwater, a spring once dug
			int SpringY = 0;
			std::vector<uint32_t> Bed; // the dry riverbed tiles from the head on (seal them: Hydrology::Sealed)
		};

		// The water and riverbed ids and the tiles a creek may cross (TerrainRole Land) from the types, on top of `base`.
		static Settings FromTypes(const std::vector<TerrainTypeDefinition>& types, Settings base)
		{
			base.Carvable.fill(0);
			for (const TerrainTypeDefinition& type : types)
			{
				if (type.TileId < 0 || type.TileId > 255)
				{
					continue;
				}
				base.Carvable[(size_t)type.TileId] = type.Role == TerrainRole::Land ? 1 : 0;
				base.WaterTileId = type.Role == TerrainRole::Water ? (uint8_t)type.TileId : base.WaterTileId;
				base.BedTileId = type.Role == TerrainRole::Riverbed ? (uint8_t)type.TileId : base.BedTileId;
			}
			return base;
		}

		// Carves the dry creeks into the map: land[i] != 0 inside the coast (empty: all), heights (nullptr: flat; creeks keep
		// to level 0), groundwater a byte per tile. Returns them, strongest head first.
		static std::vector<Creek> Carve(TileMap& map, const std::vector<uint8_t>& land, const HeightMap* heights,
			const std::vector<uint8_t>& groundwater, const Settings& settings, Random& random)
		{
			std::vector<Creek> creeks;
			const size_t count = map.GetTiles().size();
			if (groundwater.size() != count || count == 0)
			{
				return creeks;
			}
			// Candidates on a coarse grid, the wettest tile of each cell, strongest first.
			const int cell = 6;
			std::vector<uint32_t> candidates;
			int landTiles = 0;
			for (size_t i = 0; i < count; i++)
			{
				landTiles += land.empty() || land[i] != 0 ? 1 : 0;
			}
			for (int cy = 0; cy < map.GetHeight(); cy += cell)
			{
				for (int cx = 0; cx < map.GetWidth(); cx += cell)
				{
					int best = -1;
					for (int y = cy; y < cy + cell && y < map.GetHeight(); y++)
					{
						for (int x = cx; x < cx + cell && x < map.GetWidth(); x++)
						{
							const size_t index = (size_t)y * (size_t)map.GetWidth() + (size_t)x;
							if (Fits(map, land, heights, groundwater, settings, x, y)
								&& (best < 0 || groundwater[index] > groundwater[(size_t)best]))
							{
								best = (int)index;
							}
						}
					}
					if (best >= 0)
					{
						candidates.push_back((uint32_t)best);
					}
				}
			}
			std::sort(candidates.begin(), candidates.end(), [&groundwater](uint32_t a, uint32_t b)
			{
				return groundwater[a] != groundwater[b] ? groundwater[a] > groundwater[b] : a < b;
			});
			const int wanted = (int)((float)landTiles / 10000.0f * settings.PerTenThousand + 0.5f);
			for (uint32_t index : candidates)
			{
				if ((int)creeks.size() >= wanted)
				{
					break;
				}
				const int x = (int)(index % (uint32_t)map.GetWidth());
				const int y = (int)(index / (uint32_t)map.GetWidth());
				bool spaced = true;
				for (const Creek& creek : creeks)
				{
					const int dx = creek.SpringX - x;
					const int dy = creek.SpringY - y;
					spaced = spaced && dx * dx + dy * dy >= settings.Spacing * settings.Spacing;
				}
				// The earlier creeks may have come near since the candidate was found.
				if (spaced == false || Fits(map, land, heights, groundwater, settings, x, y) == false)
				{
					continue;
				}
				Creek creek = Wind(map, land, heights, settings, x, y, random);
				if ((int)creek.Bed.size() < settings.MinLength)
				{
					continue;
				}
				for (uint32_t bed : creek.Bed)
				{
					map.Set((int)(bed % (uint32_t)map.GetWidth()), (int)(bed / (uint32_t)map.GetWidth()), settings.BedTileId);
				}
				creeks.push_back(creek);
			}
			return creeks;
		}

		// Whether a spring can be at (x, y): carvable low land inside the coast over strong groundwater, WaterDistance from
		// any water, riverbed or tile a creek can't cross.
		static bool Fits(const TileMap& map, const std::vector<uint8_t>& land, const HeightMap* heights,
			const std::vector<uint8_t>& groundwater, const Settings& settings, int x, int y)
		{
			const size_t index = (size_t)y * (size_t)map.GetWidth() + (size_t)x;
			if ((float)groundwater[index] / 255.0f < settings.SpringFrom || Open(map, land, heights, settings, x, y) == false)
			{
				return false;
			}
			const int reach = settings.WaterDistance;
			for (int dy = -reach; dy <= reach; dy++)
			{
				for (int dx = -reach; dx <= reach; dx++)
				{
					if (dx * dx + dy * dy > reach * reach || map.InBounds(x + dx, y + dy) == false)
					{
						continue;
					}
					const uint8_t tile = map.Get(x + dx, y + dy);
					if (tile == settings.WaterTileId || tile == settings.BedTileId)
					{
						return false;
					}
				}
			}
			return true;
		}

	private:
		// A tile a creek may run over: carvable, inside the coast, on the ground (level 0).
		static bool Open(const TileMap& map, const std::vector<uint8_t>& land, const HeightMap* heights, const Settings& settings,
			int x, int y)
		{
			if (map.InBounds(x, y) == false)
			{
				return false;
			}
			const size_t index = (size_t)y * (size_t)map.GetWidth() + (size_t)x;
			const bool inside = land.empty() || (index < land.size() && land[index] != 0);
			const bool low = heights == nullptr || heights->GetLevels().size() != map.GetTiles().size() || heights->Get(x, y) == 0;
			return inside && low && settings.Carvable[map.Get(x, y)] != 0;
		}

		// The bed from a spring at (x, y): a winding walk that mostly keeps its heading, never touching itself, ending early
		// where it can't go on (other water, raised land, the coast).
		static Creek Wind(const TileMap& map, const std::vector<uint8_t>& land, const HeightMap* heights, const Settings& settings,
			int x, int y, Random& random)
		{
			Creek creek;
			creek.SpringX = x;
			creek.SpringY = y;
			const int steps[4][2] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };
			const int spread = settings.MaxLength - settings.MinLength;
			const int length = settings.MinLength + (int)random.Below((uint32_t)(spread > 0 ? spread : 0) + 1u);
			int heading = (int)random.Below(4);
			std::vector<uint32_t> path = { (uint32_t)((size_t)y * (size_t)map.GetWidth() + (size_t)x) };
			int cx = x;
			int cy = y;
			for (int i = 0; i < length; i++)
			{
				const float roll = random.Float();
				const int first = roll < 0.6f ? 0 : (roll < 0.8f ? 1 : 2);
				const int turns[3] = { 0, 1, 3 }; // straight on, or a quarter turn either way; never back
				bool moved = false;
				for (int attempt = 0; attempt < 3 && moved == false; attempt++)
				{
					const int dir = (heading + turns[(first + attempt) % 3]) % 4;
					const int nx = cx + steps[dir][0];
					const int ny = cy + steps[dir][1];
					if (Open(map, land, heights, settings, nx, ny) == false || Touches(map, path, nx, ny))
					{
						continue;
					}
					cx = nx;
					cy = ny;
					heading = dir;
					path.push_back((uint32_t)((size_t)cy * (size_t)map.GetWidth() + (size_t)cx));
					creek.Bed.push_back(path.back());
					moved = true;
				}
				if (moved == false)
				{
					break;
				}
			}
			return creek;
		}

		// Whether (x, y) is on or beside the path before its last two tiles (the walk's own corner is fine).
		static bool Touches(const TileMap& map, const std::vector<uint32_t>& path, int x, int y)
		{
			for (size_t i = 0; i + 2 < path.size(); i++)
			{
				const int px = (int)(path[i] % (uint32_t)map.GetWidth());
				const int py = (int)(path[i] / (uint32_t)map.GetWidth());
				if (std::abs(px - x) <= 1 && std::abs(py - y) <= 1)
				{
					return true;
				}
			}
			return path.empty() == false && path.back() == (uint32_t)((size_t)y * (size_t)map.GetWidth() + (size_t)x);
		}
	};
}
