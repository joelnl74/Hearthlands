// Tools/Tests/FarmRulesTest.cpp
// Farming (FarmRules) on painted farmland near a hamlet: farmers as jobs, sowing in season, crops growing faster on rich,
// moist soil, harvests worn into the soil, frost; seasonal wild food; the hamlet milestone and the talent it opens. Run from
// TestGameRules.

#include "hypch.h"

#include "Rules/FarmRules.h"
#include "Rules/ObjectRules.h"
#include "Rules/SettlementRules.h"
#include "Rules/SoilRules.h"
#include "Rules/TalentRules.h"

#include <cstdio>
#include <vector>

namespace Hydro
{
	static int s_FarmFailures = 0;

	static void FarmCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_FarmFailures++;
			std::printf("FAIL  FarmRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define FARM_CHECK(condition) FarmCheck((condition), #condition, __LINE__, checks)

	static ObjectDefinition MakeFarmObject(int id, ObjectKind kind, float food, int growsOn)
	{
		ObjectDefinition object;
		object.Name = "Object " + std::to_string(id);
		object.ObjectId = id;
		object.Kind = kind;
		object.GrowsOnTileId = growsOn;
		object.Food = food;
		return object;
	}

	// A 40 x 40 grass map with a pond; a 6 x 4 field of farmland (7) east of the centre (20, 20), soil and moisture set.
	struct FarmWorld
	{
		TileMap Map{ 40, 40, 2 };
		Hydrology Water;
		ObjectLayer Objects;
		SoilState Soil;
		SoilRules::Settings SoilSettings;
		std::vector<ObjectDefinition> Definitions = { MakeFarmObject(1, ObjectKind::Shrub, 0.8f, 2),
			MakeFarmObject(60, ObjectKind::Building, 0.0f, -1), MakeFarmObject(72, ObjectKind::Ground, 0.0f, 7),
			MakeFarmObject(73, ObjectKind::Ground, 0.0f, 7), MakeFarmObject(74, ObjectKind::Ground, 0.0f, 7) };
		std::vector<int> ById;
		std::vector<uint8_t> Land;

		FarmWorld()
		{
			Objects.Reset(40 * 40);
			for (int y = 12; y <= 15; y++)
			{
				for (int x = 14; x <= 17; x++)
				{
					Map.Set(x, y, 3);
				}
			}
			for (int y = 18; y <= 21; y++)
			{
				for (int x = 23; x <= 28; x++)
				{
					Map.Set(x, y, 7);
				}
			}
			for (int i = 0; i < 6; i++)
			{
				Objects.Set((size_t)(25 * 40 + 14 + i * 2), 1);
			}
			ById = ObjectRules::IndexById(Definitions);
			Land.assign(40 * 40, 1);
			Soil = SoilRules::Start(Map, SoilSettings);
			Water.Moisture.assign(40 * 40, 128);
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
			world.Soil = &Soil;
			world.SoilSettings = &SoilSettings;
			world.FieldTileId = 7;
			return world;
		}

		int CountObjects(uint8_t id) const
		{
			int count = 0;
			for (uint8_t object : Objects.GetIds())
			{
				count += object == id ? 1 : 0;
			}
			return count;
		}
	};

	static PeopleDefinition MakeFarmers()
	{
		PeopleDefinition people;
		people.Name = "Humans";
		people.PeopleId = 1;
		people.SettlementNames = { "Ashford" };
		people.VillagerNames = { "Aldric", "Maren" };
		people.FoodPerPerson = 0.5f;
		people.FarmTileId = 7;
		people.FarmFromLevel = 1;
		people.CropObjectIds = { 72, 73, 74 };
		people.CropDays = 4.0f;
		people.CropFood = 6.0f;
		people.SowPerDay = 4.0f;
		people.HarvestPerDay = 6.0f;
		people.SoilWear = 0.1f;
		PeopleLevel camp;
		camp.Name = "Camp";
		camp.MaxPeople = 12;
		camp.StoreDays = 20.0f;
		PeopleLevel hamlet = camp;
		hamlet.Name = "Hamlet";
		hamlet.People = 4;
		people.Levels = { camp, hamlet };
		return people;
	}

	int TestFarmRules(int& checks)
	{
		std::printf("Game rules: FarmRules\n");
		FarmWorld world;
		const std::vector<PeopleDefinition> peoples = { MakeFarmers() };
		const PeopleDefinition& humans = peoples[0];
		SettlementState state;
		const int id = SettlementRules::Found(state, world.View(), peoples, 0, 20, 20, 8);
		FARM_CHECK(id == 1);
		Settlement& home = state.List[0];
		Random random(4);

		// Nothing is built on farmland; a camp doesn't farm yet, a hamlet does.
		FARM_CHECK(SettlementRules::Buildable(world.View(), 24, 19) == false && SettlementRules::Buildable(world.View(), 21, 23));
		FARM_CHECK(SettlementRules::Farming(humans, home) == false);
		FarmRules::Work work = FarmRules::Step(home, world.View(), peoples, 1.0f, random);
		FARM_CHECK(home.Fields == 24 && home.FieldsGrowing == 0 && work.Sown == 0);
		home.Level = 1;
		FARM_CHECK(SettlementRules::Farming(humans, home) && FarmRules::Farmland(world.View(), humans, home).size() == 24);

		// Farmers: the empty fields want them in a sowing season, not out of it.
		SettlementRules::Survey survey = SettlementRules::Look(world.View(), humans, 20, 20);
		survey.Sowing = true;
		SettlementRules::AssignJobs(home, humans, survey);
		const int farmers = SettlementRules::Workers(home, VillagerJob::Farmer);
		FARM_CHECK(farmers >= 1 && farmers <= 6);
		survey.Sowing = false;
		SettlementRules::AssignJobs(home, humans, survey);
		FARM_CHECK(SettlementRules::Workers(home, VillagerJob::Farmer) == 0);
		survey.Sowing = true;
		SettlementRules::AssignJobs(home, humans, survey);

		// Sowing: the fields nearest the centre first, as far as the farmers' days go.
		work = FarmRules::Step(home, world.View(), peoples, 1.0f, random);
		FARM_CHECK(work.Sown >= 4 && work.Sown == world.CountObjects(72) + world.CountObjects(73) + world.CountObjects(74));
		FARM_CHECK(world.Objects.Get(20 * 40 + 23) != 0); // the nearest field
		SettlementRules::World winter = world.View();
		winter.Sowing = false;
		const int sownBefore = world.CountObjects(72) + world.CountObjects(73);
		home.FarmWork = 0.0f;
		const FarmRules::Work idle = FarmRules::Step(home, winter, peoples, 1.0f, random);
		FARM_CHECK(idle.Sown == 0 && idle.Harvested == 0 && world.CountObjects(72) + world.CountObjects(73) + world.CountObjects(74) >= sownBefore);

		// Fertility: rich soil and wet ground grow faster than plain and dry.
		const size_t rich = 18 * 40 + 23;
		const size_t poor = 21 * 40 + 28;
		world.Soil.Fertility[rich] = 255;
		world.Soil.Fertility[poor] = 20;
		world.Water.Moisture[poor] = 10;
		FARM_CHECK(FarmRules::Fertility(world.View(), rich) > 1.5f && FarmRules::Fertility(world.View(), poor) < 0.5f);
		FARM_CHECK(FarmRules::Wetness(world.View(), rich) > FarmRules::Wetness(world.View(), poor));
		world.Objects.Set(rich, 72);
		world.Objects.Set(poor, 72);
		int richDays = -1;
		int poorDays = -1;
		for (int day = 1; day <= 60 && (richDays < 0 || poorDays < 0); day++)
		{
			FarmRules::Grow(world.View(), humans, { rich, poor }, 1.0f, random);
			richDays = richDays < 0 && world.Objects.Get(rich) == 74 ? day : richDays;
			poorDays = poorDays < 0 && world.Objects.Get(poor) == 74 ? day : poorDays;
		}
		FARM_CHECK(richDays > 0 && (poorDays < 0 || poorDays > richDays));

		// Harvest: ripe fields give food (more on rich soil), are emptied, and their soil wears.
		for (int y = 18; y <= 21; y++)
		{
			for (int x = 23; x <= 28; x++)
			{
				world.Objects.Set((size_t)(y * 40 + x), 74);
			}
		}
		const uint8_t before = world.Soil.Fertility[rich];
		home.FarmWork = 0.0f;
		const float food = home.Food;
		work = FarmRules::Step(home, world.View(), peoples, 1.0f, random);
		FARM_CHECK(work.Harvested >= 6 && work.Food >= 6.0f * (float)work.Harvested * 0.5f && home.Food > food);
		FARM_CHECK(home.FieldsRipe == 24 - work.Harvested);
		FARM_CHECK(world.Soil.Fertility[rich] == before || world.Soil.Fertility[rich] == before - 26);
		FARM_CHECK(FarmRules::FarmersWanted(humans, FarmRules::Count(world.View(), humans, FarmRules::Farmland(world.View(), humans, home)), true) >= 1);

		// Frost: what is left in the fields withers.
		SettlementRules::World frost = world.View();
		frost.Frost = true;
		int withered = 0;
		for (int day = 0; day < 20; day++)
		{
			withered += FarmRules::Grow(frost, humans, FarmRules::Farmland(world.View(), humans, home), 1.0f, random);
		}
		FARM_CHECK(withered >= 1 && world.CountObjects(72) + world.CountObjects(73) + world.CountObjects(74) == 0);

		// Wild food by the season: little in winter.
		Settlement summer = home;
		Settlement cold = home;
		SettlementRules::World lean = world.View();
		lean.Harvest = 0.25f;
		SettlementRules::Update(summer, world.View(), peoples, 0.1f, random);
		SettlementRules::Update(cold, lean, peoples, 0.1f, random);
		FARM_CHECK(cold.FoodNearby < summer.FoodNearby);

		// The hamlet milestone, and the talent that waits for it.
		MilestoneDefinition hamlet;
		hamlet.MilestoneId = 39;
		hamlet.Kind = MilestoneKind::SettlementLevel;
		hamlet.TargetId = 1;
		hamlet.Count = 1;
		TalentRules::Counts counts;
		FARM_CHECK(TalentRules::Progress(hamlet, counts) == 0);
		counts.LevelById[1] = 1;
		counts.BestLevel = 1;
		FARM_CHECK(TalentRules::Progress(hamlet, counts) == 1);
		hamlet.TargetId = -1;
		FARM_CHECK(TalentRules::Progress(hamlet, counts) == 1);
		TalentDefinition farming;
		farming.TalentId = 45;
		farming.Cost = 1;
		farming.Effect = TalentEffect::UnlockTile;
		farming.Amount = 7.0f;
		farming.RequiresMilestoneId = 39;
		const std::vector<TalentDefinition> talents = { farming };
		TalentState owned;
		owned.Points = 3;
		FARM_CHECK(TalentRules::Check(talents, owned, 45) == TalentRules::Verdict::NeedsMilestone);
		owned.Reached.push_back(39);
		FARM_CHECK(TalentRules::Check(talents, owned, 45) == TalentRules::Verdict::Buyable);
		FARM_CHECK(std::string(SettlementRules::JobName(VillagerJob::Farmer)) == "Farmer");

		// The Farmland brush paints only within reach of a settlement that farms.
		{
			PeopleDefinition farmers = MakeFarmers();
			farmers.Radius = 5;
			PeopleDefinition elves = MakeFarmers();
			elves.FarmTileId = -1;
			const std::vector<PeopleDefinition> peoples = { farmers, elves };
			SettlementState state;
			Settlement camp;
			camp.Id = 1;
			camp.People = 0;
			camp.X = 10;
			camp.Y = 10;
			camp.Level = 0;
			state.List.push_back(camp);
			std::vector<uint8_t> reach;
			FarmRules::Reach(state, peoples, 7, 40, 40, reach);
			FARM_CHECK(reach.size() == 1600);
			FARM_CHECK(FarmRules::MayPaint(reach, 7, 7, 12, 10, 40) == false); // a camp doesn't farm yet
			state.List[0].Level = 1;
			FarmRules::Reach(state, peoples, 7, 40, 40, reach);
			FARM_CHECK(FarmRules::MayPaint(reach, 7, 7, 12, 10, 40));
			FARM_CHECK(FarmRules::MayPaint(reach, 7, 7, 15, 10, 40));
			FARM_CHECK(FarmRules::MayPaint(reach, 7, 7, 14, 14, 40) == false); // outside the round reach
			FARM_CHECK(FarmRules::MayPaint(reach, 7, 7, 30, 30, 40) == false);
			FARM_CHECK(FarmRules::MayPaint(reach, 7, 2, 30, 30, 40)); // other brushes paint anywhere
			Settlement grove = camp;
			grove.Id = 2;
			grove.People = 1;
			grove.X = 30;
			grove.Y = 30;
			grove.Level = 2;
			state.List.push_back(grove);
			FarmRules::Reach(state, peoples, 7, 40, 40, reach);
			FARM_CHECK(FarmRules::MayPaint(reach, 7, 7, 30, 30, 40) == false); // a people that doesn't farm gives no reach
			FARM_CHECK(FarmRules::MayPaint(reach, -1, 7, 30, 30, 40)); // no farmland tile: no limit
		}
		return s_FarmFailures;
	}
}
