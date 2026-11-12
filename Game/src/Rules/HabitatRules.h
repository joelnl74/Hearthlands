#pragma once

#include "Data/AnimalDefinition.h"
#include "State/HeightMap.h"
#include "State/Hydrology.h"
#include "State/TileMap.h"

#include <cstdint>
#include <deque>
#include <vector>

namespace Hydro
{
	// The kind of each water tile for fish and water birds: the sea; of the player's water (not Native) streams, rivers and
	// lakes by breadth, mouths, water joined to the sea, cold streams. Classify reads the whole map: only on changes. Pure.
	class HabitatRules
	{
	public:
		// The bits of Hydrology::Habitat.
		static constexpr uint8_t SeaBit = 1;
		static constexpr uint8_t StreamBit = 2;
		static constexpr uint8_t RiverBit = 4;
		static constexpr uint8_t LakeBit = 8;
		static constexpr uint8_t MouthBit = 16;   // within MouthReach water steps of the sea
		static constexpr uint8_t SeaLinkBit = 32; // joined to the sea through water
		static constexpr uint8_t ColdBit = 64;    // meltwater runs there, or it is up in the hills
		static constexpr int HabitatCount = (int)WaterHabitat::Fresh + 1;

		struct Settings
		{
			int Window = 4;       // tiles either side of a tile looked at for how broad its water is (a 9 x 9 square)
			int StreamMost = 20;  // water tiles in that square, at most, for a stream (2 across: 18)
			int LakeLeast = 49;   // ... at least, for a lake (6 across: 54); between them a big river
			int MouthReach = 8;   // water steps from the sea that count as the river's mouth
			int ColdLevel = 4;    // land this high (rock) makes its water cold
		};

		// 1 for each water tile now: call when the world is made, before the player changes anything.
		static std::vector<uint8_t> NativeOf(const TileMap& map, uint8_t waterTileId)
		{
			std::vector<uint8_t> native(map.GetTiles().size(), 0);
			for (size_t i = 0; i < native.size(); i++)
			{
				native[i] = map.GetTiles()[i] == waterTileId ? 1 : 0;
			}
			return native;
		}

		static std::vector<uint8_t> Classify(const TileMap& map, const Hydrology& water, const std::vector<uint8_t>& land,
			const HeightMap& heights)
		{
			return Classify(map, water, land, heights, Settings());
		}

		// Every tile's habitat bits (0 on land). land: 1 inside the coast (TerrainGenRules::LandOf); the water outside it is
		// the sea; empty: no sea. heights: empty or the map's size.
		static std::vector<uint8_t> Classify(const TileMap& map, const Hydrology& water, const std::vector<uint8_t>& land,
			const HeightMap& heights, const Settings& settings)
		{
			const int width = map.GetWidth();
			const int height = map.GetHeight();
			const size_t count = map.GetTiles().size();
			std::vector<uint8_t> bits(count, 0);
			if (width <= 0 || height <= 0)
			{
				return bits;
			}
			const auto isWater = [&](size_t i) { return map.GetTiles()[i] == water.WaterTileId; };
			const auto isSea = [&](size_t i) { return isWater(i) && land.size() == count && land[i] == 0; };
			const auto isMade = [&](size_t i)
			{
				return isWater(i) && isSea(i) == false && (i >= water.Native.size() || water.Native[i] == 0);
			};

			// Water tiles in each tile's square, from a summed table.
			std::vector<int> sum((size_t)(width + 1) * (size_t)(height + 1), 0);
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					const size_t i = (size_t)y * (size_t)width + (size_t)x;
					sum[(size_t)(y + 1) * (size_t)(width + 1) + (size_t)(x + 1)] = (isWater(i) ? 1 : 0)
						+ sum[(size_t)y * (size_t)(width + 1) + (size_t)(x + 1)] + sum[(size_t)(y + 1) * (size_t)(width + 1) + (size_t)x]
						- sum[(size_t)y * (size_t)(width + 1) + (size_t)x];
				}
			}
			const bool raised = heights.GetLevels().size() == count;
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					const size_t i = (size_t)y * (size_t)width + (size_t)x;
					if (isSea(i))
					{
						bits[i] = SeaBit;
						continue;
					}
					if (isMade(i) == false)
					{
						continue;
					}
					const int x0 = x - settings.Window < 0 ? 0 : x - settings.Window;
					const int y0 = y - settings.Window < 0 ? 0 : y - settings.Window;
					const int x1 = x + settings.Window + 1 > width ? width : x + settings.Window + 1;
					const int y1 = y + settings.Window + 1 > height ? height : y + settings.Window + 1;
					const size_t row0 = (size_t)y0 * (size_t)(width + 1);
					const size_t row1 = (size_t)y1 * (size_t)(width + 1);
					const int broad = sum[row1 + (size_t)x1] - sum[row0 + (size_t)x1] - sum[row1 + (size_t)x0] + sum[row0 + (size_t)x0];
					bits[i] = broad <= settings.StreamMost ? StreamBit : (broad >= settings.LakeLeast ? LakeBit : RiverBit);
					const bool melt = i < water.Melt.size() && water.Melt[i] > 0;
					const bool high = raised && heights.GetLevels()[i] >= settings.ColdLevel;
					bits[i] |= melt || high ? ColdBit : 0;
				}
			}

			// From the sea up the water the player made: every step is joined to it, the first MouthReach are its mouth.
			std::vector<int> steps(count, -1);
			std::deque<size_t> open;
			for (size_t i = 0; i < count; i++)
			{
				if (isSea(i))
				{
					steps[i] = 0;
					open.push_back(i);
				}
			}
			while (open.empty() == false)
			{
				const size_t at = open.front();
				open.pop_front();
				const int ax = (int)(at % (size_t)width);
				const int ay = (int)(at / (size_t)width);
				const int around[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
				for (const auto& step : around)
				{
					const int nx = ax + step[0];
					const int ny = ay + step[1];
					if (nx < 0 || ny < 0 || nx >= width || ny >= height)
					{
						continue;
					}
					const size_t next = (size_t)ny * (size_t)width + (size_t)nx;
					if (steps[next] >= 0 || isWater(next) == false || isSea(next))
					{
						continue;
					}
					steps[next] = steps[at] + 1;
					open.push_back(next);
					if (isMade(next))
					{
						bits[next] |= SeaLinkBit | (steps[next] <= settings.MouthReach ? MouthBit : 0);
					}
				}
			}
			return bits;
		}

		// Whether a tile with these bits is the habitat.
		static bool Matches(WaterHabitat habitat, uint8_t bits)
		{
			switch (habitat)
			{
			case WaterHabitat::None: return true;
			case WaterHabitat::Sea: return (bits & SeaBit) != 0;
			case WaterHabitat::Stream: return (bits & StreamBit) != 0;
			case WaterHabitat::River: return (bits & RiverBit) != 0;
			case WaterHabitat::Lake: return (bits & LakeBit) != 0;
			case WaterHabitat::Flowing: return (bits & (StreamBit | RiverBit)) != 0;
			case WaterHabitat::Mouth: return (bits & MouthBit) != 0;
			case WaterHabitat::SeaRun: return (bits & (StreamBit | RiverBit)) != 0 && (bits & SeaLinkBit) != 0;
			case WaterHabitat::ColdStream: return (bits & StreamBit) != 0 && (bits & ColdBit) != 0;
			case WaterHabitat::Fresh: return (bits & (StreamBit | RiverBit | LakeBit)) != 0;
			}
			return false;
		}

		// Per WaterHabitat (by its number) the tiles that are it; None counts every water tile with any bits.
		static std::vector<uint32_t> Count(const std::vector<uint8_t>& bits)
		{
			std::vector<uint32_t> counts((size_t)HabitatCount, 0);
			for (uint8_t tile : bits)
			{
				if (tile == 0)
				{
					continue;
				}
				for (int h = 0; h < HabitatCount; h++)
				{
					counts[(size_t)h] += Matches((WaterHabitat)h, tile) ? 1u : 0u;
				}
			}
			return counts;
		}
	};
}
