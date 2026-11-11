#pragma once

#include "Data/TerrainTypeDefinition.h"
#include "State/HeightMap.h"
#include "State/TileMap.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// Makes a world from a seed: layered value noise gives each tile a height and a moisture (0 to 1), and each tile takes
	// the generated terrain type whose ranges hold both; ShapeCoast rounds the map off with sea. Pure and repeatable.
	class TerrainGenRules
	{
	public:
		struct Settings
		{
			uint32_t Seed = 1;
			int Width = 64;
			int Height = 64;
			float HeightScale = 12.0f;   // tiles per cell of the coarsest height noise
			float MoistureScale = 16.0f; // tiles per cell of the coarsest moisture noise
			int Octaves = 3;             // each one half the cell size and half the weight of the one before
		};

		static TileMap Generate(const Settings& settings, const std::vector<TerrainTypeDefinition>& types)
		{
			TileMap map(settings.Width, settings.Height);
			for (int y = 0; y < map.GetHeight(); y++)
			{
				for (int x = 0; x < map.GetWidth(); x++)
				{
					const float height = Noise(x, y, settings.HeightScale, settings.Octaves, settings.Seed);
					const float moisture = Noise(x, y, settings.MoistureScale, settings.Octaves, settings.Seed ^ 0x9E3779B9u);
					map.Set(x, y, PickType(height, moisture, types));
				}
			}
			return map;
		}

		// The land's height in levels from the same height noise Generate uses: 0 up to fromHeight, then one level for every
		// 1 / levelsPerHeight above it, at most maxLevel. Only where keep[i] != 0 (empty: everywhere); 0 elsewhere.
		static HeightMap Levels(const Settings& settings, float fromHeight, float levelsPerHeight, int maxLevel,
			const std::vector<uint8_t>& keep = {})
		{
			HeightMap heights(settings.Width, settings.Height);
			for (int y = 0; y < settings.Height; y++)
			{
				for (int x = 0; x < settings.Width; x++)
				{
					const size_t index = (size_t)y * (size_t)settings.Width + (size_t)x;
					if (keep.empty() == false && (index >= keep.size() || keep[index] == 0))
					{
						continue;
					}
					const float height = Noise(x, y, settings.HeightScale, settings.Octaves, settings.Seed);
					const int level = (int)std::floor((height - fromHeight) * levelsPerHeight);
					heights.Set(x, y, (uint8_t)(level < 0 ? 0 : (level > maxLevel ? maxLevel : level)));
				}
			}
			heights.TakeChanges(); // a new map is not a list of changes
			return heights;
		}

		// Floods the map's rim with seaTileId along a rounded, wavy coast: a tile turns to sea when it is nearer than
		// coastWidth (0.3 to 2 times, by noise) to the edge of a rounded rectangle the size of the map. 0 changes nothing.
		static TileMap ShapeCoast(const TileMap& map, float coastWidth, uint32_t seed, uint8_t seaTileId)
		{
			TileMap shaped(map);
			if (coastWidth <= 0.0f)
			{
				return shaped;
			}
			const float halfX = (float)map.GetWidth() * 0.5f;
			const float halfY = (float)map.GetHeight() * 0.5f;
			const float corner = (halfX < halfY ? halfX : halfY) * 0.85f; // how round the corners are
			const float noiseScale = coastWidth * 3.0f > 8.0f ? coastWidth * 3.0f : 8.0f;
			for (int y = 0; y < map.GetHeight(); y++)
			{
				for (int x = 0; x < map.GetWidth(); x++)
				{
					const float px = std::abs((float)x + 0.5f - halfX) - (halfX - corner);
					const float py = std::abs((float)y + 0.5f - halfY) - (halfY - corner);
					const float outsideX = px > 0.0f ? px : 0.0f;
					const float outsideY = py > 0.0f ? py : 0.0f;
					const float inner = px > py ? px : py;
					const float edge = std::sqrt(outsideX * outsideX + outsideY * outsideY) + (inner < 0.0f ? inner : 0.0f) - corner;
					if (-edge < coastWidth * (0.3f + 1.7f * Contrast(Noise(x, y, noiseScale, 5, seed ^ 0x51EDC0A5u))))
					{
						shaped.Set(x, y, seaTileId);
					}
				}
			}
			return shaped;
		}

		// Turns the land nearest the sea into a beach: every non-sea tile within beachWidth (0.4 to 1.6 times, by noise)
		// tiles of a sea tile, counted in steps to the 8 neighbours. 0 changes nothing.
		static TileMap AddBeach(const TileMap& map, float beachWidth, uint32_t seed, uint8_t seaTileId, uint8_t beachTileId)
		{
			TileMap shaped(map);
			if (beachWidth <= 0.0f)
			{
				return shaped;
			}
			const std::vector<int> steps = StepsToSea(map, seaTileId);
			for (int y = 0; y < map.GetHeight(); y++)
			{
				for (int x = 0; x < map.GetWidth(); x++)
				{
					const int distance = steps[(size_t)y * (size_t)map.GetWidth() + (size_t)x];
					const float reach = beachWidth * (0.4f + 1.2f * Contrast(Noise(x, y, 6.0f, 2, seed ^ 0xBEAC4u)));
					if (distance > 0 && (float)distance <= reach)
					{
						shaped.Set(x, y, beachTileId);
					}
				}
			}
			return shaped;
		}

		// 1 inside the coast, where the player may paint; 0 for the open sea: the seaTileId tiles joined to the map's rim
		// (8 neighbours). Inland lakes stay land. Row by row, like TileMap::GetTiles.
		static std::vector<uint8_t> LandOf(const TileMap& map, uint8_t seaTileId)
		{
			const int width = map.GetWidth();
			const int height = map.GetHeight();
			std::vector<uint8_t> land((size_t)width * (size_t)height, 1);
			std::vector<int> queue;
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					const bool rim = x == 0 || y == 0 || x == width - 1 || y == height - 1;
					if (rim && map.Get(x, y) == seaTileId)
					{
						land[(size_t)y * (size_t)width + (size_t)x] = 0;
						queue.push_back(y * width + x);
					}
				}
			}
			for (size_t head = 0; head < queue.size(); head++)
			{
				const int x = queue[head] % width;
				const int y = queue[head] / width;
				for (int oy = -1; oy <= 1; oy++)
				{
					for (int ox = -1; ox <= 1; ox++)
					{
						const int nx = x + ox;
						const int ny = y + oy;
						const size_t next = (size_t)ny * (size_t)width + (size_t)nx;
						if (nx >= 0 && ny >= 0 && nx < width && ny < height && land[next] == 1 && map.Get(nx, ny) == seaTileId)
						{
							land[next] = 0;
							queue.push_back(ny * width + nx);
						}
					}
				}
			}
			return land;
		}

		// Steps from each tile to the nearest sea tile over the 8 neighbours (0 on sea; outside the map counts as sea).
		static std::vector<int> StepsToSea(const TileMap& map, uint8_t seaTileId)
		{
			const int width = map.GetWidth();
			const int height = map.GetHeight();
			std::vector<int> steps((size_t)width * (size_t)height, -1);
			std::vector<int> queue;
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					const bool rim = x == 0 || y == 0 || x == width - 1 || y == height - 1;
					const size_t index = (size_t)y * (size_t)width + (size_t)x;
					if (map.Get(x, y) == seaTileId)
					{
						steps[index] = 0;
						queue.push_back((int)index);
					}
					else if (rim)
					{
						steps[index] = 1;
						queue.push_back((int)index);
					}
				}
			}
			for (size_t head = 0; head < queue.size(); head++)
			{
				const int x = queue[head] % width;
				const int y = queue[head] / width;
				for (int oy = -1; oy <= 1; oy++)
				{
					for (int ox = -1; ox <= 1; ox++)
					{
						const int nx = x + ox;
						const int ny = y + oy;
						if (nx >= 0 && ny >= 0 && nx < width && ny < height && steps[(size_t)ny * (size_t)width + (size_t)nx] < 0)
						{
							steps[(size_t)ny * (size_t)width + (size_t)nx] = steps[(size_t)queue[head]] + 1;
							queue.push_back(ny * width + nx);
						}
					}
				}
			}
			return steps;
		}

		// Among the generated types whose ranges hold the point, the highest Priority (the first on a tie); without a match,
		// the generated type nearest to the point, so every tile gets a valid id. No generated types: 0.
		static uint8_t PickType(float height, float moisture, const std::vector<TerrainTypeDefinition>& types)
		{
			const TerrainTypeDefinition* best = nullptr;
			for (const TerrainTypeDefinition& type : types)
			{
				const bool holds = InRange(height, type.MinHeight, type.MaxHeight) && InRange(moisture, type.MinMoisture, type.MaxMoisture);
				if (type.Generated && holds)
				{
					if (best == nullptr || type.Priority > best->Priority)
					{
						best = &type;
					}
				}
			}
			if (best == nullptr)
			{
				float bestDistance = 0.0f;
				for (const TerrainTypeDefinition& type : types)
				{
					if (type.Generated == false)
					{
						continue;
					}
					const float distance = DistanceOutside(height, type.MinHeight, type.MaxHeight)
						+ DistanceOutside(moisture, type.MinMoisture, type.MaxMoisture);
					if (best == nullptr || distance < bestDistance)
					{
						best = &type;
						bestDistance = distance;
					}
				}
			}
			return best == nullptr ? (uint8_t)0 : ClampId(best->TileId);
		}

		// Layered value noise at a tile, 0 to 1.
		static float Noise(int x, int y, float scale, int octaves, uint32_t seed)
		{
			float sum = 0.0f;
			float weight = 1.0f;
			float total = 0.0f;
			float cell = scale > 1.0f ? scale : 1.0f;
			const int layers = octaves > 0 ? octaves : 1;
			for (int octave = 0; octave < layers; octave++)
			{
				sum += weight * ValueNoise(((float)x + 0.5f) / cell, ((float)y + 0.5f) / cell, seed + (uint32_t)octave * 1013u);
				total += weight;
				weight *= 0.5f;
				cell = cell * 0.5f > 1.0f ? cell * 0.5f : 1.0f;
			}
			return sum / total;
		}

	private:
		// Spreads noise (mostly near 0.5) over 0 to 1, clamped.
		static float Contrast(float noise)
		{
			const float spread = (noise - 0.5f) * 3.0f + 0.5f;
			return spread < 0.0f ? 0.0f : (spread > 1.0f ? 1.0f : spread);
		}

		static bool InRange(float value, float low, float high)
		{
			return value >= low && value <= high;
		}

		static float DistanceOutside(float value, float low, float high)
		{
			if (value < low)
			{
				return low - value;
			}
			if (value > high)
			{
				return value - high;
			}
			return 0.0f;
		}

		static uint8_t ClampId(int tileId)
		{
			return (uint8_t)(tileId < 0 ? 0 : (tileId > 255 ? 255 : tileId));
		}

		// 0 to 1 per lattice point: an integer hash, the same on every compiler.
		static float Lattice(int x, int y, uint32_t seed)
		{
			uint32_t h = seed;
			h ^= (uint32_t)x * 0x27D4EB2Du;
			h = (h ^ (h >> 15)) * 0x85EBCA6Bu;
			h ^= (uint32_t)y * 0x165667B1u;
			h = (h ^ (h >> 13)) * 0xC2B2AE35u;
			h ^= h >> 16;
			return (float)(h & 0xFFFFFFu) / (float)0x1000000u;
		}

		// Smoothstep-interpolated lattice values, as in scripts/prototypes/terrain_blend.py's vnoise.
		static float ValueNoise(float u, float v, uint32_t seed)
		{
			const float fx0 = std::floor(u);
			const float fy0 = std::floor(v);
			const int x0 = (int)fx0;
			const int y0 = (int)fy0;
			float fx = u - fx0;
			float fy = v - fy0;
			fx = fx * fx * (3.0f - 2.0f * fx);
			fy = fy * fy * (3.0f - 2.0f * fy);
			const float top = Lattice(x0, y0, seed) * (1.0f - fx) + Lattice(x0 + 1, y0, seed) * fx;
			const float bottom = Lattice(x0, y0 + 1, seed) * (1.0f - fx) + Lattice(x0 + 1, y0 + 1, seed) * fx;
			return top * (1.0f - fy) + bottom * fy;
		}
	};
}
