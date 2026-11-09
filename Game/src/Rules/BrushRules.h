#pragma once

#include "Data/TerrainTypeDefinition.h"

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// The player's terrain brush as maths: where the pointer's ray meets the ground, where that is on the map, the round
	// set of tiles a brush covers, and which tile types a run starts with. Pure; the map is centred on the ground, 1 m tiles.
	class BrushRules
	{
	public:
		// Where a ray meets the plane z = groundZ. False when it runs parallel to the plane or points away from it.
		static bool RayToGround(const glm::vec3& origin, const glm::vec3& direction, float groundZ, glm::vec3& hit)
		{
			if (std::abs(direction.z) < 1e-6f)
			{
				return false;
			}
			const float t = (groundZ - origin.z) / direction.z;
			if (t < 0.0f)
			{
				return false;
			}
			hit = origin + direction * t;
			return true;
		}

		// A point given from the map's centre (Y up) in tiles from the map's top-left corner, x right and y down like
		// TileMap's rows: tile (x, y) covers [x, x + 1) x [y, y + 1).
		static glm::vec2 MapPoint(const glm::vec2& local, int width, int height)
		{
			return glm::vec2(local.x + (float)width * 0.5f, (float)height * 0.5f - local.y);
		}

		static bool OnMap(const glm::vec2& point, int width, int height)
		{
			return point.x >= 0.0f && point.y >= 0.0f && point.x < (float)width && point.y < (float)height;
		}

		// Points from one map point to another at most `spacing` apart, both ends included, so a fast drag leaves no gaps.
		static std::vector<glm::vec2> Stroke(const glm::vec2& from, const glm::vec2& to, float spacing)
		{
			const float length = glm::length(to - from);
			const int steps = (int)std::ceil(length / (spacing > 0.05f ? spacing : 0.05f));
			std::vector<glm::vec2> points;
			for (int i = 0; i <= steps; i++)
			{
				points.push_back(steps == 0 ? to : from + (to - from) * ((float)i / (float)steps));
			}
			return points;
		}

		// The tiles on the map whose centres lie within radius of a map point: a round brush (at least the tile under the
		// point). land: per tile, 0 where painting isn't allowed (the sea); empty allows every tile.
		static std::vector<glm::ivec2> Stamp(const glm::vec2& point, float radius, int width, int height,
			const std::vector<uint8_t>& land = {})
		{
			std::vector<glm::ivec2> tiles;
			const int reach = (int)std::ceil(radius) + 1;
			const glm::ivec2 under((int)std::floor(point.x), (int)std::floor(point.y));
			for (int y = under.y - reach; y <= under.y + reach; y++)
			{
				for (int x = under.x - reach; x <= under.x + reach; x++)
				{
					const glm::vec2 offset = glm::vec2((float)x + 0.5f, (float)y + 0.5f) - point;
					const bool inside = glm::dot(offset, offset) <= radius * radius || (x == under.x && y == under.y);
					const bool onMap = x >= 0 && y >= 0 && x < width && y < height;
					const bool allowed = onMap && (land.empty() || land[(size_t)y * (size_t)width + (size_t)x] != 0);
					if (inside && allowed)
					{
						tiles.push_back(glm::ivec2(x, y));
					}
				}
			}
			return tiles;
		}

		// The tile ids a run can paint from the start (StartsUnlocked), ascending, each once.
		static std::vector<uint8_t> StartingBrushes(const std::vector<TerrainTypeDefinition>& types)
		{
			std::vector<bool> unlocked(256, false);
			for (const TerrainTypeDefinition& type : types)
			{
				if (type.StartsUnlocked && type.TileId >= 0 && type.TileId <= 255)
				{
					unlocked[(size_t)type.TileId] = true;
				}
			}
			std::vector<uint8_t> brushes;
			for (int id = 0; id < 256; id++)
			{
				if (unlocked[(size_t)id])
				{
					brushes.push_back((uint8_t)id);
				}
			}
			return brushes;
		}
	};
}
