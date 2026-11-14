#pragma once

#include "Data/AnimalDefinition.h"
#include "Data/BuildingDefinition.h"
#include "Data/ObjectDefinition.h"
#include "Data/PeopleDefinition.h"
#include "Rules/ObjectRules.h"
#include "Rules/SoilRules.h"
#include "Rules/WildlifeRules.h"
#include "State/HeightMap.h"
#include "State/Hydrology.h"
#include "State/ObjectLayer.h"
#include "State/Random.h"
#include "State/SettlementState.h"
#include "State/TileMap.h"
#include "State/WildlifeState.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace Hydro
{
	// A settlement's ground: the world as its settlers see it, its levels and the area each uses (Reach, HomeReach), and
	// what the villagers do on it: hunt, fell, plant, build and rebuild. SettlementRules builds on it. Pure.
	class SettlementGround
	{
	public:
		// The world as the settlers see it.
		struct World
		{
			const TileMap* Map = nullptr;
			const Hydrology* Water = nullptr;
			ObjectLayer* Objects = nullptr;
			const std::vector<ObjectDefinition>* Definitions = nullptr;
			const std::vector<int>* ById = nullptr;   // object id to row (ObjectRules::IndexById)
			const std::vector<uint8_t>* Land = nullptr; // 1 inside the coast; empty: all
			WildlifeState* Wildlife = nullptr;
			const std::vector<AnimalDefinition>* Animals = nullptr;
			int FireObjectId = -1;                     // the centre of a people without their own CentreObjectId
			const HeightMap* Heights = nullptr;        // for centres on high land (CentreMinLevel); nullptr flat
			SoilState* Soil = nullptr;                 // farming: field fertility, worn by harvests; nullptr plain
			const SoilRules::Settings* SoilSettings = nullptr;
			int FieldTileId = -1;                      // the farmland the player paints: nothing is built on it
			float Harvest = 1.0f;                      // the season (SeasonState): wild food, times this ...
			float Growth = 1.0f;                       // ... crops grow, times this
			bool Sowing = true;                        // ... farmers sow
			bool Frost = false;                        // ... crops wither
			const std::vector<BuildingDefinition>* Buildings = nullptr; // what peoples build besides shelters; nullptr none
			const std::vector<int>* Talents = nullptr; // the owned talents (TalentIds): which buildings are known
		};

		static const ObjectDefinition* Object(const World& world, uint8_t id)
		{
			const int row = id != 0 && id < world.ById->size() ? (*world.ById)[id] : -1;
			return row >= 0 ? &(*world.Definitions)[(size_t)row] : nullptr;
		}

		static size_t IndexOf(const World& world, int x, int y)
		{
			return (size_t)y * (size_t)world.Map->GetWidth() + (size_t)x;
		}

		// A tile a centre or a shelter can go on: land inside the coast, not water or riverbed, empty or under a ground plant.
		static bool Buildable(const World& world, int x, int y)
		{
			const TileMap& map = *world.Map;
			if (map.InBounds(x, y) == false)
			{
				return false;
			}
			const size_t index = IndexOf(world, x, y);
			const bool land = world.Land->empty() || (index < world.Land->size() && (*world.Land)[index] != 0);
			const ObjectDefinition* object = Object(world, world.Objects->Get(index));
			const bool free = world.Objects->Get(index) == 0 || (object != nullptr && object->Kind == ObjectKind::Ground);
			const bool field = world.FieldTileId >= 0 && map.Get(x, y) == (uint8_t)world.FieldTileId;
			return land && WildlifeRules::Walkable(map.Get(x, y), *world.Water) && free && field == false;
		}

		// The object at a people's settlements' centre: their CentreObjectId, else the world's fire.
		static int CentreObject(const World& world, const PeopleDefinition& people)
		{
			return people.CentreObjectId > 0 ? people.CentreObjectId : world.FireObjectId;
		}

		// Whether (x, y) is high enough for the people's centre (CentreMinLevel: a mine in the hills).
		static bool HighEnough(const World& world, const PeopleDefinition& people, int x, int y)
		{
			const bool sized = world.Heights != nullptr && world.Heights->GetLevels().size() == world.Map->GetTiles().size();
			return people.CentreMinLevel <= 0 || (sized && world.Heights->Get(x, y) >= people.CentreMinLevel);
		}

		// The people's level `level` (clamped to their Levels); a plain one when they have none.
		static const PeopleLevel& LevelOf(const PeopleDefinition& people, int level)
		{
			static const PeopleLevel plain;
			if (people.Levels.empty())
			{
				return plain;
			}
			return people.Levels[(size_t)std::clamp(level, 0, (int)people.Levels.size() - 1)];
		}

		static bool HasNextLevel(const PeopleDefinition& people, int level)
		{
			return level + 1 < (int)people.Levels.size();
		}

		// The tiles round the centre a settlement of `level` uses (food, wood, fields, where its villagers walk): the level's
		// Radius, else the people's. It grows with the level.
		static int Reach(const PeopleDefinition& people, int level)
		{
			const int radius = LevelOf(people, level).Radius;
			return std::max(1, radius > 0 ? radius : people.Radius);
		}

		// The tiles from the centre a settlement of `level` puts up its shelters (and counts them): the level's, else 4.
		static int HomeReach(const PeopleDefinition& people, int level)
		{
			const int reach = LevelOf(people, level).ShelterReach;
			return reach > 0 ? reach : ShelterReach;
		}

		// Hunts up to `want` animals from the centre's region, the ones they hunt in turn (at most a quarter of each a time).
		// Returns how many they took; the region has that many fewer.
		static float Hunt(const World& world, const PeopleDefinition& people, const Settlement& settlement, float want)
		{
			if (world.Wildlife == nullptr || world.Animals == nullptr || want <= 0.0f)
			{
				return 0.0f;
			}
			const size_t region = world.Wildlife->RegionOf(settlement.X, settlement.Y);
			float taken = 0.0f;
			for (int animalId : people.HuntAnimalIds)
			{
				const int row = WildlifeRules::RowOf(*world.Animals, animalId);
				if (row < 0 || (size_t)row >= world.Wildlife->Population.size() || region >= world.Wildlife->GetRegionCount())
				{
					continue;
				}
				float& living = world.Wildlife->Population[(size_t)row][region];
				const float take = std::min(want - taken, living * 0.25f);
				living -= take;
				taken += take;
			}
			return taken;
		}

		// Fells a tree within the settlement's reach (of a few looked at, the one with the most wood): the wood goes to the store.
		static bool Fell(const World& world, const PeopleDefinition& people, Settlement& settlement, Random& random)
		{
			const TileMap& map = *world.Map;
			const int radius = Reach(people, settlement.Level);
			size_t best = ~(size_t)0;
			float bestWood = 0.0f;
			for (int tries = 0; tries < 48; tries++)
			{
				const int dx = (int)random.Below((uint32_t)(radius * 2 + 1)) - radius;
				const int dy = (int)random.Below((uint32_t)(radius * 2 + 1)) - radius;
				const int x = settlement.X + dx;
				const int y = settlement.Y + dy;
				if (dx * dx + dy * dy > radius * radius || map.InBounds(x, y) == false)
				{
					continue;
				}
				const size_t index = IndexOf(world, x, y);
				const ObjectDefinition* object = Object(world, world.Objects->Get(index));
				if (object != nullptr && object->Kind == ObjectKind::Tree && object->Wood > bestWood)
				{
					best = index;
					bestWood = object->Wood;
				}
			}
			if (best == ~(size_t)0)
			{
				return false;
			}
			world.Objects->Set(best, 0);
			settlement.Wood += bestWood;
			return true;
		}

		// The level (index) whose shelter the object is, of the people's; -1 none of theirs.
		static int ShelterLevel(const PeopleDefinition& people, int objectId)
		{
			for (size_t level = 0; level < people.Levels.size(); level++)
			{
				if (people.Levels[level].ShelterObjectId > 0 && people.Levels[level].ShelterObjectId == objectId)
				{
					return (int)level;
				}
			}
			return -1;
		}

		// Whether the object is one of the people's shelters from below the settlement's level, not the level's own.
		static bool Older(const PeopleDefinition& people, const Settlement& settlement, int objectId)
		{
			const int level = ShelterLevel(people, objectId);
			return level >= 0 && level < settlement.Level && objectId != LevelOf(people, settlement.Level).ShelterObjectId;
		}

		// The settlement's older shelters within its HomeReach (tents in a hamlet).
		static int CountOld(const World& world, const PeopleDefinition& people, const Settlement& settlement)
		{
			int count = 0;
			const int home = HomeReach(people, settlement.Level);
			for (int dy = -home; dy <= home; dy++)
			{
				for (int dx = -home; dx <= home; dx++)
				{
					const int x = settlement.X + dx;
					const int y = settlement.Y + dy;
					const bool inside = dx * dx + dy * dy <= home * home && world.Map->InBounds(x, y);
					count += inside && Older(people, settlement, world.Objects->Get(IndexOf(world, x, y))) ? 1 : 0;
				}
			}
			return count;
		}

		// Rebuilds the older shelter nearest the centre as the level's (a tent into a hut). False: none to rebuild.
		static bool Upgrade(const World& world, const PeopleDefinition& people, const Settlement& settlement)
		{
			const int shelter = LevelOf(people, settlement.Level).ShelterObjectId;
			if (shelter <= 0 || shelter > 255)
			{
				return false;
			}
			for (int ring = 1; ring <= HomeReach(people, settlement.Level); ring++)
			{
				for (int dy = -ring; dy <= ring; dy++)
				{
					for (int dx = -ring; dx <= ring; dx++)
					{
						const int x = settlement.X + dx;
						const int y = settlement.Y + dy;
						const bool edge = std::abs(dx) == ring || std::abs(dy) == ring;
						if (edge && world.Map->InBounds(x, y) && Older(people, settlement, world.Objects->Get(IndexOf(world, x, y))))
						{
							world.Objects->Set(IndexOf(world, x, y), (uint8_t)shelter);
							return true;
						}
					}
				}
			}
			return false;
		}

		// Whether the people plant saplings (foresters).
		static bool Plants(const PeopleDefinition& people)
		{
			return people.PlantPerDay > 0.0f && people.ForesterShare > 0.0f && people.PlantObjectIds.empty() == false;
		}

		// Plants one of the people's saplings on an empty tile within the settlement's reach (beyond the shelters) where it can
		// grow.
		static bool Plant(const World& world, const PeopleDefinition& people, const Settlement& settlement, Random& random)
		{
			if (Plants(people) == false)
			{
				return false;
			}
			const TileMap& map = *world.Map;
			const bool raised = world.Heights != nullptr && world.Heights->GetLevels().size() == map.GetTiles().size();
			const int radius = Reach(people, settlement.Level);
			const int home = HomeReach(people, settlement.Level) + 1;
			for (int tries = 0; tries < 32; tries++)
			{
				const int dx = (int)random.Below((uint32_t)(radius * 2 + 1)) - radius;
				const int dy = (int)random.Below((uint32_t)(radius * 2 + 1)) - radius;
				const int distance = dx * dx + dy * dy;
				const int x = settlement.X + dx;
				const int y = settlement.Y + dy;
				const bool inside = distance <= radius * radius && distance > home * home;
				if (inside == false || map.InBounds(x, y) == false || world.Objects->Get(IndexOf(world, x, y)) != 0)
				{
					continue;
				}
				const int pick = people.PlantObjectIds[(size_t)random.Below((uint32_t)people.PlantObjectIds.size())];
				const ObjectDefinition* sapling = pick > 0 && pick < 256 ? Object(world, (uint8_t)pick) : nullptr;
				if (sapling != nullptr && ObjectRules::Fits(*sapling, map, *world.Water, x, y, raised ? world.Heights : nullptr))
				{
					world.Objects->Set(IndexOf(world, x, y), (uint8_t)pick);
					return true;
				}
			}
			return false;
		}

		// Puts up a building on a buildable tile its RingFrom to RingTo tiles from the centre, with no building beside it.
		static bool PutUp(const World& world, const BuildingDefinition& building, const Settlement& settlement, Random& random)
		{
			const int from = std::max(2, building.RingFrom);
			const int to = std::max(from, building.RingTo);
			for (int tries = 0; tries < 60; tries++)
			{
				const int dx = (int)random.Below((uint32_t)(to * 2 + 1)) - to;
				const int dy = (int)random.Below((uint32_t)(to * 2 + 1)) - to;
				const int distance = dx * dx + dy * dy;
				const int x = settlement.X + dx;
				const int y = settlement.Y + dy;
				if (distance < from * from || distance > to * to || Buildable(world, x, y) == false || BesideBuilding(world, x, y))
				{
					continue;
				}
				world.Objects->Set(IndexOf(world, x, y), (uint8_t)building.ObjectId);
				return true;
			}
			return false;
		}

		// Puts up a shelter 2 to `reach` tiles (the level's HomeReach) from the centre on a buildable tile with no other building
		// beside it.
		static bool Build(const World& world, int shelterObjectId, const Settlement& settlement, Random& random,
			int reach = ShelterReach)
		{
			for (int tries = 0; tries < 40; tries++)
			{
				const int dx = (int)random.Below((uint32_t)(reach * 2 + 1)) - reach;
				const int dy = (int)random.Below((uint32_t)(reach * 2 + 1)) - reach;
				const int distance = dx * dx + dy * dy;
				const int x = settlement.X + dx;
				const int y = settlement.Y + dy;
				const bool outside = distance < 4 || distance > reach * reach;
				if (outside || Buildable(world, x, y) == false || BesideBuilding(world, x, y))
				{
					continue;
				}
				world.Objects->Set(IndexOf(world, x, y), (uint8_t)shelterObjectId);
				return true;
			}
			return false;
		}

		static bool BesideBuilding(const World& world, int x, int y)
		{
			const TileMap& map = *world.Map;
			for (int dy = -1; dy <= 1; dy++)
			{
				for (int dx = -1; dx <= 1; dx++)
				{
					if ((dx != 0 || dy != 0) && map.InBounds(x + dx, y + dy))
					{
						const ObjectDefinition* object = Object(world, world.Objects->Get(IndexOf(world, x + dx, y + dy)));
						if (object != nullptr && object->Kind == ObjectKind::Building)
						{
							return true;
						}
					}
				}
			}
			return false;
		}

		static constexpr int ShelterReach = 4;    // tiles from the centre shelters go up (and are counted) without a level's own
	};
}
