// Tools/Tests/SettlementRulesTest.cpp
// A settlement's life (SettlementRules) on a small map: villagers and their jobs, focus, levels, hunger; and the settler
// milestones (TalentRules). Run from TestGameRules.

#include "hypch.h"

#include "Rules/ObjectRules.h"
#include "Rules/SettlementRules.h"
#include "Rules/TalentRules.h"

#include <cstdio>
#include <vector>

namespace Hydro
{
	static int s_SettlementFailures = 0;

	static void SettlementCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_SettlementFailures++;
			std::printf("FAIL  SettlementRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define SETTLEMENT_CHECK(condition) SettlementCheck((condition), #condition, __LINE__, checks)

	static bool SettlerNear(float a, float b)
	{
		return a - b < 1e-3f && b - a < 1e-3f;
	}

	static ObjectDefinition MakeThing(int id, ObjectKind kind, float food, float wood)
	{
		ObjectDefinition object;
		object.Name = "Thing " + std::to_string(id);
		object.ObjectId = id;
		object.Kind = kind;
		object.GrowsOnTileId = -1;
		object.Food = food;
		object.Wood = wood;
		return object;
	}

	// A 40 x 40 grass map: a lake at (24..27, 18..21), berry bushes and trees round (20, 20), deer in its region.
	struct SettlerWorld
	{
		TileMap Map{ 40, 40, 2 };
		Hydrology Water;
		ObjectLayer Objects;
		std::vector<ObjectDefinition> Definitions = { MakeThing(1, ObjectKind::Shrub, 0.8f, 0.0f),
			MakeThing(2, ObjectKind::Tree, 0.0f, 3.0f), MakeThing(3, ObjectKind::Ground, 0.0f, 0.0f),
			MakeThing(60, ObjectKind::Building, 0.0f, 0.0f), MakeThing(61, ObjectKind::Building, 0.0f, 0.0f),
			MakeThing(62, ObjectKind::Building, 0.0f, 0.0f) };
		std::vector<int> ById;
		std::vector<uint8_t> Land;
		WildlifeState Wildlife;
		std::vector<AnimalDefinition> Animals;

		SettlerWorld()
		{
			Objects.Reset(40 * 40);
			for (int y = 18; y <= 21; y++)
			{
				for (int x = 24; x <= 27; x++)
				{
					Map.Set(x, y, 3);
				}
			}
			for (int i = 0; i < 6; i++)
			{
				Objects.Set((size_t)(14 * 40 + 14 + i * 2), 1);     // bushes along y = 14
				Objects.Set((size_t)((26 + (i % 2)) * 40 + 15 + i * 2), 2); // trees along y = 26 / 27
			}
			ById = ObjectRules::IndexById(Definitions);
			Land.assign(40 * 40, 1);
			AnimalDefinition deer;
			deer.AnimalId = 1;
			Animals = { deer };
			Wildlife = WildlifeRules::Start(40, 40, 1);
			Wildlife.Population[0][Wildlife.RegionOf(20, 20)] = 8.0f;
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
			return world;
		}
	};

	// Humans: a camp of tents (61, 3 each, 6 wood) up to 9, a hamlet of huts (62, 4 each) up to 12 once 6 live there with
	// a tent and 2 days of food in store. They eat 0.6 a day.
	static PeopleDefinition MakeHumans()
	{
		PeopleDefinition people;
		people.Name = "Humans";
		people.SettlementNames = { "Ashford", "Riverstead" };
		people.VillagerNames = { "Aldric", "Maren", "Tobin" };
		people.HuntAnimalIds = { 1 };
		people.GroupMin = 3;
		people.GroupMax = 3;
		people.FoodPerPerson = 0.6f;
		PeopleLevel camp;
		camp.Name = "Camp";
		camp.ShelterObjectId = 61;
		camp.PeoplePerShelter = 3;
		camp.WoodPerShelter = 6.0f;
		camp.MaxPeople = 9;
		camp.StoreDays = 4.0f;
		PeopleLevel hamlet;
		hamlet.Name = "Hamlet";
		hamlet.People = 6;
		hamlet.Shelters = 1;
		hamlet.FoodDays = 2.0f;
		hamlet.ShelterObjectId = 62;
		hamlet.PeoplePerShelter = 4;
		hamlet.WoodPerShelter = 6.0f;
		hamlet.MaxPeople = 12;
		hamlet.StoreDays = 5.0f;
		people.Levels = { camp, hamlet };
		return people;
	}

	static int CountObjects(const ObjectLayer& layer, uint8_t id)
	{
		int count = 0;
		for (uint8_t object : layer.GetIds())
		{
			count += object == id ? 1 : 0;
		}
		return count;
	}

	int TestSettlementRules(int& checks)
	{
		std::printf("Game rules: SettlementRules\n");
		SettlerWorld world;
		const std::vector<PeopleDefinition> peoples = { MakeHumans() };
		const PeopleDefinition& humans = peoples[0];
		SettlementState state;

		// Found: only on free, dry land inside the coast; a ground plant gives way, a tree doesn't.
		SETTLEMENT_CHECK(SettlementRules::Found(state, world.View(), peoples, 0, 25, 19, 3) == 0); // the lake
		SETTLEMENT_CHECK(SettlementRules::Found(state, world.View(), peoples, 0, 15, 26, 3) == 0); // a tree
		world.Land[(size_t)(5 * 40 + 5)] = 0;
		SETTLEMENT_CHECK(SettlementRules::Found(state, world.View(), peoples, 0, 5, 5, 3) == 0);   // the sea side
		SETTLEMENT_CHECK(SettlementRules::Found(state, world.View(), peoples, 3, 20, 20, 3) == 0); // no such people
		world.Objects.Set((size_t)(20 * 40 + 20), 3);
		const int id = SettlementRules::Found(state, world.View(), peoples, 0, 20, 20, 3);
		SETTLEMENT_CHECK(id == 1 && state.List.size() == 1 && state.NextId == 2 && state.Revision == 1);
		Settlement& camp = state.List[0];
		SETTLEMENT_CHECK(world.Objects.Get((size_t)(20 * 40 + 20)) == 60 && camp.Name == "Ashford" && camp.People == 0);
		SETTLEMENT_CHECK(camp.Count == 3.0f && camp.Villagers.size() == 3 && SettlerNear(camp.Food, 3.6f) && camp.Level == 0);
		// Names in turn from the 7th on (the first settlement), a second round numbered.
		SETTLEMENT_CHECK(camp.Villagers[0].Name == "Maren" && camp.Villagers[1].Name == "Tobin" && camp.Villagers[2].Name == "Aldric");
		Villager fourth = SettlementRules::NewVillager(camp, humans);
		SETTLEMENT_CHECK(fourth.Name == "Maren II" && fourth.Id == 4);
		camp.NextVillager = 4;
		camp.VillagerNames = 3;

		// Look: fresh water, berries, wood, game; the scores against the people's needs.
		const SettlementRules::Survey survey = SettlementRules::Look(world.View(), humans, 20, 20);
		SETTLEMENT_CHECK(survey.FreshWater == 16 && SettlerNear(survey.PlantFood, 6.0f * 0.8f) && SettlerNear(survey.Wood, 18.0f));
		SETTLEMENT_CHECK(survey.Trees == 6 && SettlerNear(survey.Game, 8.0f) && survey.Shelters == 0 && survey.ShelterRoom == 0);
		float water = 0.0f;
		float food = 0.0f;
		float wood = 0.0f;
		SettlementRules::Scores(humans, survey, water, food, wood);
		SETTLEMENT_CHECK(water == 1.0f && food == 1.0f && wood == 1.0f && SettlementRules::Meets(humans, survey));
		SETTLEMENT_CHECK(SettlementRules::RoomFor(humans, 0, 0) == 3 && SettlementRules::RoomFor(humans, 0, 6) == 9);
		SETTLEMENT_CHECK(SettlementRules::RoomFor(humans, 0, 30) == 9 && SettlementRules::RoomFor(humans, 1, 30) == 12);
		SETTLEMENT_CHECK(SettlementRules::ShelterHolds(humans, 61) == 3 && SettlementRules::ShelterHolds(humans, 62) == 4);
		SETTLEMENT_CHECK(SettlementRules::ShelterHolds(humans, 60) == 0);

		// Jobs: a gatherer feeds three (2 food each, 4.8 in the bushes), a second fills the store; no room for a fourth: a
		// woodcutter.
		camp.Room = 3;
		SettlementRules::AssignJobs(camp, humans, survey);
		SETTLEMENT_CHECK(SettlementRules::Workers(camp, VillagerJob::Gatherer) == 2);
		SETTLEMENT_CHECK(SettlementRules::Workers(camp, VillagerJob::Woodcutter) == 1);
		SETTLEMENT_CHECK(SettlementRules::WantShelter(camp, humans));
		// A shelter's wood in store: a builder instead. They keep their jobs while still wanted.
		const int firstGatherer = camp.Villagers[0].Job == VillagerJob::Gatherer ? camp.Villagers[0].Id : camp.Villagers[1].Id;
		camp.Wood = 6.0f;
		SettlementRules::AssignJobs(camp, humans, survey);
		SETTLEMENT_CHECK(SettlementRules::Workers(camp, VillagerJob::Builder) == 1 && SettlementRules::Workers(camp, VillagerJob::Woodcutter) == 0);
		bool kept = false;
		for (const Villager& villager : camp.Villagers)
		{
			kept = kept || (villager.Id == firstGatherer && villager.Job == VillagerJob::Gatherer);
		}
		SETTLEMENT_CHECK(kept);
		// The hurt can't work; tension brings a guard.
		camp.Villagers[2].HurtDays = 2.0f;
		camp.Tension = 0.6f;
		camp.FeudWith = 7;
		SettlementRules::AssignJobs(camp, humans, survey);
		SETTLEMENT_CHECK(camp.Villagers[2].Job == VillagerJob::Hurt && SettlementRules::Workers(camp, VillagerJob::Guard) == 1);
		camp.Villagers[2].HurtDays = 0.0f;
		camp.Count = 6.0f;
		SettlementRules::SyncVillagers(camp, humans);
		SETTLEMENT_CHECK(camp.Villagers.size() == 6 && camp.Villagers[3].Name == "Maren II");
		SettlementRules::AssignJobs(camp, humans, survey);
		SETTLEMENT_CHECK(SettlementRules::Workers(camp, VillagerJob::Guard) >= 1);
		// Twelve with no focus: some rest; focus on food: everyone spare hunts; on wood: they fell.
		camp.Tension = 0.0f;
		camp.FeudWith = 0;
		camp.Wood = 0.0f;
		camp.Food = 0.0f;
		camp.Room = 9;
		camp.Count = 12.0f;
		SettlementRules::SyncVillagers(camp, humans);
		SettlementRules::AssignJobs(camp, humans, survey);
		const int resting = SettlementRules::Workers(camp, VillagerJob::Resting);
		const int hunters = SettlementRules::Workers(camp, VillagerJob::Hunter);
		const int cutters = SettlementRules::Workers(camp, VillagerJob::Woodcutter);
		SETTLEMENT_CHECK(resting >= 1 && SettlementRules::Workers(camp, VillagerJob::Gatherer) == 2 && cutters == 2);
		camp.Focus = SettlementFocus::Food;
		SettlementRules::AssignJobs(camp, humans, survey);
		SETTLEMENT_CHECK(SettlementRules::Workers(camp, VillagerJob::Resting) == 0 && SettlementRules::Workers(camp, VillagerJob::Hunter) > hunters);
		camp.Focus = SettlementFocus::Wood;
		SettlementRules::AssignJobs(camp, humans, survey);
		SETTLEMENT_CHECK(SettlementRules::Workers(camp, VillagerJob::Woodcutter) > cutters && SettlementRules::Workers(camp, VillagerJob::Resting) == 0);
		camp.Focus = SettlementFocus::None;
		camp.Count = 3.0f;
		camp.Food = 3.6f;
		SettlementRules::SyncVillagers(camp, humans);
		SETTLEMENT_CHECK(camp.Villagers.size() == 3);

		// Living: the berries feed them, they fell, build a tent, grow and become a hamlet; villagers follow the people.
		Random random(5);
		int felled = 0;
		int built = 0;
		bool leveled = false;
		for (int day = 0; day < 160 && leveled == false; day++)
		{
			const SettlementRules::Report report = SettlementRules::Update(camp, world.View(), peoples, 0.5f, random);
			felled += report.Felled;
			built += report.Built;
			leveled = report.LeveledUp;
			SETTLEMENT_CHECK(camp.Villagers.size() == (size_t)(int)(camp.Count + 0.5f));
		}
		SETTLEMENT_CHECK(felled >= 1 && built >= 1 && camp.Shelters >= 1 && camp.Count >= 5.5f);
		SETTLEMENT_CHECK(leveled && camp.Level == 1 && CountObjects(world.Objects, 62) == 1);
		SETTLEMENT_CHECK(CountObjects(world.Objects, 61) + CountObjects(world.Objects, 62) == built); // a tent became a hut at once
		SETTLEMENT_CHECK(SettlementRules::ShelterLevel(humans, 61) == 0 && SettlementRules::ShelterLevel(humans, 62) == 1);
		SETTLEMENT_CHECK(SettlementRules::Older(humans, camp, 61) && SettlementRules::Older(humans, camp, 62) == false);
		SETTLEMENT_CHECK(SettlementRules::CountOld(world.View(), humans, camp) == built - 1);
		SETTLEMENT_CHECK(world.Objects.Get((size_t)(20 * 40 + 20)) == 60 && SettlerNear(SettlementRules::People(state), camp.Count));
		SETTLEMENT_CHECK(SettlementRules::NextNeeds(camp, humans).empty() && SettlementRules::HasNextLevel(humans, 1) == false);
		Settlement fresh = camp;
		fresh.Level = 0;
		fresh.Count = 4.0f;
		fresh.Shelters = 0;
		fresh.Food = 3.0f;
		const std::vector<SettlementRules::Need> needs = SettlementRules::NextNeeds(fresh, humans);
		SETTLEMENT_CHECK(needs.size() == 3 && needs[0].Name == "People" && needs[0].Have == 4.0f && needs[0].Want == 6.0f);
		SETTLEMENT_CHECK(needs[1].Name == "Shelters" && needs[2].Name == "Food in store" && needs[2].Want == 8.0f);
		SETTLEMENT_CHECK(SettlementRules::NeedsMet(needs) == false && SettlementRules::NeedsMet({}) == false);

		// A villager lost: gone from the list and the count.
		const size_t living = camp.Villagers.size();
		const float count = camp.Count;
		SETTLEMENT_CHECK(SettlementRules::Lose(camp, 0) && camp.Villagers.size() == living - 1 && SettlerNear(camp.Count, count - 1.0f));
		SETTLEMENT_CHECK(SettlementRules::Lose(camp, 99) == false);

		// Hunger: no berries, no game, no store: people leave, and at last the settlement goes, its fire with it.
		for (size_t i = 0; i < world.Objects.GetIds().size(); i++)
		{
			world.Objects.Set(i, world.Objects.Get(i) == 1 ? (uint8_t)0 : world.Objects.Get(i));
		}
		world.Wildlife.Population[0][world.Wildlife.RegionOf(20, 20)] = 0.0f;
		camp.Food = 0.0f;
		const float before = camp.Count;
		SettlementRules::Update(camp, world.View(), peoples, 1.0f, random);
		SETTLEMENT_CHECK(camp.Count < before && camp.Food == 0.0f);
		bool abandoned = false;
		for (int day = 0; day < 60 && abandoned == false; day++)
		{
			abandoned = SettlementRules::Update(camp, world.View(), peoples, 1.0f, random).Abandoned;
		}
		SETTLEMENT_CHECK(abandoned && camp.Count == 0.0f && camp.Villagers.empty() && world.Objects.Get((size_t)(20 * 40 + 20)) == 0);

		// Rename: the next of its people's names. The fire gone: the settlement with it.
		SettlementRules::Rename(camp, peoples);
		SETTLEMENT_CHECK(camp.Name == "Riverstead");
		SettlementRules::Rename(camp, peoples);
		SETTLEMENT_CHECK(camp.Name == "Ashford");
		SETTLEMENT_CHECK(SettlementRules::Update(camp, world.View(), peoples, 1.0f, random).Gone);
		SETTLEMENT_CHECK(std::string(SettlementRules::JobName(VillagerJob::Woodcutter)) == "Woodcutter");

		// The milestone: settlers living.
		TalentRules::Counts counts;
		counts.People = 4;
		MilestoneDefinition first;
		first.Kind = MilestoneKind::Settlers;
		SETTLEMENT_CHECK(TalentRules::Progress(first, counts) == 4);

		return s_SettlementFailures;
	}
}
