#pragma once

#include "Data/TerrainTypeDefinition.h"
#include "State/HeightMap.h"
#include "State/TileMap.h"

#include <cstdint>
#include <vector>

namespace Hydro
{
	// Mountains from the land's height: rock from RockLevel, snow from the ragged snowline, back down when the land drops
	// (snow to rock, rock to bare dirt). Meltwater runs down the raised land below the snow: it wets it, and fills riverbeds.
	class MountainRules
	{
	public:
		struct Settings
		{
			int RockTileId = 6; // the types with the Rock and Snow roles (FromTypes); -1 none
			int SnowTileId = 4;
			int BareTileId = 1; // what rock turns back into (the rock type's BelowTileId)
			int RockLevel = 4;  // the types' FromLevel
			int SnowLevel = 9;
			float SnowShift = 0.0f; // the season's move of the snowline (SeasonState::SnowShift)
			float Ragged = 1.5f; // how much the snowline wobbles from tile to tile, in levels (both ways together)
			int MeltReach = 16; // tiles of raised land below the snow that meltwater runs down
		};

		struct SettleReport
		{
			int Snowed = 0; // land that turned to snow
			int Melted = 0; // snow that turned back to rock (or lower)
			int Rocked = 0; // land that turned to rock
			int Bared = 0;  // rock that turned back to bare land
		};

		// `settings` with the ids and levels from the types' roles (unchanged when a role has no type or no FromLevel).
		static Settings FromTypes(const std::vector<TerrainTypeDefinition>& types, Settings settings)
		{
			for (const TerrainTypeDefinition& type : types)
			{
				if (type.Role == TerrainRole::Rock)
				{
					settings.RockTileId = type.TileId;
					settings.RockLevel = type.FromLevel > 0 ? type.FromLevel : settings.RockLevel;
					settings.BareTileId = type.BelowTileId >= 0 ? type.BelowTileId : settings.BareTileId;
				}
				else if (type.Role == TerrainRole::Snow)
				{
					settings.SnowTileId = type.TileId;
					settings.SnowLevel = type.FromLevel > 0 ? type.FromLevel : settings.SnowLevel;
				}
			}
			return settings;
		}

		// The level at which the tile (x, y) turns to snow: SnowLevel moved by the season and by up to half of Ragged either
		// way, fixed per tile.
		static float SnowLine(int x, int y, const Settings& settings)
		{
			return (float)settings.SnowLevel + settings.SnowShift + (Hash(x, y) - 0.5f) * settings.Ragged;
		}

		// The type the height alone gives the tile (x, y) at `level`: snow, rock, or -1 (the land keeps its own type). Snow
		// only on rock heights, however low a winter brings the snowline: grass under it stays grass.
		static int LevelTile(int level, int x, int y, const Settings& settings)
		{
			if (settings.SnowTileId >= 0 && (float)level >= SnowLine(x, y, settings) && level >= settings.RockLevel)
			{
				return settings.SnowTileId;
			}
			if (settings.RockTileId >= 0 && level >= settings.RockLevel)
			{
				return settings.RockTileId;
			}
			return -1;
		}

		// Every tile to the type its height gives it (LevelTile), rock and snow below it back to bare land.
		static SettleReport Settle(TileMap& map, const HeightMap& heights, const Settings& settings)
		{
			SettleReport report;
			if (heights.GetLevels().size() != map.GetTiles().size())
			{
				return report; // no heights yet: nothing to settle by
			}
			for (int y = 0; y < map.GetHeight(); y++)
			{
				for (int x = 0; x < map.GetWidth(); x++)
				{
					SettleTile(map, heights, x, y, settings, report);
				}
			}
			return report;
		}

		// The same for the listed tiles (indices, row by row) only: after a brush stroke or the height changes of a tick.
		static SettleReport SettleTiles(TileMap& map, const HeightMap& heights, const std::vector<uint32_t>& tiles,
			const Settings& settings)
		{
			SettleReport report;
			const uint32_t count = heights.GetLevels().size() == map.GetTiles().size() ? (uint32_t)map.GetTiles().size() : 0u;
			for (uint32_t index : tiles)
			{
				if (index < count)
				{
					SettleTile(map, heights, (int)(index % (uint32_t)map.GetWidth()), (int)(index / (uint32_t)map.GetWidth()), settings,
						report);
				}
			}
			return report;
		}

		// Per tile, how much meltwater runs there: 255 on snow, fading to 1 at MeltReach steps down the raised land (level 1 and
		// up); 0 elsewhere. It wets that land (WaterRules::MeltMoisture), and a riverbed on the ground next to it fills.
		static std::vector<uint8_t> Meltwater(const TileMap& map, const HeightMap& heights, const Settings& settings)
		{
			const int width = map.GetWidth();
			const size_t count = map.GetTiles().size();
			std::vector<uint8_t> melt(count, 0);
			if (settings.SnowTileId < 0 || heights.GetLevels().size() != count)
			{
				return melt;
			}
			std::vector<int> steps(count, -1);
			std::vector<uint32_t> queue;
			for (size_t i = 0; i < count; i++)
			{
				if ((int)map.GetTiles()[i] == settings.SnowTileId)
				{
					steps[i] = 0;
					melt[i] = 255;
					queue.push_back((uint32_t)i);
				}
			}
			for (size_t head = 0; head < queue.size(); head++)
			{
				const int x = (int)(queue[head] % (uint32_t)width);
				const int y = (int)(queue[head] / (uint32_t)width);
				for (int oy = -1; oy <= 1; oy++)
				{
					for (int ox = -1; ox <= 1; ox++)
					{
						const int nx = x + ox;
						const int ny = y + oy;
						if (map.InBounds(nx, ny) == false || heights.Get(nx, ny) == 0)
						{
							continue;
						}
						const size_t next = (size_t)ny * (size_t)width + (size_t)nx;
						if (steps[next] < 0 && steps[queue[head]] < settings.MeltReach)
						{
							steps[next] = steps[queue[head]] + 1;
							melt[next] = MeltAt(steps[next], settings);
							queue.push_back((uint32_t)next);
						}
					}
				}
			}
			return melt;
		}

		// The meltwater `steps` tiles down from the snow: 255 at the snow, falling evenly to 1 at MeltReach.
		static uint8_t MeltAt(int steps, const Settings& settings)
		{
			const int reach = settings.MeltReach > 0 ? settings.MeltReach : 1;
			const int value = 255 - (254 * steps) / reach;
			return (uint8_t)(value < 1 ? 1 : (value > 255 ? 255 : value));
		}

	private:
		static void SettleTile(TileMap& map, const HeightMap& heights, int x, int y, const Settings& settings, SettleReport& report)
		{
			const int tile = map.Get(x, y);
			const int want = LevelTile(heights.Get(x, y), x, y, settings);
			const bool raised = tile == settings.RockTileId || tile == settings.SnowTileId;
			int next = tile;
			if (want >= 0)
			{
				next = want;
			}
			else if (raised && settings.BareTileId >= 0)
			{
				next = settings.BareTileId;
			}
			if (next == tile || map.Set(x, y, (uint8_t)next) == false)
			{
				return;
			}
			report.Snowed += next == settings.SnowTileId ? 1 : 0;
			report.Melted += tile == settings.SnowTileId ? 1 : 0;
			report.Rocked += next == settings.RockTileId && tile != settings.SnowTileId ? 1 : 0;
			report.Bared += tile == settings.RockTileId && next != settings.SnowTileId ? 1 : 0;
		}

		// 0 to 1 per tile, the same on every compiler.
		static float Hash(int x, int y)
		{
			uint32_t h = 0x6D2B79F5u;
			h ^= (uint32_t)x * 0x27D4EB2Du;
			h = (h ^ (h >> 15)) * 0x85EBCA6Bu;
			h ^= (uint32_t)y * 0x165667B1u;
			h = (h ^ (h >> 13)) * 0xC2B2AE35u;
			h ^= h >> 16;
			return (float)(h & 0xFFFFFFu) / (float)0x1000000u;
		}
	};
}
