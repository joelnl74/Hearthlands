// Tools/Tests/WildlifeRulesTest.cpp
// The wild animals' numbers (WildlifeRules) and their milestones (TalentRules) on small maps; run from TestGameRules.

#include "hypch.h"

#include "Rules/TalentRules.h"
#include "Rules/WildlifeRules.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace Hydro
{
	static int s_WildlifeFailures = 0;

	static void WildlifeCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_WildlifeFailures++;
			std::printf("FAIL  WildlifeRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define WILDLIFE_CHECK(condition) WildlifeCheck((condition), #condition, __LINE__, checks)

	static AnimalDefinition MakeAnimal(int id, AnimalKind kind, std::vector<int> tiles, float density)
	{
		AnimalDefinition animal;
		animal.Name = "Animal " + std::to_string(id);
		animal.AnimalId = id;
		animal.Kind = kind;
		animal.TileIds = tiles;
		animal.Density = density;
		animal.SpreadPerDay = 0.0f;
		animal.ArrivePerDay = 0.0f;
		return animal;
	}

	// A 32 x 16 map, two regions of 16: the left grass (a tree on every 4th tile), the right dirt with a pond.
	struct WildWorld
	{
		TileMap Map{ 32, 16, 1 };
		HeightMap Heights{ 32, 16 };
		Hydrology Water;
		ObjectLayer Objects;
		std::vector<uint8_t> Cover;

		WildWorld()
		{
			Water.Moisture.assign(32 * 16, 128);
			Objects.Reset(32 * 16);
			for (int y = 0; y < 16; y++)
			{
				for (int x = 0; x < 16; x++)
				{
					Map.Set(x, y, 2);
					if ((x + y * 16) % 4 == 0)
					{
						Objects.Set((size_t)(y * 32 + x), 7);
					}
				}
			}
			for (int y = 4; y < 8; y++)
			{
				for (int x = 20; x < 24; x++)
				{
					Map.Set(x, y, 3);
				}
			}
			ObjectDefinition tree;
			tree.ObjectId = 7;
			tree.Kind = ObjectKind::Tree;
			ObjectDefinition reed;
			reed.ObjectId = 8;
			reed.Kind = ObjectKind::Ground;
			Cover = WildlifeRules::CoverOf({ tree, reed });
		}
	};

	static bool Close(float a, float b, float within)
	{
		return std::fabs(a - b) <= within;
	}

	int TestWildlifeRules(int& checks)
	{
		std::printf("Game rules: WildlifeRules\n");
		WildWorld world;
		WILDLIFE_CHECK(world.Cover[7] == 1 && world.Cover[8] == 0 && world.Cover[0] == 0);

		// Regions: 16 tiles across; the edge regions may be smaller.
		WildlifeState state = WildlifeRules::Start(40, 20, 3);
		WILDLIFE_CHECK(state.RegionsX == 3 && state.RegionsY == 2 && state.GetRegionCount() == 6);
		WILDLIFE_CHECK(state.Population.size() == 3 && state.Population[2].size() == 6 && state.Capacity[0].size() == 6);
		WILDLIFE_CHECK(state.RegionOf(17, 3) == 1 && state.RegionOf(39, 19) == 5 && state.RegionOf(48, 0) == 6);
		int x0 = 0;
		int y0 = 0;
		int x1 = 0;
		int y1 = 0;
		WildlifeRules::Bounds(state, 5, 40, 20, x0, y0, x1, y1);
		WILDLIFE_CHECK(x0 == 32 && y0 == 16 && x1 == 39 && y1 == 19);

		// Suits: the tile, its moisture and height; any land, or the water for fish.
		AnimalDefinition deer = MakeAnimal(1, AnimalKind::Walker, { 2 }, 0.05f);
		deer.MinMoisture = 0.2f;
		deer.MaxLevel = 3;
		WILDLIFE_CHECK(WildlifeRules::Suits(deer, 2, 128, 0, world.Water));
		WILDLIFE_CHECK(WildlifeRules::Suits(deer, 1, 128, 0, world.Water) == false);
		WILDLIFE_CHECK(WildlifeRules::Suits(deer, 2, 20, 0, world.Water) == false);
		WILDLIFE_CHECK(WildlifeRules::Suits(deer, 2, 128, 4, world.Water) == false);
		const AnimalDefinition fish = MakeAnimal(20, AnimalKind::Fish, {}, 0.1f);
		const AnimalDefinition crow = MakeAnimal(16, AnimalKind::Bird, {}, 0.01f);
		WILDLIFE_CHECK(WildlifeRules::Suits(fish, 3, 0, 0, world.Water) && WildlifeRules::Suits(fish, 1, 0, 0, world.Water) == false);
		WILDLIFE_CHECK(WildlifeRules::Suits(crow, 1, 0, 0, world.Water) && WildlifeRules::Suits(crow, 3, 0, 0, world.Water) == false);
		WILDLIFE_CHECK(WildlifeRules::Suits(crow, 5, 0, 0, world.Water) == false && WildlifeRules::Walkable(5, world.Water) == false);

		// Measure: deer on the grass only, fish in the pond; cover and water limits.
		AnimalDefinition boar = MakeAnimal(2, AnimalKind::Walker, { 2, 1 }, 0.1f);
		boar.MinCover = 0.3f; // a tree on a quarter of the grass isn't enough
		AnimalDefinition egret = MakeAnimal(15, AnimalKind::Bird, {}, 0.01f);
		egret.MinWater = 0.05f;
		std::vector<AnimalDefinition> animals = { deer, fish, boar, egret };
		state = WildlifeRules::Start(32, 16, animals.size());
		WildlifeRules::Measure(state, animals, world.Map, world.Water, world.Heights, world.Objects, world.Cover, 0);
		WildlifeRules::Measure(state, animals, world.Map, world.Water, world.Heights, world.Objects, world.Cover, 1);
		WILDLIFE_CHECK(Close(state.Capacity[0][0], 256.0f * 0.05f, 1e-3f) && state.Capacity[0][1] == 0.0f);
		WILDLIFE_CHECK(state.Capacity[1][0] == 0.0f && Close(state.Capacity[1][1], 16.0f * 0.1f, 1e-3f));
		WILDLIFE_CHECK(state.Capacity[2][0] == 0.0f && state.Capacity[2][1] == 0.0f);
		WILDLIFE_CHECK(state.Capacity[3][0] == 0.0f && Close(state.Capacity[3][1], 240.0f * 0.01f, 1e-3f));
		const WildlifeRules::Survey survey = WildlifeRules::Look(world.Map, world.Water, world.Objects, world.Cover, 0, 0, 15, 15);
		WILDLIFE_CHECK(survey.Tiles == 256 && survey.Land == 256 && survey.Covered == 64 && survey.Water == 0);

		// Grow: towards what the land holds, never past it; above it they leave.
		Random random(7);
		WildlifeRules::Factors factors;
		state.Population[0][0] = 2.0f;
		for (int day = 0; day < 60; day++)
		{
			WildlifeRules::Grow(state, animals, 0, 1.0f, factors, random);
		}
		WILDLIFE_CHECK(state.Population[0][0] > 12.0f && state.Population[0][0] <= 12.8f + 1e-4f);
		state.Population[0][0] = 30.0f;
		WildlifeRules::Grow(state, animals, 0, 1.0f, factors, random);
		WILDLIFE_CHECK(Close(state.Population[0][0], 30.0f - (30.0f - 12.8f) * WildlifeRules::LeavePerDay, 1e-3f));
		factors.Capacity = 2.0f; // a talent: the land holds twice as many
		const float before = state.Population[0][0];
		WildlifeRules::Grow(state, animals, 0, 1.0f, factors, random);
		WILDLIFE_CHECK(state.Population[0][0] > before);
		factors.Capacity = 1.0f;

		// No births in a region that has none; newcomers from outside when ArrivePerDay says so.
		WILDLIFE_CHECK(state.Population[1][1] == 0.0f);
		WildlifeRules::Grow(state, animals, 1, 5.0f, factors, random);
		WILDLIFE_CHECK(state.Population[1][1] == 0.0f);
		animals[1].ArrivePerDay = 1.0f;
		animals[1].GroupMin = 1;
		animals[1].GroupMax = 1;
		WildlifeRules::Grow(state, animals, 1, 1.0f, factors, random);
		WILDLIFE_CHECK(state.Population[1][1] >= 1.0f);

		// Migrants: in the cold the land holds only the season's share of them.
		animals[0].Migrates = true;
		factors.Migrants = 0.1f;
		WILDLIFE_CHECK(Close(WildlifeRules::Room(state, animals, 0, 0, factors), 1.28f, 1e-3f));
		factors.Migrants = 1.0f;
		animals[0].Migrates = false;

		// Spreading: to a neighbouring region with room.
		animals[1].SpreadPerDay = 0.5f;
		state.Population[1][1] = 1.6f;
		state.Population[1][0] = 0.0f;
		state.Capacity[1][0] = 4.0f;
		animals[1].BirthsPerDay = 0.0f;
		for (int day = 0; day < 32 && state.Population[1][0] == 0.0f; day++)
		{
			WildlifeRules::Grow(state, animals, 1, 1.0f, factors, random); // one neighbour of four is on the map
		}
		WILDLIFE_CHECK(Close(state.Population[1][0] + state.Population[1][1], 1.6f, 1e-3f) && state.Population[1][0] > 0.0f);

		// Hunters live only where their prey feeds them, and eat it.
		AnimalDefinition wolf = MakeAnimal(3, AnimalKind::Walker, {}, 1.0f);
		wolf.PreyId = 1;
		wolf.PreyPerAnimal = 4.0f;
		wolf.EatPerDay = 0.5f;
		std::vector<AnimalDefinition> hunt = { deer, wolf };
		WildlifeState hunting = WildlifeRules::Start(32, 16, hunt.size());
		WildlifeRules::Measure(hunting, hunt, world.Map, world.Water, world.Heights, world.Objects, world.Cover, 0);
		hunting.Population[0][0] = 8.0f;
		WILDLIFE_CHECK(Close(WildlifeRules::Room(hunting, hunt, 1, 0, factors), 2.0f, 1e-4f));
		hunting.Population[1][0] = 2.0f;
		hunt[0].BirthsPerDay = 0.0f;
		WildlifeRules::Grow(hunting, hunt, 0, 1.0f, factors, random);
		WILDLIFE_CHECK(Close(hunting.Population[0][0], 7.0f, 1e-3f));
		hunt[1].PreyId = 99; // prey not in the table: no room
		WILDLIFE_CHECK(WildlifeRules::Room(hunting, hunt, 1, 0, factors) == 0.0f);

		// Fill: a new world holds StartShare of what its land holds.
		WildlifeState filled = WildlifeRules::Start(32, 16, animals.size());
		animals[0].StartShare = 0.5f;
		WildlifeRules::Fill(filled, animals, world.Map, world.Water, world.Heights, world.Objects, world.Cover, factors);
		WILDLIFE_CHECK(Close(filled.Population[0][0], 6.4f, 1e-3f) && filled.Population[0][1] == 0.0f);
		WILDLIFE_CHECK(Close(filled.Total(0), 6.4f, 1e-3f));

		// Update: a few regions a tick, round the map.
		const uint64_t revision = filled.Revision;
		WildlifeRules::Update(filled, animals, world.Map, world.Water, world.Heights, world.Objects, world.Cover, 1, 0.1f, factors, random);
		WILDLIFE_CHECK(filled.NextRegion == 1 && filled.Revision == revision + 1);
		WildlifeRules::Update(filled, animals, world.Map, world.Water, world.Heights, world.Objects, world.Cover, 1, 0.1f, factors, random);
		WILDLIFE_CHECK(filled.NextRegion == 0);

		// Shown: a share of the animals, the hibernating ones out of sight in the cold, at most MaxShown.
		AnimalDefinition badger = MakeAnimal(4, AnimalKind::Walker, {}, 0.1f);
		badger.Hibernates = true;
		badger.MaxShown = 3;
		WILDLIFE_CHECK(WildlifeRules::Shown(badger, 2.0f, 0.0f, 0.0f) == 2);
		WILDLIFE_CHECK(WildlifeRules::Shown(badger, 2.0f, 0.9f, 0.0f) == 0);
		WILDLIFE_CHECK(WildlifeRules::Shown(badger, 10.0f, 0.0f, 0.0f) == 3);
		WILDLIFE_CHECK(WildlifeRules::Shown(fish, 4.0f, 0.9f, 0.5f) == 6); // sight: half again; fish never sleep

		// Milestones: animals of a species, all of them, and the species living.
		filled.Population[1][1] = 3.5f;
		const std::vector<uint32_t> totals = WildlifeRules::Totals(filled, animals);
		WILDLIFE_CHECK(totals[1] == 6 && totals[20] == 3 && totals[15] == 1 && totals[2] == 0);
		TalentRules::Counts counts;
		TalentRules::AddAnimals(counts, totals);
		WILDLIFE_CHECK(counts.AllAnimals == 10 && counts.Species == 3);
		MilestoneDefinition herds;
		herds.Kind = MilestoneKind::Animals;
		herds.TargetId = 1;
		MilestoneDefinition any = herds;
		any.TargetId = -1;
		MilestoneDefinition kinds = any;
		kinds.Kind = MilestoneKind::Species; // TargetId -1: every kind
		WILDLIFE_CHECK(TalentRules::Progress(herds, counts) == 6 && TalentRules::Progress(any, counts) == 10);
		WILDLIFE_CHECK(TalentRules::Progress(kinds, counts) == 3);

		return s_WildlifeFailures;
	}
}
