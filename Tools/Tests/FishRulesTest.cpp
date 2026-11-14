// Tools/Tests/FishRulesTest.cpp
// Fishing (FishRules, with BuildingRules and SettlementRules): the fish within a settlement's reach, the catch that thins
// them, the fishing lodge on the shore where the fish are, fishers only once it stands, and the food they bring. Run from
// TestGameRules.

#include "hypch.h"

#include "Rules/BuildingRules.h"
#include "Rules/FishRules.h"
#include "Rules/ObjectRules.h"
#include "Rules/SettlementRules.h"
#include "Rules/WildlifeRules.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace Hydro
{
	static int s_FishFailures = 0;

	static void FishCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_FishFailures++;
			std::printf("FAIL  FishRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define FISH_CHECK(condition) FishCheck((condition), #condition, __LINE__, checks)

	static ObjectDefinition MakeFishBuilding(int id)
	{
		ObjectDefinition object;
		object.Name = "Building " + std::to_string(id);
		object.ObjectId = id;
		object.Kind = ObjectKind::Building;
		return object;
	}

	// A 32 x 16 grass map in two wildlife regions; a river down x 20 and 21 (the right region) with 8 pike in it.
	struct FishWorld
	{
		TileMap Map{ 32, 16, 2 };
		Hydrology Water;
		ObjectLayer Objects;
		std::vector<ObjectDefinition> Definitions = { MakeFishBuilding(60), MakeFishBuilding(83) };
		std::vector<int> ById;
		std::vector<uint8_t> Land;
		std::vector<AnimalDefinition> Animals;
		WildlifeState Wildlife;
		std::vector<BuildingDefinition> Buildings;
		std::vector<int> Talents;

		FishWorld()
		{
			Objects.Reset(32 * 16);
			ById = ObjectRules::IndexById(Definitions);
			Land.assign(32 * 16, 1);
			Water.Moisture.assign(32 * 16, 128);
			for (int y = 0; y < 16; y++)
			{
				Map.Set(20, y, Water.WaterTileId);
				Map.Set(21, y, Water.WaterTileId);
			}
			AnimalDefinition pike;
			pike.Name = "Pike";
			pike.AnimalId = 32;
			pike.Kind = AnimalKind::Fish;
			Animals = { pike };
			Wildlife = WildlifeRules::Start(32, 16, 1, 16);
			Wildlife.Population[0][1] = 8.0f;
			BuildingDefinition lodge;
			lodge.Name = "Fishing lodge";
			lodge.BuildingId = 10;
			lodge.PeopleId = 1;
			lodge.Kind = BuildingKind::FishingLodge;
			lodge.ObjectId = 83;
			lodge.RequiresTalentId = 54;
			lodge.MinLevel = 1;
			lodge.Wood = 6.0f;
			lodge.Site = BuildingSite::Shore;
			lodge.RingFrom = 3;
			lodge.Fishers = 3;
			Buildings = { lodge };
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
			world.Wildlife = &Wildlife;
			world.Animals = &Animals;
			world.FireObjectId = 60;
			world.Buildings = &Buildings;
			world.Talents = &Talents;
			return world;
		}
	};

	// Fishers: radius 8, berries hardly anywhere, a camp and a hamlet (no shelters to build).
	static PeopleDefinition MakeFishers()
	{
		PeopleDefinition people;
		people.Name = "Humans";
		people.PeopleId = 1;
		people.SettlementNames = { "Ashford" };
		people.VillagerNames = { "Aldric", "Maren", "Edda" };
		people.Radius = 8;
		people.FoodPerPerson = 0.5f;
		people.FishAnimalIds = { 32 };
		people.FishPerPerson = 0.5f;
		people.FoodPerFish = 5.0f;
		PeopleLevel camp;
		camp.Name = "Camp";
		camp.MaxPeople = 12;
		camp.StoreDays = 20.0f;
		camp.ShelterObjectId = 0;
		PeopleLevel hamlet = camp;
		hamlet.Name = "Hamlet";
		hamlet.People = 99;
		people.Levels = { camp, hamlet };
		return people;
	}

	static void TestStockAndCatch(int& checks)
	{
		FishWorld world;
		const PeopleDefinition people = MakeFishers();
		// From (14, 8) the square of 8 holds 7 of the right region's 16 columns: 7/16 of its 8 pike.
		FISH_CHECK(FishRules::Stock(world.View(), people, 14, 8, 0) == 0.0f);
		FISH_CHECK(std::fabs(FishRules::Stock(world.View(), people, 14, 8, 8) - 3.5f) < 1e-4f);
		FISH_CHECK(FishRules::Stock(world.View(), people, 24, 8, 0) == 8.0f);
		PeopleDefinition dry = people;
		dry.FishAnimalIds.clear();
		FISH_CHECK(FishRules::Stock(world.View(), dry, 24, 8, 8) == 0.0f);

		// At most a quarter of what is in reach a time; the water has that many fewer.
		const float caught = FishRules::Catch(world.View(), people, 14, 8, 8, 10.0f);
		FISH_CHECK(std::fabs(caught - 0.875f) < 1e-4f && std::fabs(world.Wildlife.Population[0][1] - 7.125f) < 1e-4f);
		FISH_CHECK(FishRules::Catch(world.View(), people, 14, 8, 8, 0.1f) == 0.1f);
		FISH_CHECK(FishRules::Catch(world.View(), people, 14, 8, 8, 0.0f) == 0.0f);

		// The shore: land beside water.
		FISH_CHECK(FishRules::Shore(world.View(), 19, 8) && FishRules::Shore(world.View(), 22, 3));
		FISH_CHECK(FishRules::Shore(world.View(), 20, 8) == false && FishRules::Shore(world.View(), 14, 8) == false);
	}

	static void TestLodge(int& checks)
	{
		FishWorld world;
		const std::vector<PeopleDefinition> peoples = { MakeFishers() };
		const PeopleDefinition& people = peoples[0];
		SettlementState state;
		FISH_CHECK(SettlementRules::Found(state, world.View(), peoples, 0, 14, 8, 6) == 1);
		Settlement& home = state.List[0];
		Random random(3);

		// No fish within reach: no shore with fish to build on.
		Settlement nowhere = home;
		nowhere.X = 4;
		FISH_CHECK(FishRules::PutUp(world.View(), world.Buildings[0], people, nowhere, 8, random) == false);

		// Wanted once known, at its level, with fish within reach.
		SettlementRules::Survey survey = SettlementRules::Look(world.View(), people, 14, 8);
		FISH_CHECK(std::fabs(survey.Fish - 3.5f) < 1e-4f);
		FISH_CHECK(SettlementRules::Plan(home, world.View(), people, survey) == nullptr);
		world.Talents = { 54 };
		FISH_CHECK(SettlementRules::Plan(home, world.View(), people, survey) == nullptr); // a camp: not yet
		home.Level = 1;
		FISH_CHECK(SettlementRules::Plan(home, world.View(), people, survey) == &world.Buildings[0] && home.Planned == 10);
		survey.Fish = 1.0f;
		FISH_CHECK(SettlementRules::Plan(home, world.View(), people, survey) == nullptr);

		// No fishers before it stands, however many fish.
		survey.Fish = 40.0f;
		SettlementRules::AssignJobs(home, people, survey);
		FISH_CHECK(SettlementRules::Workers(home, VillagerJob::Fisher) == 0);

		// Built: on the shore, beside the river.
		home.Wood = 10.0f;
		home.Building = 1.0f;
		SettlementRules::Report report;
		for (int day = 0; day < 6 && report.BuiltBuilding == 0; day++)
		{
			home.Food = 20.0f;
			home.Building = 1.0f;
			report = SettlementRules::Update(home, world.View(), peoples, 0.5f, random);
		}
		FISH_CHECK(report.BuiltBuilding == 10 && home.Buildings == std::vector<int>({ 10 }));
		int lodges = 0;
		bool onShore = true;
		for (int y = 0; y < 16; y++)
		{
			for (int x = 0; x < 32; x++)
			{
				const bool lodge = world.Objects.Get((size_t)(y * 32 + x)) == 83;
				lodges += lodge ? 1 : 0;
				onShore = onShore && (lodge == false || (FishRules::Shore(world.View(), x, y) && (x == 19 || x == 22)));
			}
		}
		FISH_CHECK(lodges == 1 && onShore);

		// Then the lodge's fishers (at most 3) fish when it gives the most food, and bring it home; the pike thin.
		survey = SettlementRules::Look(world.View(), people, 14, 8);
		survey.Fishers = 3;
		SettlementRules::AssignJobs(home, people, survey);
		const int fishers = SettlementRules::Workers(home, VillagerJob::Fisher);
		FISH_CHECK(fishers >= 1 && fishers <= 3);
		const float before = world.Wildlife.Population[0][1];
		const float eaters = home.Count;
		home.Food = 5.0f;
		report = SettlementRules::Update(home, world.View(), peoples, 1.0f, random);
		FISH_CHECK(report.Caught > 0.0f && world.Wildlife.Population[0][1] < before);
		FISH_CHECK(std::fabs(home.Food - (5.0f - eaters * 0.5f + report.Caught * 5.0f)) < 0.01f);
		FISH_CHECK(std::string(SettlementRules::JobName(VillagerJob::Fisher)) == "Fisher");

		// Fished out: no fishers.
		world.Wildlife.Population[0][1] = 0.0f;
		survey = SettlementRules::Look(world.View(), people, 14, 8);
		survey.Fishers = 3;
		SettlementRules::AssignJobs(home, people, survey);
		FISH_CHECK(SettlementRules::Workers(home, VillagerJob::Fisher) == 0);
	}

	int TestFishRules(int& checks)
	{
		std::printf("Game rules: FishRules\n");
		s_FishFailures = 0;
		TestStockAndCatch(checks);
		TestLodge(checks);
		return s_FishFailures;
	}
}
