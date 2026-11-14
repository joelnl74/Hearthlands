#pragma once

#include "Rules/WildlifeRules.h"
#include "State/HeightMap.h"
#include "State/Hydrology.h"
#include "State/TileMap.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Hydro
{
	// Walking without pathfinding (SettlersView): a walker heads for its spot and, where water or a step too steep is in
	// the way, tries headings turned to one side, then the other, keeping the side that worked. Pure.
	class WalkRules
	{
	public:
		struct Ground
		{
			const TileMap* Map = nullptr;
			const Hydrology* Water = nullptr;   // nullptr: every tile is land
			const HeightMap* Heights = nullptr; // nullptr or not the map's size: flat
			int Climb = 1;                      // height levels a step from one tile to the next goes up or down, at most
		};

		// Whether a walker can stand on (x, y): in the map, not water or a riverbed.
		static bool Standable(const Ground& ground, int x, int y)
		{
			if (ground.Map == nullptr || ground.Map->InBounds(x, y) == false)
			{
				return false;
			}
			return ground.Water == nullptr || WildlifeRules::Walkable(ground.Map->Get(x, y), *ground.Water);
		}

		// Whether a walker on (fromX, fromY) can step onto (x, y): standable and no steeper than Climb. One on a tile it can't
		// stand on (the player flooded it) may step onto any standable tile, to get out.
		static bool Open(const Ground& ground, int fromX, int fromY, int x, int y)
		{
			if (Standable(ground, x, y) == false)
			{
				return false;
			}
			const bool raised = ground.Heights != nullptr && ground.Heights->GetLevels().size() == ground.Map->GetTiles().size();
			if (raised == false || Standable(ground, fromX, fromY) == false)
			{
				return true;
			}
			return std::abs((int)ground.Heights->Get(x, y) - (int)ground.Heights->Get(fromX, fromY)) <= std::max(0, ground.Climb);
		}

		// Whether the way from `from` along `direction` (a unit vector) is open for `ahead` tiles, looked at every half tile;
		// a diagonal step needs both tiles beside it open (no cutting a corner of water).
		static bool Clear(const Ground& ground, const glm::vec2& from, const glm::vec2& direction, float ahead)
		{
			int x = (int)std::floor(from.x);
			int y = (int)std::floor(from.y);
			const int steps = std::max(1, (int)std::ceil(ahead / Probe));
			for (int i = 1; i <= steps; i++)
			{
				const glm::vec2 at = from + direction * std::min(ahead, (float)i * Probe);
				const int nx = (int)std::floor(at.x);
				const int ny = (int)std::floor(at.y);
				if ((nx != x || ny != y) && Open(ground, x, y, nx, ny) == false)
				{
					return false;
				}
				if (nx != x && ny != y && (Open(ground, x, y, nx, y) == false || Open(ground, x, y, x, ny) == false))
				{
					return false;
				}
				x = nx;
				y = ny;
			}
			return true;
		}

		// The heading (a unit vector) from `from` to `to`: straight where the next `ahead` tiles are clear, else turned in
		// steps of Turn degrees (up to MostTurns) to `side` (1 left, -1 right), then to the other. `side` keeps the side that
		// worked, so a walker follows a shore round its end. Zero: boxed in, or already there.
		static glm::vec2 Heading(const Ground& ground, const glm::vec2& from, const glm::vec2& to, int& side, float ahead = 0.75f)
		{
			const glm::vec2 way = to - from;
			const float distance = std::sqrt(way.x * way.x + way.y * way.y);
			if (distance < 1e-4f)
			{
				return glm::vec2(0.0f);
			}
			const glm::vec2 straight = way / distance;
			const float look = std::min(ahead, distance);
			if (Clear(ground, from, straight, look))
			{
				return straight;
			}
			side = side < 0 ? -1 : 1;
			for (int pass = 0; pass < 2; pass++)
			{
				for (int step = 1; step <= MostTurns; step++)
				{
					const int turn = pass == 0 ? side : -side;
					const glm::vec2 heading = Turned(straight, (float)(turn * step) * Turn);
					if (Clear(ground, from, heading, ahead))
					{
						side = turn;
						return heading;
					}
				}
			}
			return glm::vec2(0.0f);
		}

		// `direction` turned by `degrees` (positive: left on the map, from +x towards +y).
		static glm::vec2 Turned(const glm::vec2& direction, float degrees)
		{
			const float radians = degrees * 0.017453293f;
			const float c = std::cos(radians);
			const float s = std::sin(radians);
			return glm::vec2(direction.x * c - direction.y * s, direction.x * s + direction.y * c);
		}

		static constexpr float Probe = 0.5f;  // tiles between the points looked at ahead
		static constexpr float Turn = 35.0f;  // degrees a turned heading steps
		static constexpr int MostTurns = 4;   // ... at most (140 degrees each way)
	};
}
