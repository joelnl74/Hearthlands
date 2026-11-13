// Tools/Tests/SettlementAreaTest.cpp
// The area that grows with a settlement's level (SettlementRules::Reach, HomeReach; the survey, felling, farmland reach)
// and walking without pathfinding (WalkRules: round water and steep steps). Run from TestGameRules.

#include "hypch.h"

#include "Rules/FarmRules.h"
#include "Rules/ObjectRules.h"
#include "Rules/SettlementRules.h"
#include "Rules/WalkRules.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace Hydro
{
	static int s_AreaFailures = 0;

	static void AreaCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_AreaFailures++;
			std::printf("FAIL  SettlementAreaTest.cpp line %d: %s\n", line, what);
		}
	}

#define AREA_CHECK(condition) AreaCheck((condition), #condition, __LINE__, checks)

	static ObjectDefinition MakeAreaObject(int id, ObjectKind kind, float food, float wood)
	{
		ObjectDefinition object;
		object.Name = "Object " + std::to_string(id);
		object.ObjectId = id;
		object.Kind = kind;
		object.Food = food;
		object.Wood = wood;
		return object;
	}

	// A 40 x 40 grass map round a centre at (20, 20): bushes 8 tiles east, trees on a ring 7 to 9 tiles out to the west,
	// a hut 5 tiles north.
	struct AreaWorld
	{
		TileMap Map{ 40, 40, 2 };
		Hydrology Water;
		ObjectLayer Objects;
		std::vector<ObjectDefinition> Definitions = { MakeAreaObject(1, ObjectKind::Shrub, 1.0f, 0.0f),
			MakeAreaObject(5, ObjectKind::Tree, 0.0f, 3.0f), MakeAreaObject(60, ObjectKind::Building, 0.0f, 0.0f),
			MakeAreaObject(61, ObjectKind::Building, 0.0f, 0.0f) };
		std::vector<int> ById;
		std::vector<uint8_t> Land;

		AreaWorld()
		{
			Objects.Reset(40 * 40);
			Objects.Set((size_t)(20 * 40 + 20), 60);
			for (int y = 19; y <= 21; y++)
			{
				Objects.Set((size_t)(y * 40 + 28), 1);
			}
			for (int y = 0; y < 40; y++)
			{
				for (int x = 0; x < 20; x++)
				{
					const int d = (x - 20) * (x - 20) + (y - 20) * (y - 20);
					if (d >= 49 && d <= 81)
					{
						Objects.Set((size_t)(y * 40 + x), 5);
					}
				}
			}
			Objects.Set((size_t)(15 * 40 + 20), 61);
			ById = ObjectRules::IndexById(Definitions);
			Land.assign(40 * 40, 1);
		}

		SettlementRules::World View()
		{
			SettlementRules::World world;
			world.Map = &Map;
			world.Water = &Water;
			world.Objects = &Objects;
			world.Definitions = &Definitions;
			world.ById = &ById;
			world.Land = &Land;
			world.FireObjectId = 60;
			world.FieldTileId = 7;
			return world;
		}
	};

	// Radius 6; a camp (the people's Radius, shelters within 4), a hamlet (10, shelters within 5), a village (14).
	static PeopleDefinition MakeAreaPeople()
	{
		PeopleDefinition people;
		people.Name = "Humans";
		people.PeopleId = 1;
		people.Radius = 6;
		people.FarmTileId = 7;
		people.FarmFromLevel = 1;
		people.CropObjectIds = { 72, 73, 74 };
		PeopleLevel camp;
		camp.Name = "Camp";
		camp.ShelterObjectId = 61;
		PeopleLevel hamlet = camp;
		hamlet.Name = "Hamlet";
		hamlet.Radius = 10;
		hamlet.ShelterReach = 5;
		PeopleLevel village = hamlet;
		village.Name = "Village";
		village.Radius = 14;
		village.ShelterReach = 6;
		people.Levels = { camp, hamlet, village };
		return people;
	}

	static void TestReachByLevel(int& checks)
	{
		const PeopleDefinition people = MakeAreaPeople();
		AREA_CHECK(SettlementRules::Reach(people, 0) == 6);
		AREA_CHECK(SettlementRules::Reach(people, 1) == 10);
		AREA_CHECK(SettlementRules::Reach(people, 2) == 14);
		AREA_CHECK(SettlementRules::Reach(people, 9) == 14);
		AREA_CHECK(SettlementRules::HomeReach(people, 0) == SettlementRules::ShelterReach);
		AREA_CHECK(SettlementRules::HomeReach(people, 1) == 5);
		PeopleDefinition plain = people;
		plain.Levels.clear();
		AREA_CHECK(SettlementRules::Reach(plain, 3) == 6);

		// The survey: the bushes 8 tiles out and the hut 5 tiles out count only from the hamlet on.
		AreaWorld world;
		const SettlementRules::Survey camp = SettlementRules::Look(world.View(), people, 20, 20, SettlementRules::Reach(people, 0),
			SettlementRules::HomeReach(people, 0));
		const SettlementRules::Survey hamlet = SettlementRules::Look(world.View(), people, 20, 20,
			SettlementRules::Reach(people, 1), SettlementRules::HomeReach(people, 1));
		AREA_CHECK(camp.PlantFood == 0.0f && camp.Trees == 0);
		AREA_CHECK(hamlet.PlantFood > 2.5f && hamlet.Trees > 20);
		AREA_CHECK(camp.Shelters == 0 && hamlet.Shelters == 1);
		AREA_CHECK(SettlementRules::Look(world.View(), people, 20, 20).PlantFood == 0.0f); // to settle: the people's Radius

		// Felling: no tree within a camp's 6 tiles, the ring's trees within a hamlet's 10.
		Settlement settlement;
		settlement.X = 20;
		settlement.Y = 20;
		Random random(7);
		AREA_CHECK(SettlementRules::Fell(world.View(), people, settlement, random) == false);
		settlement.Level = 1;
		AREA_CHECK(SettlementRules::Fell(world.View(), people, settlement, random) && settlement.Wood == 3.0f);

		// Shelters go up within the level's reach: a village's within 6 tiles, and some beyond a camp's 4.
		AreaWorld open;
		int beyond = 0;
		int outside = 0;
		for (int i = 0; i < 40; i++)
		{
			open.Objects.Reset(40 * 40);
			open.Objects.Set((size_t)(20 * 40 + 20), 60);
			if (SettlementRules::Build(open.View(), 61, settlement, random, 6))
			{
				for (size_t index = 0; index < open.Objects.GetIds().size(); index++)
				{
					const int dx = (int)(index % 40) - 20;
					const int dy = (int)(index / 40) - 20;
					const int d = dx * dx + dy * dy;
					beyond += open.Objects.Get(index) == 61 && d > 16 ? 1 : 0;
					outside += open.Objects.Get(index) == 61 && d > 36 ? 1 : 0;
				}
			}
		}
		AREA_CHECK(beyond > 0 && outside == 0);
	}

	static void TestFarmlandReachByLevel(int& checks)
	{
		const PeopleDefinition people = MakeAreaPeople();
		SettlementState state;
		Settlement hamlet;
		hamlet.Id = 1;
		hamlet.X = 20;
		hamlet.Y = 20;
		hamlet.Level = 1;
		state.List.push_back(hamlet);
		std::vector<uint8_t> reach;
		FarmRules::Reach(state, { people }, 7, 40, 40, reach);
		AREA_CHECK(reach[(size_t)(20 * 40 + 30)] == 1);  // 10 tiles out
		AREA_CHECK(reach[(size_t)(20 * 40 + 33)] == 0);  // 13 tiles out
		state.List[0].Level = 2;
		FarmRules::Reach(state, { people }, 7, 40, 40, reach);
		AREA_CHECK(reach[(size_t)(20 * 40 + 33)] == 1);
	}

	// Walks from `from` towards `to` in tenth-tile steps the way SettlersView does; true when it gets there. `wet` counts the
	// steps that ended on a tile it can't stand on.
	static bool Walk(const WalkRules::Ground& ground, glm::vec2 from, const glm::vec2& to, int& wet, int steps = 800)
	{
		int side = 1;
		for (int i = 0; i < steps; i++)
		{
			const glm::vec2 way = to - from;
			if (std::sqrt(way.x * way.x + way.y * way.y) <= 0.1f)
			{
				return true;
			}
			const glm::vec2 heading = WalkRules::Heading(ground, from, to, side);
			if (heading.x == 0.0f && heading.y == 0.0f)
			{
				return false;
			}
			from += heading * 0.1f;
			wet += WalkRules::Standable(ground, (int)std::floor(from.x), (int)std::floor(from.y)) ? 0 : 1;
		}
		return false;
	}

	static void TestWalking(int& checks)
	{
		TileMap map(24, 24, 2);
		Hydrology water;
		WalkRules::Ground ground;
		ground.Map = &map;
		ground.Water = &water;

		const glm::vec2 turned = WalkRules::Turned(glm::vec2(1.0f, 0.0f), 90.0f);
		AREA_CHECK(std::fabs(turned.x) < 1e-4f && std::fabs(turned.y - 1.0f) < 1e-4f);
		int side = 1;
		const glm::vec2 straight = WalkRules::Heading(ground, glm::vec2(2.5f, 10.5f), glm::vec2(17.5f, 10.5f), side);
		AREA_CHECK(straight.x == 1.0f && straight.y == 0.0f);

		// A river across the way (x 11 and 12, y 4 to 16): they walk round its end and never step in.
		for (int y = 4; y <= 16; y++)
		{
			map.Set(11, y, water.WaterTileId);
			map.Set(12, y, water.WaterTileId);
		}
		side = 1;
		const glm::vec2 around = WalkRules::Heading(ground, glm::vec2(10.6f, 10.5f), glm::vec2(15.5f, 10.5f), side);
		AREA_CHECK((around.x != 1.0f || around.y != 0.0f) && (around.x != 0.0f || around.y != 0.0f));
		int wet = 0;
		AREA_CHECK(Walk(ground, glm::vec2(8.5f, 10.5f), glm::vec2(15.5f, 10.5f), wet));
		AREA_CHECK(wet == 0);
		wet = 0;
		AREA_CHECK(Walk(ground, glm::vec2(15.5f, 6.5f), glm::vec2(8.5f, 12.5f), wet) && wet == 0);

		// Boxed in by water: no heading; one standing in water (the player flooded it) may step out.
		TileMap pond(12, 12, 2);
		for (int y = 3; y <= 7; y++)
		{
			for (int x = 3; x <= 7; x++)
			{
				pond.Set(x, y, (x == 5 && y == 5) ? (uint8_t)2 : water.WaterTileId);
			}
		}
		ground.Map = &pond;
		side = 1;
		const glm::vec2 boxed = WalkRules::Heading(ground, glm::vec2(5.5f, 5.5f), glm::vec2(10.5f, 5.5f), side);
		AREA_CHECK(boxed.x == 0.0f && boxed.y == 0.0f);
		AREA_CHECK(WalkRules::Open(ground, 4, 5, 2, 5) && WalkRules::Open(ground, 5, 5, 4, 5) == false);

		// A cliff of 3 levels east of x 9: too steep for a Climb of 1 (they walk along it), fine for 3.
		TileMap flat(20, 20, 2);
		HeightMap heights(20, 20, 0);
		for (int y = 0; y < 20; y++)
		{
			for (int x = 10; x < 20; x++)
			{
				heights.Set(x, y, 3);
			}
		}
		ground.Map = &flat;
		ground.Heights = &heights;
		ground.Climb = 1;
		AREA_CHECK(WalkRules::Clear(ground, glm::vec2(9.5f, 5.5f), glm::vec2(1.0f, 0.0f), 0.75f) == false);
		wet = 0;
		AREA_CHECK(Walk(ground, glm::vec2(5.5f, 5.5f), glm::vec2(14.5f, 5.5f), wet, 400) == false);
		ground.Climb = 3;
		AREA_CHECK(WalkRules::Clear(ground, glm::vec2(9.5f, 5.5f), glm::vec2(1.0f, 0.0f), 0.75f));
		wet = 0;
		AREA_CHECK(Walk(ground, glm::vec2(5.5f, 5.5f), glm::vec2(14.5f, 5.5f), wet));
	}

	int TestSettlementArea(int& checks)
	{
		std::printf("Game rules: settlement area, WalkRules\n");
		s_AreaFailures = 0;
		TestReachByLevel(checks);
		TestFarmlandReachByLevel(checks);
		TestWalking(checks);
		return s_AreaFailures;
	}
}
