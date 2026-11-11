#pragma once

#include "Rules/BrushRules.h"
#include "State/HeightMap.h"
#include "State/TileMap.h"

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// The land's height as maths: how far a stroke of Raise or Dig moves a tile (a dome, not a block), slopes kept within
	// MaxStep levels, how high a level stands in the world and where a ray meets the raised land. Pure.
	class HeightRules
	{
	public:
		struct Settings
		{
			int MaxLevel = 15;     // the highest level
			int MaxStep = 2;       // most levels between neighbouring land tiles (8 neighbours)
			uint8_t WaterTileId = 3; // water and riverbed stay on the ground: slopes stop at them (a cliff by a river)
			uint8_t BedTileId = 5;
		};

		// One press of Raise or Dig: a tile moves at most its bump from its level when the press began, however long it is held.
		struct Stroke
		{
			std::vector<int16_t> Start; // per tile: its level at the press, -1 not touched yet

			void Begin(size_t count)
			{
				Start.assign(count, (int16_t)-1);
			}
		};

		// Levels one stroke moves a tile `distance` tiles from the brush's centre: 1 at the rim, one more every 2 tiles in
		// (a radius 8 brush: 5 in the middle), so a stroke makes a dome.
		static int Bump(float distance, float radius)
		{
			const float inside = radius - distance;
			return 1 + (inside > 0.0f ? (int)std::floor(inside * 0.5f) : 0);
		}

		// The tile's level when the stroke began (recorded on first touch).
		static int StartLevel(const HeightMap& heights, Stroke& stroke, int x, int y)
		{
			const size_t index = (size_t)y * (size_t)heights.GetWidth() + (size_t)x;
			if (index >= stroke.Start.size())
			{
				return heights.Get(x, y);
			}
			if (stroke.Start[index] < 0)
			{
				stroke.Start[index] = (int16_t)heights.Get(x, y);
			}
			return stroke.Start[index];
		}

		// Raises (bump > 0) or lowers (bump < 0) the tile towards its stroke start plus bump, within 0 and MaxLevel. True on a change.
		static bool Move(HeightMap& heights, Stroke& stroke, int x, int y, int bump, const Settings& settings)
		{
			if (heights.InBounds(x, y) == false)
			{
				return false;
			}
			const int start = StartLevel(heights, stroke, x, y);
			const int target = Clamp(start + bump, 0, settings.MaxLevel);
			const int level = heights.Get(x, y);
			const bool move = bump > 0 ? level < target : level > target;
			return move && heights.Set(x, y, (uint8_t)target);
		}

		// Keeps slopes within MaxStep after a change, spreading out from `from`: raising lifts lower land neighbours, lowering
		// cuts higher ones down. The sea (land 0), water and riverbed never move. Returns the tiles it moved.
		static std::vector<uint32_t> Slump(HeightMap& heights, const TileMap& map, const std::vector<uint8_t>& land,
			std::vector<uint32_t> from, bool raising, const Settings& settings)
		{
			std::vector<uint32_t> moved;
			const int width = heights.GetWidth();
			for (size_t head = 0; head < from.size(); head++)
			{
				const int x = (int)(from[head] % (uint32_t)width);
				const int y = (int)(from[head] / (uint32_t)width);
				const int level = heights.Get(x, y);
				for (int oy = -1; oy <= 1; oy++)
				{
					for (int ox = -1; ox <= 1; ox++)
					{
						const int nx = x + ox;
						const int ny = y + oy;
						if ((ox == 0 && oy == 0) || Movable(map, land, nx, ny, settings) == false)
						{
							continue;
						}
						const int next = heights.Get(nx, ny);
						const int limit = raising ? level - settings.MaxStep : level + settings.MaxStep;
						const bool off = raising ? next < limit : next > limit;
						if (off && heights.Set(nx, ny, (uint8_t)Clamp(limit, 0, settings.MaxLevel)))
						{
							const uint32_t index = (uint32_t)(ny * width + nx);
							moved.push_back(index);
							from.push_back(index);
						}
					}
				}
			}
			return moved;
		}

		// Whether the slope rule may move the tile: on the map, on the land, not water or riverbed.
		static bool Movable(const TileMap& map, const std::vector<uint8_t>& land, int x, int y, const Settings& settings)
		{
			if (map.InBounds(x, y) == false)
			{
				return false;
			}
			const size_t index = (size_t)y * (size_t)map.GetWidth() + (size_t)x;
			const uint8_t tile = map.GetTiles()[index];
			const bool onLand = land.empty() || (index < land.size() && land[index] != 0);
			return onLand && tile != settings.WaterTileId && tile != settings.BedTileId;
		}

		// A level's height in metres for `levelPixels` art pixels of cliff on screen, at pixelsPerTile art pixels a metre seen
		// from `elevationDegrees` above the ground (a wall h high shows h cos(elevation)): 8 at 32 from 30 degrees = 0.2887.
		static float LevelHeight(float levelPixels, float pixelsPerTile, float elevationDegrees)
		{
			const float shown = pixelsPerTile * std::cos(glm::radians(elevationDegrees));
			return shown > 0.0f ? levelPixels / shown : 0.0f;
		}

		// Where a ray (map space: from its centre, z up, 1 m tiles) first meets the raised land, else the ground: point the map
		// point (BrushRules::MapPoint), z the height met. False off the map or for a ray that doesn't go down.
		static bool RayToLand(const glm::vec3& origin, const glm::vec3& direction, const HeightMap& heights, float levelHeight,
			int topLevel, glm::vec2& point, float& z)
		{
			const int width = heights.GetWidth();
			const int height = heights.GetHeight();
			if (direction.z > -1e-6f || width == 0 || height == 0)
			{
				return false;
			}
			const float top = (float)topLevel * levelHeight;
			const float tGround = -origin.z / direction.z;
			float t = origin.z > top ? (top - origin.z) / direction.z : 0.0f;
			const float across = glm::length(glm::vec2(direction.x, direction.y));
			const float step = across > 1e-4f ? 0.2f / across : (tGround - t) + 1.0f; // a fifth of a tile at a time
			for (int i = 0; i < 20000 && t < tGround; i++)
			{
				const float next = t + step < tGround ? t + step : tGround;
				if (Below(origin + direction * next, heights, levelHeight))
				{
					float low = t;
					float high = next;
					for (int k = 0; k < 8; k++)
					{
						const float middle = (low + high) * 0.5f;
						if (Below(origin + direction * middle, heights, levelHeight))
						{
							high = middle;
						}
						else
						{
							low = middle;
						}
					}
					const glm::vec3 hit = origin + direction * high;
					point = BrushRules::MapPoint(glm::vec2(hit.x, hit.y), width, height);
					z = (float)LevelAt(heights, point) * levelHeight;
					return BrushRules::OnMap(point, width, height);
				}
				t = next;
			}
			const glm::vec3 ground = origin + direction * tGround;
			point = BrushRules::MapPoint(glm::vec2(ground.x, ground.y), width, height);
			z = 0.0f;
			return BrushRules::OnMap(point, width, height);
		}

		// The level under a map point (0 off the map).
		static int LevelAt(const HeightMap& heights, const glm::vec2& point)
		{
			return heights.Get((int)std::floor(point.x), (int)std::floor(point.y));
		}

	private:
		static int Clamp(int value, int low, int high)
		{
			return value < low ? low : (value > high ? high : value);
		}

		// Whether a point (map space) is inside the raised land under it.
		static bool Below(const glm::vec3& at, const HeightMap& heights, float levelHeight)
		{
			const glm::vec2 point = BrushRules::MapPoint(glm::vec2(at.x, at.y), heights.GetWidth(), heights.GetHeight());
			return at.z <= (float)LevelAt(heights, point) * levelHeight + 1e-4f;
		}
	};
}
