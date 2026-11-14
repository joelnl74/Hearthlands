#pragma once

#include "Data/BuildingDefinition.h"
#include "Data/PeopleDefinition.h"
#include "Rules/SettlementGround.h"
#include "Rules/WildlifeRules.h"
#include "State/Random.h"
#include "State/SettlementState.h"
#include "State/WildlifeState.h"

#include <algorithm>
#include <cstdint>

namespace Hydro
{
	// Fishing (pure): the fish a people catches (FishAnimalIds, the player's water life) within a settlement's reach (each
	// region by its share inside the square round the centre), the catch that thins them, and the lodge's shore.
	class FishRules
	{
	public:
		using World = SettlementGround::World;

		// The fish they catch living within `radius` of (x, y).
		static float Stock(const World& world, const PeopleDefinition& people, int x, int y, int radius)
		{
			float stock = 0.0f;
			Visit(world, x, y, radius, [&](size_t region, float share)
			{
				for (int animalId : people.FishAnimalIds)
				{
					const int row = WildlifeRules::RowOf(*world.Animals, animalId);
					stock += row >= 0 ? world.Wildlife->Get((size_t)row, region) * share : 0.0f;
				}
			});
			return stock;
		}

		// Catches up to `want` fish within `radius` of (x, y), at most a quarter of each kind in a region's share a time.
		// Returns how many they took; the water has that many fewer.
		static float Catch(const World& world, const PeopleDefinition& people, int x, int y, int radius, float want)
		{
			float taken = 0.0f;
			if (want <= 0.0f)
			{
				return taken;
			}
			Visit(world, x, y, radius, [&](size_t region, float share)
			{
				for (int animalId : people.FishAnimalIds)
				{
					const int row = WildlifeRules::RowOf(*world.Animals, animalId);
					if (row < 0 || (size_t)row >= world.Wildlife->Population.size() || taken >= want)
					{
						continue;
					}
					float& living = world.Wildlife->Population[(size_t)row][region];
					const float take = std::min(want - taken, living * share * MostShare);
					living -= take;
					taken += take;
				}
			});
			return taken;
		}

		// Whether (x, y) is land beside water (one of its four neighbours).
		static bool Shore(const World& world, int x, int y)
		{
			const TileMap& map = *world.Map;
			if (map.InBounds(x, y) == false || world.Water == nullptr || map.Get(x, y) == world.Water->WaterTileId)
			{
				return false;
			}
			const int dx[] = { 1, -1, 0, 0 };
			const int dy[] = { 0, 0, 1, -1 };
			for (int i = 0; i < 4; i++)
			{
				const int nx = x + dx[i];
				const int ny = y + dy[i];
				if (map.InBounds(nx, ny) && map.Get(nx, ny) == world.Water->WaterTileId)
				{
					return true;
				}
			}
			return false;
		}

		// Puts up a fishing lodge on a buildable shore tile from the building's RingFrom to `reach` tiles out with no
		// building beside it: of the ones looked at, in the region with the most of their fish. False: no shore with fish.
		static bool PutUp(const World& world, const BuildingDefinition& building, const PeopleDefinition& people,
			const Settlement& settlement, int reach, Random& random)
		{
			const int from = std::max(2, building.RingFrom);
			const int to = std::max(from, reach);
			int bestX = 0;
			int bestY = 0;
			float bestFish = 0.0f;
			for (int tries = 0; tries < 80; tries++)
			{
				const int dx = (int)random.Below((uint32_t)(to * 2 + 1)) - to;
				const int dy = (int)random.Below((uint32_t)(to * 2 + 1)) - to;
				const int distance = dx * dx + dy * dy;
				const int x = settlement.X + dx;
				const int y = settlement.Y + dy;
				const bool inside = distance >= from * from && distance <= to * to;
				if (inside == false || Shore(world, x, y) == false || SettlementGround::Buildable(world, x, y) == false
					|| SettlementGround::BesideBuilding(world, x, y))
				{
					continue;
				}
				const float fish = Stock(world, people, x, y, 0);
				if (fish > bestFish)
				{
					bestFish = fish;
					bestX = x;
					bestY = y;
				}
			}
			if (bestFish <= 0.0f)
			{
				return false;
			}
			world.Objects->Set(SettlementGround::IndexOf(world, bestX, bestY), (uint8_t)building.ObjectId);
			return true;
		}

		static constexpr float MostShare = 0.25f; // of the fish in reach they catch at a time, at most
		static constexpr float LodgeFrom = 2.0f;  // fish within reach from which they want a fishing lodge

	private:
		// Calls `visit(region, share)` for each region the square of `radius` round (x, y) touches, `share` the part of the
		// region inside it (radius 0: the tile's whole region).
		template<typename Visitor>
		static void Visit(const World& world, int x, int y, int radius, Visitor visit)
		{
			if (world.Wildlife == nullptr || world.Animals == nullptr || world.Map == nullptr)
			{
				return;
			}
			const WildlifeState& state = *world.Wildlife;
			const int width = world.Map->GetWidth();
			const int height = world.Map->GetHeight();
			if (radius <= 0)
			{
				const size_t region = state.RegionOf(x, y);
				if (region < state.GetRegionCount())
				{
					visit(region, 1.0f);
				}
				return;
			}
			const int x0 = std::max(0, x - radius);
			const int y0 = std::max(0, y - radius);
			const int x1 = std::min(width - 1, x + radius);
			const int y1 = std::min(height - 1, y + radius);
			if (x0 > x1 || y0 > y1 || state.RegionSize <= 0)
			{
				return;
			}
			for (int ry = y0 / state.RegionSize; ry <= y1 / state.RegionSize && ry < state.RegionsY; ry++)
			{
				for (int rx = x0 / state.RegionSize; rx <= x1 / state.RegionSize && rx < state.RegionsX; rx++)
				{
					const size_t region = (size_t)ry * (size_t)state.RegionsX + (size_t)rx;
					int bx0 = 0;
					int by0 = 0;
					int bx1 = 0;
					int by1 = 0;
					WildlifeRules::Bounds(state, region, width, height, bx0, by0, bx1, by1);
					const int ox = std::min(x1, bx1) - std::max(x0, bx0) + 1;
					const int oy = std::min(y1, by1) - std::max(y0, by0) + 1;
					const int area = (bx1 - bx0 + 1) * (by1 - by0 + 1);
					if (ox > 0 && oy > 0 && area > 0)
					{
						visit(region, (float)(ox * oy) / (float)area);
					}
				}
			}
		}
	};
}
