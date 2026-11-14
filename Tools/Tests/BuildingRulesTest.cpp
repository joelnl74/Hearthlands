// Tools/Tests/BuildingRulesTest.cpp
// The buildings a settlement puts up besides its shelters (BuildingRules, with SettlementRules and FarmRules): known by a
// talent, built when needed (a full store, hunting, more fields than worked), standing near the centre, and what they lift:
// the store, the meat kept, the fields worked. Run from TestGameRules.

#include "hypch.h"

#include "Rules/BuildingRules.h"
#include "Rules/FarmRules.h"
#include "Rules/ObjectRules.h"
#include "Rules/SettlementRules.h"

#include <cstdio>
#include <vector>

namespace Hydro
{
	static int s_BuildingFailures = 0;

	static void BuildingCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_BuildingFailures++;
			std::printf("FAIL  BuildingRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define BUILDING_CHECK(condition) BuildingCheck((condition), #condition, __LINE__, checks)

	static ObjectDefinition MakeStructure(int id)
	{
		ObjectDefinition object;
		object.Name = "Building " + std::to_string(id);
		object.ObjectId = id;
		object.Kind = ObjectKind::Building;
		return object;
	}

	static BuildingDefinition MakeBuilding(int id, BuildingKind kind, int objectId, int talentId, int minLevel, float wood)
	{
		BuildingDefinition building;
		building.Name = kind == BuildingKind::Store ? "Granary" : (kind == BuildingKind::Barn ? "Barn" : "Hunting lodge");
		building.BuildingId = id;
		building.PeopleId = 1;
		building.Kind = kind;
		building.ObjectId = objectId;
		building.RequiresTalentId = talentId;
		building.MinLevel = minLevel;
		building.Wood = wood;
		building.RingFrom = 2;
		building.RingTo = 4;
		building.StoreDays = kind == BuildingKind::Store ? 6.0f : 0.0f;
		building.HuntKeep = kind == BuildingKind::HuntingLodge ? 0.5f : 0.0f;
		building.Fields = kind == BuildingKind::Barn ? 24 : 0;
		return building;
	}

	// A 30 x 30 grass island with the humans' buildings known to it.
	struct BuildingWorld
	{
		TileMap Map{ 30, 30, 2 };
		Hydrology Water;
		ObjectLayer Objects;
		std::vector<ObjectDefinition> Definitions = { MakeStructure(60), MakeStructure(75), MakeStructure(76), MakeStructure(77) };
		std::vector<int> ById;
		std::vector<uint8_t> Land;
		std::vector<BuildingDefinition> Buildings = { MakeBuilding(1, BuildingKind::Store, 75, 46, 0, 6.0f),
			MakeBuilding(2, BuildingKind::HuntingLodge, 76, 47, 0, 6.0f), MakeBuilding(3, BuildingKind::Barn, 77, 45, 1, 10.0f) };
		std::vector<int> Talents;

		BuildingWorld()
		{
			Objects.Reset(30 * 30);
			ById = ObjectRules::IndexById(Definitions);
			Land.assign(30 * 30, 1);
			Water.Moisture.assign(30 * 30, 128);
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
			world.Buildings = &Buildings;
			world.Talents = &Talents;
			return world;
		}
	};

	static PeopleDefinition MakeBuilders()
	{
		PeopleDefinition people;
		people.Name = "Humans";
		people.PeopleId = 1;
		people.SettlementNames = { "Ashford" };
		people.VillagerNames = { "Aldric", "Maren", "Edda" };
		people.FoodPerPerson = 0.5f;
		people.FellPerDay = 1.0f;
		PeopleLevel camp;
		camp.Name = "Camp";
		camp.MaxPeople = 12;
		camp.StoreDays = 4.0f;
		camp.ShelterObjectId = 0; // no shelters to build: only the buildings
		PeopleLevel hamlet = camp;
		hamlet.Name = "Hamlet";
		hamlet.People = 99;
		people.Levels = { camp, hamlet };
		return people;
	}

	int TestBuildingRules(int& checks)
	{
		std::printf("Game rules: BuildingRules\n");
		BuildingWorld world;
		const std::vector<PeopleDefinition> peoples = { MakeBuilders() };
		const PeopleDefinition& humans = peoples[0];
		SettlementState state;
		BUILDING_CHECK(SettlementRules::Found(state, world.View(), peoples, 0, 15, 15, 6) == 1);
		Settlement& home = state.List[0];
		const std::vector<BuildingDefinition>& buildings = world.Buildings;

		// Rows, talents and levels.
		BUILDING_CHECK(BuildingRules::RowOf(buildings, 76, 1) == 1 && BuildingRules::RowOf(buildings, 76, 3) == -1);
		BUILDING_CHECK(BuildingRules::Find(buildings, 3) != nullptr && BuildingRules::Find(buildings, 9) == nullptr);
		BUILDING_CHECK(BuildingRules::Known(buildings[0], world.Talents) == false);
		world.Talents = { 46 };
		BUILDING_CHECK(BuildingRules::Known(buildings[0], world.Talents));
		BUILDING_CHECK(BuildingRules::Allowed(buildings[0], humans, home, world.Talents));
		world.Talents = { 46, 45 };
		BUILDING_CHECK(BuildingRules::Allowed(buildings[2], humans, home, world.Talents) == false); // a barn from a hamlet

		// What they build: the store once it is nearly full, a lodge once they hunt, a barn for fields beyond those worked.
		BuildingRules::Needs needs;
		BUILDING_CHECK(BuildingRules::Wanted(buildings, humans, home, world.Talents, needs) == nullptr);
		needs.StoreFull = true;
		BUILDING_CHECK(BuildingRules::Wanted(buildings, humans, home, world.Talents, needs) == &buildings[0]);
		needs.StoreFull = false;
		needs.Hunting = true;
		BUILDING_CHECK(BuildingRules::Wanted(buildings, humans, home, world.Talents, needs) == nullptr); // lodges not known
		world.Talents.push_back(47);
		BUILDING_CHECK(BuildingRules::Wanted(buildings, humans, home, world.Talents, needs) == &buildings[1]);
		needs.Hunting = false;
		needs.MoreFields = true;
		BUILDING_CHECK(BuildingRules::Wanted(buildings, humans, home, world.Talents, needs) == nullptr);
		home.Level = 1;
		BUILDING_CHECK(BuildingRules::Wanted(buildings, humans, home, world.Talents, needs) == &buildings[2]);
		home.Buildings = { 3 };
		BUILDING_CHECK(BuildingRules::Wanted(buildings, humans, home, world.Talents, needs) == nullptr); // it stands
		home.Buildings = { 1, 2, 3 };
		const BuildingRules::Bonus bonus = BuildingRules::Of(buildings, home);
		BUILDING_CHECK(bonus.StoreDays == 6.0f && bonus.HuntKeep == 0.5f && bonus.Fields == 24);
		home.Buildings.clear();
		home.Level = 0;

		// The panel's lines.
		world.Talents = { 46 };
		const std::vector<BuildingRules::Line> lines = BuildingRules::Lines(buildings, humans, home, world.Talents);
		BUILDING_CHECK(lines.size() == 3 && lines[0].State == "when needed" && lines[1].State == "not known yet");
		BUILDING_CHECK(lines.size() == 3 && lines[2].State == "not known yet");
		world.Talents = { 46, 45 };
		BUILDING_CHECK(BuildingRules::Lines(buildings, humans, home, world.Talents)[2].State == "from level 2");

		// A full store: the settlement plans a granary, a builder puts it up with its wood, and the store holds more.
		Random random(11);
		world.Talents = { 46 };
		home.Count = 6.0f;
		home.Food = 100.0f;
		home.Wood = 10.0f;
		SettlementRules::Update(home, world.View(), peoples, 0.5f, random);
		const float plain = 6.0f * 0.5f * 4.0f;
		BUILDING_CHECK(home.Food <= plain + 0.01f);
		SettlementRules::Report report;
		for (int day = 0; day < 4 && report.BuiltBuilding == 0; day++)
		{
			home.Food = 100.0f;
			report = SettlementRules::Update(home, world.View(), peoples, 1.0f, random);
		}
		BUILDING_CHECK(report.BuiltBuilding == 1 && home.Wood <= 4.01f && home.Planned == 0);
		int standing = 0;
		bool inRing = true;
		for (int y = 0; y < 30; y++)
		{
			for (int x = 0; x < 30; x++)
			{
				const int dx = x - 15;
				const int dy = y - 15;
				const bool granary = world.Objects.Get((size_t)(y * 30 + x)) == 75;
				standing += granary ? 1 : 0;
				inRing = inRing && (granary == false || (dx * dx + dy * dy >= 4 && dx * dx + dy * dy <= 16));
			}
		}
		BUILDING_CHECK(standing == 1 && inRing);
		BUILDING_CHECK(SettlementRules::Look(world.View(), humans, 15, 15).Buildings == std::vector<int>({ 1 }));
		home.Food = 100.0f;
		SettlementRules::Update(home, world.View(), peoples, 0.5f, random);
		const float bigger = home.Count * 0.5f * (4.0f + 6.0f);
		BUILDING_CHECK(home.Buildings == std::vector<int>({ 1 }) && home.Food > plain + 1.0f && home.Food <= bigger + 0.01f);
		BUILDING_CHECK(home.Planned == 0); // nothing else known or needed

		// Hunters make them want a lodge once it is known.
		world.Talents = { 46, 47 };
		home.Villagers[0].Job = VillagerJob::Hunter;
		SettlementRules::Survey survey = SettlementRules::Look(world.View(), humans, 15, 15);
		BUILDING_CHECK(SettlementRules::Plan(home, world.View(), humans, survey) == &buildings[1] && home.Planned == 2);

		// The barn: farmers sow no more fields than they work.
		PeopleDefinition farmers = humans;
		farmers.CropObjectIds = { 72, 73, 74 };
		std::vector<size_t> farmland;
		for (size_t i = 0; i < 20; i++)
		{
			farmland.push_back(i);
		}
		Settlement field = home;
		field.FarmWork = 0.0f;
		const FarmRules::Work sown = FarmRules::Tend(world.View(), farmers, field, farmland, 100.0f, 5);
		BUILDING_CHECK(sown.Sown == 5);
		const FarmRules::Work more = FarmRules::Tend(world.View(), farmers, field, farmland, 100.0f, 8);
		BUILDING_CHECK(more.Sown == 3);
		return s_BuildingFailures;
	}
}
