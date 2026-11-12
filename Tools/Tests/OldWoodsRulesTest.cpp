// Tools/Tests/OldWoodsRulesTest.cpp
// Old woods (ancient stages in ObjectRules), wood elves settling by them (SettlingRules), dwarves in the hills and their
// milestones.

#include "hypch.h"

#include "Rules/ObjectRules.h"
#include "Rules/SettlementRules.h"
#include "Rules/SettlingRules.h"
#include "Rules/TalentRules.h"

#include <cstdio>
#include <vector>

namespace Hydro
{
	static int s_OldWoodsFailures = 0;

	static void OldWoodsCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_OldWoodsFailures++;
			std::printf("FAIL  OldWoodsRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define OLDWOODS_CHECK(condition) OldWoodsCheck((condition), #condition, __LINE__, checks)

	static ObjectDefinition MakeWoodsObject(int id, ObjectKind kind, float canopy, float food, float wood)
	{
		ObjectDefinition object;
		object.ObjectId = id;
		object.Kind = kind;
		object.GrowsOnTileId = -1;
		object.Canopy = canopy;
		object.Food = food;
		object.Wood = wood;
		return object;
	}

	int TestOldWoodsRules(int& checks)
	{
		std::printf("Game rules: OldWoods\n");

		// Rows: oak (7) grows into an ancient oak (24) in a day; a berry bush, the lantern (62), the leaf hut (63), a fire (60).
		ObjectDefinition oak = MakeWoodsObject(7, ObjectKind::Tree, 1.0f, 0.0f, 3.0f);
		oak.GrowsIntoObjectId = 24;
		oak.GrowDays = 0.01f;
		ObjectDefinition ancient = MakeWoodsObject(24, ObjectKind::Tree, 1.0f, 0.5f, 8.0f);
		ancient.Ancient = true;
		const std::vector<ObjectDefinition> objects = { oak, ancient, MakeWoodsObject(2, ObjectKind::Shrub, 0.0f, 0.8f, 0.0f),
			MakeWoodsObject(62, ObjectKind::Building, 0.0f, 0.0f, 0.0f), MakeWoodsObject(63, ObjectKind::Building, 0.0f, 0.0f, 0.0f),
			MakeWoodsObject(60, ObjectKind::Building, 0.0f, 0.0f, 0.0f) };
		const std::vector<int> byId = ObjectRules::IndexById(objects);

		// A grass map (30 x 30) with a closed wood of oaks 2 apart on the left (x 2..12) and a lone oak at (24, 24).
		TileMap map(30, 30, 2);
		Hydrology water;
		water.Moisture.assign(900, 200);
		ObjectLayer layer;
		layer.Reset(900);
		for (int y = 2; y <= 12; y += 2)
		{
			for (int x = 2; x <= 12; x += 2)
			{
				layer.Set((size_t)y * 30 + (size_t)x, 7);
			}
		}
		layer.Set(24 * 30 + 24, 7);
		ForestRules::Settings forest;
		forest.AncientCanopy = 7.0f;

		// Without Old woods nothing grows ancient; the canopy count: 9 inside the wood, 1 alone.
		Random random(4);
		ObjectRules::Step(layer, map, water, {}, objects, byId, 40000, 1.0f, random, nullptr, {}, forest);
		TalentRules::Counts counts = TalentRules::Count(map, layer, objects, byId, HeightMap(30, 30), 0);
		OLDWOODS_CHECK(counts.Ancient == 0 && layer.Get(6 * 30 + 6) == 7);
		ForestRules::Canopy canopy;
		canopy.Layer = &layer;
		canopy.Objects = &objects;
		canopy.ById = &byId;
		OLDWOODS_CHECK(canopy.Total(map, 6, 6) == 9.0f && canopy.Total(map, 24, 24) == 1.0f && canopy.Total(map, 2, 2) == 4.0f);

		// With it the oaks deep in the wood grow ancient; the edge's and the lone oak don't.
		forest.OldGrowth = true;
		ObjectRules::Step(layer, map, water, {}, objects, byId, 40000, 1.0f, random, nullptr, {}, forest);
		OLDWOODS_CHECK(layer.Get(6 * 30 + 6) == 24 && layer.Get(8 * 30 + 8) == 24);
		OLDWOODS_CHECK(layer.Get(2 * 30 + 2) == 7 && layer.Get(24 * 30 + 24) == 7);
		counts = TalentRules::Count(map, layer, objects, byId, HeightMap(30, 30), 0);
		OLDWOODS_CHECK(counts.Ancient >= 4 && counts.Objects[(int)ObjectKind::Tree] == 37);
		MilestoneDefinition growth;
		growth.Kind = MilestoneKind::Ancient;
		growth.Count = 4;
		OLDWOODS_CHECK(TalentRules::Progress(growth, counts) == counts.Ancient);

		// Elves: a lantern (62) at the centre, leaf huts (63), no felling, 4 ancient trees (24) within 10 tiles, berries and water.
		PeopleDefinition humans;
		humans.Name = "Humans";
		humans.PeopleId = 1;
		humans.SettlementNames = { "Ashford" };
		PeopleDefinition elves;
		elves.Name = "Elves";
		elves.PeopleId = 2;
		elves.SettlementNames = { "Silverglade", "Moonwhisper" };
		elves.VillagerNames = { "Aerin", "Liriel" };
		elves.CentreObjectId = 62;
		elves.NeedWood = 0.0f;
		elves.NeedObjectIds = { 24, 25 };
		elves.NeedObjects = 4;
		elves.NeedObjectsName = "old trees";
		elves.FellPerDay = 0.0f;
		elves.GroupMin = 3;
		elves.GroupMax = 3;
		elves.FoodPerPerson = 0.5f;
		PeopleLevel grove;
		grove.Name = "Grove";
		grove.ShelterObjectId = 63;
		grove.WoodPerShelter = 0.0f;
		grove.MaxPeople = 9;
		PeopleLevel glade = grove;
		glade.Name = "Glade";
		glade.People = 5;
		glade.NeedObjects = 50; // more old trees than the wood has
		elves.Levels = { grove, glade };
		const std::vector<PeopleDefinition> peoples = { humans, elves };
		for (int i = 0; i < 4; i++)
		{
			map.Set(11 + i, 16, 3); // a pond south-east of the wood
			layer.Set(14 * 30 + 3 + (size_t)i * 2, 2); // berry bushes on y = 14
		}
		std::vector<uint8_t> land(900, 1);
		SettlementRules::World world;
		world.Map = &map;
		world.Water = &water;
		world.Objects = &layer;
		world.Definitions = &objects;
		world.ById = &byId;
		world.Land = &land;
		world.FireObjectId = 60;
		OLDWOODS_CHECK(SettlementRules::CentreObject(world, elves) == 62 && SettlementRules::CentreObject(world, humans) == 60);

		// The best place for them is by the old trees: the lantern goes up there, named from their names.
		SettlementState state;
		int x = 0;
		int y = 0;
		OLDWOODS_CHECK(SettlingRules::Find(world, state, elves, -1, -1, 0, x, y));
		const SettlementRules::Survey there = SettlementRules::Look(world, elves, x, y);
		OLDWOODS_CHECK(there.Needed >= 4 && SettlementRules::NeededShare(elves, there) == 1.0f && x <= 16 && y <= 18);
		const int id = SettlementRules::Found(state, world, peoples, 1, x, y, 3);
		OLDWOODS_CHECK(id == 1 && layer.Get((size_t)y * 30 + (size_t)x) == 62 && state.List[0].Name == "Silverglade");
		OLDWOODS_CHECK(state.List[0].Villagers.size() == 3 && state.List[0].People == 1);

		// Too few old trees elsewhere: no place for them far from the wood.
		const SettlementRules::Survey few = SettlementRules::Look(world, elves, 24, 26);
		OLDWOODS_CHECK(few.Needed < 4 && SettlementRules::Meets(elves, few) == false);

		// They live on berries and acorns and put up leaf huts without felling a tree; the next level wants more old trees.
		Random days(9);
		Settlement& camp = state.List[0];
		int felled = 0;
		int built = 0;
		for (int day = 0; day < 40; day++)
		{
			const SettlementRules::Report report = SettlementRules::Update(camp, world, peoples, 0.5f, days);
			felled += report.Felled;
			built += report.Built;
		}
		OLDWOODS_CHECK(felled == 0 && built >= 1 && camp.Shelters >= 1 && camp.Count > 3.0f && camp.Level == 0);
		const std::vector<SettlementRules::Need> needs = SettlementRules::NextNeeds(camp, elves);
		OLDWOODS_CHECK(needs.size() == 2 && needs[1].Name == "Old trees" && needs[1].Want == 50.0f && needs[1].Have >= 4.0f);

		// Settlers per people for the milestones.
		counts.People = (uint32_t)(SettlementRules::People(state) + 0.5f);
		counts.PeopleById[2] = counts.People;
		MilestoneDefinition elvesCame;
		elvesCame.Kind = MilestoneKind::Settlers;
		elvesCame.TargetId = 2;
		MilestoneDefinition humansCame = elvesCame;
		humansCame.TargetId = 1;
		MilestoneDefinition anyone = elvesCame;
		anyone.TargetId = -1;
		OLDWOODS_CHECK(TalentRules::Progress(elvesCame, counts) == counts.People && TalentRules::Progress(humansCame, counts) == 0);
		OLDWOODS_CHECK(TalentRules::Progress(anyone, counts) == counts.People);

		// The lantern taken away (painted over): the settlement goes with it; a human fire there wouldn't keep it.
		layer.Set((size_t)camp.Y * 30 + (size_t)camp.X, 60);
		OLDWOODS_CHECK(SettlementRules::Update(camp, world, peoples, 0.1f, days).Gone);

		// Dwarves: a mine (64) up in the hills (level 1+), 40 rock tiles (6) within 12 tiles.
		PeopleDefinition dwarves;
		dwarves.Name = "Dwarves";
		dwarves.PeopleId = 3;
		dwarves.SettlementNames = { "Ironhold" };
		dwarves.CentreObjectId = 64;
		dwarves.CentreMinLevel = 1;
		dwarves.Radius = 12;
		dwarves.NeedWater = 0.0f;
		dwarves.NeedFood = 0.0f;
		dwarves.NeedWood = 0.0f;
		dwarves.NeedTileIds = { 6 };
		dwarves.NeedTiles = 40;
		dwarves.NeedTilesName = "mountain rock";
		dwarves.SettleSpacing = 4;
		std::vector<ObjectDefinition> rows = objects;
		rows.push_back(MakeWoodsObject(64, ObjectKind::Building, 0.0f, 0.0f, 0.0f));
		const std::vector<int> rowIds = ObjectRules::IndexById(rows);
		HeightMap heights(30, 30);
		for (int hy = 18; hy < 30; hy++)
		{
			for (int hx = 18; hx < 30; hx++)
			{
				heights.Set(hx, hy, 2);
				map.Set(hx, hy, (hx + hy) % 3 == 0 ? 2 : 6); // a rocky hill in the corner, some grass on it
			}
		}
		world.Definitions = &rows;
		world.ById = &rowIds;
		world.Heights = &heights;
		OLDWOODS_CHECK(SettlementRules::HighEnough(world, dwarves, 10, 20) == false && SettlementRules::HighEnough(world, dwarves, 22, 24));
		SettlementState mountains;
		OLDWOODS_CHECK(SettlementRules::Found(mountains, world, { dwarves }, 0, 10, 20, 3) == 0); // the low land: no mine
		OLDWOODS_CHECK(SettlingRules::Find(world, mountains, dwarves, -1, -1, 0, x, y) && heights.Get(x, y) >= 1);
		const SettlementRules::Survey rock = SettlementRules::Look(world, dwarves, x, y);
		OLDWOODS_CHECK(rock.NeededTiles >= 40 && SettlementRules::NeededShare(dwarves, rock) == 1.0f);
		return s_OldWoodsFailures;
	}
}
