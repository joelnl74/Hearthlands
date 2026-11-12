// Tools/Tests/SettlingRulesTest.cpp
// Settlements that come on their own (SettlingRules): talents' chances, the best place, new groups less often for each
// settlement, branching out; and the feuds between peoples who can't stand each other (FeudRules). Run from TestGameRules.

#include "hypch.h"

#include "Rules/FeudRules.h"
#include "Rules/ObjectRules.h"
#include "Rules/SettlementRules.h"
#include "Rules/SettlingRules.h"
#include "Rules/TalentRules.h"

#include <cstdio>
#include <vector>

namespace Hydro
{
	static int s_SettlingFailures = 0;

	static void SettlingCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_SettlingFailures++;
			std::printf("FAIL  SettlingRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define SETTLING_CHECK(condition) SettlingCheck((condition), #condition, __LINE__, checks)

	static ObjectDefinition MakeSettlingObject(int id, ObjectKind kind, float food, float wood)
	{
		ObjectDefinition object;
		object.Name = "Object " + std::to_string(id);
		object.ObjectId = id;
		object.Kind = kind;
		object.GrowsOnTileId = -1;
		object.Food = food;
		object.Wood = wood;
		return object;
	}

	// A 96 x 48 grass map with two good places: a lake with bushes and six trees round it in the west (around 20, 24) and
	// a smaller one with three trees in the east (around 76, 24); dry, bare land between.
	struct SettlingWorld
	{
		TileMap Map{ 96, 48, 2 };
		Hydrology Water;
		ObjectLayer Objects;
		std::vector<ObjectDefinition> Definitions = { MakeSettlingObject(1, ObjectKind::Shrub, 0.8f, 0.0f),
			MakeSettlingObject(2, ObjectKind::Tree, 0.0f, 3.0f), MakeSettlingObject(60, ObjectKind::Building, 0.0f, 0.0f),
			MakeSettlingObject(61, ObjectKind::Building, 0.0f, 0.0f), MakeSettlingObject(62, ObjectKind::Building, 0.0f, 0.0f),
			MakeSettlingObject(63, ObjectKind::Building, 0.0f, 0.0f) };
		std::vector<int> ById;
		std::vector<uint8_t> Land;

		SettlingWorld()
		{
			Objects.Reset(96 * 48);
			Oasis(20, 24, 4, 6);
			Oasis(76, 24, 3, 3);
			ById = ObjectRules::IndexById(Definitions);
			Land.assign(96 * 48, 1);
		}

		// A lake of `size` x `size` at (x, y), bushes in a row above it and `trees` below.
		void Oasis(int x, int y, int size, int trees)
		{
			for (int dy = 0; dy < size; dy++)
			{
				for (int dx = 0; dx < size; dx++)
				{
					Map.Set(x + dx, y + dy, 3);
				}
			}
			for (int i = 0; i < 6; i++)
			{
				Objects.Set((size_t)(y - 5) * 96 + (size_t)(x - 5 + i * 2), 1);
				Objects.Set((size_t)(y + size + 4) * 96 + (size_t)(x - 5 + i * 2), i < trees ? (uint8_t)2 : (uint8_t)0);
			}
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
			return world;
		}
	};

	static PeopleDefinition MakeSettlers(const char* name, int id, int centre, int shelter)
	{
		PeopleDefinition people;
		people.Name = name;
		people.PeopleId = id;
		people.SettlementNames = { std::string(name) + " One", std::string(name) + " Two", std::string(name) + " Three" };
		people.VillagerNames = { "A", "B", "C", "D" };
		people.CentreObjectId = centre;
		people.NeedWood = 0.0f;
		people.GroupMin = 3;
		people.GroupMax = 3;
		people.FoodPerPerson = 0.5f;
		people.SettleSpacing = 30;
		people.BranchReach = 70;
		PeopleLevel first;
		first.Name = "Camp";
		first.ShelterObjectId = shelter;
		first.WoodPerShelter = 0.0f;
		first.MaxPeople = 6;
		PeopleLevel second = first;
		second.Name = "Hamlet";
		second.People = 6;
		second.MaxPeople = 9;
		people.Levels = { first, second };
		return people;
	}

	int TestSettlingRules(int& checks)
	{
		std::printf("Game rules: SettlingRules, FeudRules\n");
		SettlingWorld world;
		std::vector<PeopleDefinition> peoples = { MakeSettlers("Humans", 1, 60, 61), MakeSettlers("Elves", 2, 62, 63) };

		// Talents: a people settles once a Settlers talent for it is owned; a better one replaces it (the best counts).
		TalentDefinition firstFire;
		firstFire.TalentId = 14;
		firstFire.Effect = TalentEffect::Settlers;
		firstFire.Amount = 0.05f;
		firstFire.TargetId = 1;
		TalentDefinition wanderers = firstFire;
		wanderers.TalentId = 40;
		wanderers.Amount = 0.2f;
		wanderers.RequiresTalentId = 14;
		TalentDefinition calm;
		calm.TalentId = 43;
		calm.Effect = TalentEffect::FeudCalm;
		calm.Amount = 0.5f;
		const std::vector<TalentDefinition> talents = { firstFire, wanderers, calm };
		TalentState owned;
		owned.Points = 10;
		SETTLING_CHECK(owned.SettleChance(1) == 0.0f);
		TalentRules::Buy(talents, owned, 14);
		SETTLING_CHECK(owned.SettleChance(1) == 0.05f && owned.SettleChance(2) == 0.0f && owned.Get(TalentEffect::Settlers) == 0.0f);
		TalentRules::Buy(talents, owned, 40);
		TalentRules::Buy(talents, owned, 43);
		SETTLING_CHECK(owned.SettleChance(1) == 0.2f && owned.Get(TalentEffect::FeudCalm) == 0.5f && owned.SettleChance(0) == 0.0f);

		// Chances: none without a talent; their first soon (ArrivePerDay); then less for each settlement they have.
		SettlementState state;
		SETTLING_CHECK(SettlingRules::GroupChance(state, peoples[0], 0, 0.0f) == 0.0f);
		SETTLING_CHECK(SettlingRules::GroupChance(state, peoples[0], 0, 0.1f) == 0.5f);

		// The best place: by the bigger lake, its centre on free land keeping to the people's needs.
		int x = 0;
		int y = 0;
		SETTLING_CHECK(SettlingRules::Find(world.View(), state, peoples[0], -1, -1, 0, x, y));
		SETTLING_CHECK(x < 48 && SettlementRules::Meets(peoples[0], SettlementRules::Look(world.View(), peoples[0], x, y)));

		// A step with no talent: nobody comes. With one (ArrivePerDay 1 over a day): the first group comes there.
		Random random(3);
		peoples[0].ArrivePerDay = 1.0f;
		SettlingRules::Report report = SettlingRules::Step(state, world.View(), peoples, { 0.0f, 0.0f }, 1.0f, random);
		SETTLING_CHECK(report.Arrivals.empty() && state.List.empty() && state.SearchWait.size() == 2);
		report = SettlingRules::Step(state, world.View(), peoples, { 0.1f, 0.0f }, 1.0f, random);
		SETTLING_CHECK(report.Arrivals.size() == 1 && report.Arrivals[0].Row == 0 && report.Arrivals[0].From == 0);
		SETTLING_CHECK(state.List.size() == 1 && state.List[0].X == x && state.List[0].Y == y && state.List[0].Villagers.size() == 3);
		SETTLING_CHECK(world.Objects.Get((size_t)y * 96 + (size_t)x) == 60 && state.List[0].Name == "Humans One");
		SETTLING_CHECK(SettlingRules::GroupChance(state, peoples[0], 0, 0.1f) == 0.05f); // halved for the one they have
		SETTLING_CHECK(SettlingRules::Spaced(state, x + 3, y, 30) == false && SettlingRules::Spaced(state, x + 40, y, 30));

		// The next best place keeps its distance: the eastern lake.
		SETTLING_CHECK(SettlingRules::Find(world.View(), state, peoples[0], -1, -1, 0, x, y) && x > 48);
		const int second = SettlementRules::Found(state, world.View(), peoples, 0, x, y, 3);
		SETTLING_CHECK(second == 2 && state.List[1].Name == "Humans Two");

		// No place left (both taken, the rest dry): they say so and wait before looking again.
		report = SettlingRules::Step(state, world.View(), peoples, { 100.0f, 0.0f }, 1.0f, random);
		SETTLING_CHECK(report.Arrivals.empty() && report.NoPlace.size() == 1 && state.SearchWait[0] == SettlingRules::NoPlaceWait);
		report = SettlingRules::Step(state, world.View(), peoples, { 100.0f, 0.0f }, 1.0f, random);
		SETTLING_CHECK(report.NoPlace.empty() && state.SearchWait[0] == SettlingRules::NoPlaceWait - 1.0f);

		// Branching: a full settlement of level 1 with food sends three villagers to the best place within reach.
		state.List.pop_back();
		world.Objects.Set((size_t)y * 96 + (size_t)x, 0);
		Settlement& home = state.List[0];
		home.Level = 1;
		home.Count = 9.0f;
		home.Room = 9;
		home.Food = 20.0f;
		SettlementRules::SyncVillagers(home, peoples[0]);
		const std::string mover = home.Villagers.back().Name;
		peoples[0].BranchPerDay = 1.0f;
		state.SearchWait[0] = 50.0f; // no new groups: only the branch
		report = SettlingRules::Step(state, world.View(), peoples, { 0.1f, 0.0f }, 1.0f, random);
		SETTLING_CHECK(report.Arrivals.size() == 1 && report.Arrivals[0].From == state.List[0].Id && state.List.size() == 2);
		SETTLING_CHECK(state.List[0].Count == 6.0f && state.List[0].Villagers.size() == 6 && state.List[1].Count == 3.0f);
		SETTLING_CHECK(state.List[1].Villagers.size() == 3 && state.List[1].Villagers[2].Name == mover && state.List[1].X > 48);
		SETTLING_CHECK(state.List[1].Food > 0.0f && state.List[0].Food < 20.0f);
		// Not full, or below BranchFromLevel: no branch.
		state.List[0].Room = 12;
		report = SettlingRules::Step(state, world.View(), peoples, { 0.1f, 0.0f }, 1.0f, random);
		SETTLING_CHECK(report.Arrivals.empty());

		// Feuds: the elves dislike the humans here. Tension builds much faster close than far, none beyond the radius.
		peoples[1].Dislikes = { 1 };
		peoples[1].DislikeRadius = 32;
		peoples[1].TensionPerDay = 0.2f;
		SETTLING_CHECK(FeudRules::Dislikes(peoples, 1, 0) && FeudRules::Dislikes(peoples, 0, 1) == false);
		const float close = FeudRules::Rate(peoples[1], 2.0f, 0.0f);
		const float distant = FeudRules::Rate(peoples[1], 28.0f, 0.0f);
		SETTLING_CHECK(close > distant * 4.0f && FeudRules::Rate(peoples[1], 2.0f, 0.5f) < close * 0.51f);
		SettlementState feud;
		const int humansAt = SettlementRules::Found(feud, world.View(), peoples, 0, 18, 30, 6);
		const int elvesAt = SettlementRules::Found(feud, world.View(), peoples, 1, 22, 31, 6);
		SETTLING_CHECK(humansAt == 1 && elvesAt == 2); // close beside each other: no limits on where they live
		float distance = 0.0f;
		SETTLING_CHECK(FeudRules::Nearest(feud, peoples, 1, distance) == 0 && distance < 5.0f);
		SETTLING_CHECK(FeudRules::Nearest(feud, peoples, 0, distance) == -1); // the humans don't mind
		std::vector<FeudRules::Incident> incidents = FeudRules::Step(feud, world.View(), peoples, 0.0f, 1.0f, random);
		SETTLING_CHECK(feud.List[1].Tension > 0.1f && feud.List[1].FeudWith == humansAt && feud.List[0].Tension == 0.0f);
		incidents = FeudRules::Step(feud, world.View(), peoples, 0.0f, 1.0f, random);
		bool quarrel = false;
		for (const FeudRules::Incident& incident : incidents)
		{
			quarrel = quarrel || (incident.Kind == FeudRules::IncidentKind::Quarrel && incident.From == elvesAt && incident.To == humansAt);
		}
		SETTLING_CHECK(quarrel);

		// High tension: raids take food (or burn a shelter), fights hurt, and at the very top someone dies; each eases the
		// attackers' tension.
		feud.List[0].Food = 30.0f;
		feud.List[1].Tension = 1.0f;
		int stolen = 0;
		int fights = 0;
		int deaths = 0;
		bool vented = false;
		const float before = feud.List[0].Count;
		for (int day = 0; day < 80; day++)
		{
			feud.List[0].Food = std::max(feud.List[0].Food, 10.0f);
			const float raging = feud.List[1].Tension;
			feud.List[1].Tension = 1.0f;
			for (const FeudRules::Incident& incident : FeudRules::Step(feud, world.View(), peoples, 0.0f, 0.5f, random))
			{
				stolen += incident.Kind == FeudRules::IncidentKind::Stolen ? 1 : 0;
				fights += incident.Kind == FeudRules::IncidentKind::Fight ? 1 : 0;
				deaths += incident.Kind == FeudRules::IncidentKind::Death ? 1 : 0;
				SETTLING_CHECK(incident.Kind != FeudRules::IncidentKind::Death || incident.Who.empty() == false);
			}
			vented = vented || feud.List[1].Tension < 1.0f - 1e-4f || raging < 1.0f;
		}
		SETTLING_CHECK(stolen >= 1 && fights >= 1 && deaths >= 1 && vented);
		SETTLING_CHECK(feud.List[0].Count == before - (float)deaths && feud.List[0].Villagers.size() == (size_t)(feud.List[0].Count + 0.5f));
		bool hurt = false;
		for (const Villager& villager : feud.List[1].Villagers)
		{
			hurt = hurt || villager.HurtDays > 0.0f;
		}
		SETTLING_CHECK(hurt || fights >= 1);

		// Guards: with more guards than raiders, raids mostly fail.
		Settlement raiders = feud.List[1];
		Settlement guarded = feud.List[0];
		guarded.Count = 6.0f;
		SettlementRules::SyncVillagers(guarded, peoples[0]);
		for (Villager& villager : guarded.Villagers)
		{
			villager.Job = VillagerJob::Guard;
			villager.HurtDays = 0.0f;
		}
		int repelled = 0;
		std::vector<FeudRules::Incident> raids;
		for (int i = 0; i < 40; i++)
		{
			FeudRules::Raid(raiders, guarded, world.View(), peoples, random, raids);
		}
		for (const FeudRules::Incident& incident : raids)
		{
			repelled += incident.Kind == FeudRules::IncidentKind::Repelled ? 1 : 0;
		}
		SETTLING_CHECK(repelled > 20);

		// A settlement removed leaves a ruin: new groups keep away until it is forgotten.
		SettlementState ruined;
		SETTLING_CHECK(SettlementRules::Found(ruined, world.View(), peoples, 0, 40, 10, 3) == 1);
		SettlingRules::Remove(ruined, 0);
		SETTLING_CHECK(ruined.List.empty() && ruined.Ruins.size() == 1 && SettlingRules::Spaced(ruined, 42, 10, 30) == false);
		world.Objects.Set(10 * 96 + 40, 0);
		SettlingRules::Step(ruined, world.View(), peoples, { 0.0f, 0.0f }, SettlingRules::RuinDays + 1.0f, random);
		SETTLING_CHECK(ruined.Ruins.empty() && SettlingRules::Spaced(ruined, 42, 10, 30));
		// A disliked neighbour spoils a place, more the closer: 0 beyond its radius.
		SETTLING_CHECK(SettlingRules::FoePenalty(feud, peoples, peoples[1], 20, 30) > 1.5f);
		SETTLING_CHECK(SettlingRules::FoePenalty(feud, peoples, peoples[1], 80, 30) == 0.0f);
		SETTLING_CHECK(SettlingRules::FoePenalty(feud, peoples, peoples[0], 20, 30) == 0.0f);

		// Apart (the humans' fire moved far off): tension fades and the elves calm down.
		feud.List[0].X = 90;
		feud.List[0].Y = 2;
		feud.List[1].Tension = 0.3f;
		incidents = FeudRules::Step(feud, world.View(), peoples, 0.0f, 2.0f, random);
		SETTLING_CHECK(feud.List[1].Tension < 0.25f && incidents.size() == 1 && incidents[0].Kind == FeudRules::IncidentKind::Calmed);

		// Elves and their branch, wood elves: elves plant saplings (foresters); by old woods, once a talent allows it, an
		// elf settlement becomes wood elves (no wood elf group ever comes on its own).
		SettlingWorld grove;
		grove.Definitions.push_back(MakeSettlingObject(5, ObjectKind::Tree, 0.0f, 0.0f));
		grove.Definitions.back().GrowsOnTileId = 2;
		grove.Definitions.push_back(MakeSettlingObject(24, ObjectKind::Tree, 0.4f, 8.0f));
		grove.Definitions.push_back(MakeSettlingObject(69, ObjectKind::Building, 0.0f, 0.0f));
		grove.Definitions.push_back(MakeSettlingObject(70, ObjectKind::Building, 0.0f, 0.0f));
		grove.ById = ObjectRules::IndexById(grove.Definitions);
		PeopleDefinition elves = MakeSettlers("Elves", 4, 70, 69);
		elves.PlantObjectIds = { 5 };
		elves.PlantPerDay = 2.0f;
		elves.ForesterShare = 0.3f;
		PeopleDefinition woodElves = MakeSettlers("Wood elves", 2, 62, 63);
		woodElves.EvolvesFromId = 4;
		woodElves.EvolvePerDay = 1.0f;
		woodElves.NeedObjectIds = { 24 };
		woodElves.NeedObjects = 2;
		woodElves.NeedObjectsName = "old trees";
		woodElves.ArrivePerDay = 1.0f;
		const std::vector<PeopleDefinition> kin = { elves, woodElves };
		SETTLING_CHECK(SettlingRules::BranchOf(kin, 0) == 1 && SettlingRules::BranchOf(kin, 1) == -1);
		SettlementState glade;
		const int glen = SettlementRules::Found(glade, grove.View(), kin, 0, 20, 30, 6);
		SETTLING_CHECK(glen == 1);
		Settlement& home2 = glade.List[0];
		// Foresters: some of the six tend the forest and plant saplings beyond the shelters, on the grass.
		SettlementRules::AssignJobs(home2, elves, SettlementRules::Look(grove.View(), elves, 20, 30));
		SETTLING_CHECK(SettlementRules::Workers(home2, VillagerJob::Forester) >= 1);
		int planted = 0;
		for (int day = 0; day < 6; day++)
		{
			planted += SettlementRules::Update(home2, grove.View(), kin, 0.5f, random).Planted;
		}
		int saplings = 0;
		for (uint8_t id : grove.Objects.GetIds())
		{
			saplings += id == 5 ? 1 : 0;
		}
		SETTLING_CHECK(planted >= 2 && saplings == planted && std::string(SettlementRules::JobName(VillagerJob::Forester)) == "Forester");
		// A wood elf branch never comes as a group; no old trees, or no talent: no change, but the panel's count is kept.
		report = SettlingRules::Step(glade, grove.View(), kin, { 0.0f, 100.0f }, 1.0f, random);
		SETTLING_CHECK(report.Arrivals.empty() && glade.List.size() == 1 && report.Evolved.empty());
		SETTLING_CHECK(glade.List[0].EvolveTo == 1 && glade.List[0].EvolveWant == 2 && glade.List[0].EvolveHave == 0);
		SETTLING_CHECK(glade.List[0].EvolveAllowed);
		grove.Objects.Set(36 * 96 + 26, 24);
		grove.Objects.Set(37 * 96 + 14, 24); // two ancient trees near
		report = SettlingRules::Step(glade, grove.View(), kin, { 0.0f, 0.0f }, 1.0f, random);
		SETTLING_CHECK(report.Evolved.empty() && glade.List[0].EvolveHave == 2 && glade.List[0].EvolveAllowed == false);
		// Allowed: they become wood elves; the hearth a lantern, bowers leaf huts; name and villagers stay.
		const size_t villagers = glade.List[0].Villagers.size();
		grove.Objects.Set(30 * 96 + 22, 69); // a bower
		report = SettlingRules::Step(glade, grove.View(), kin, { 0.0f, 1.0f }, 1.0f, random);
		SETTLING_CHECK(report.Evolved.size() == 1 && report.Evolved[0].From == 0 && report.Evolved[0].To == 1);
		SETTLING_CHECK(glade.List[0].People == 1 && glade.List[0].Name == "Elves One" && glade.List[0].Villagers.size() == villagers);
		SETTLING_CHECK(grove.Objects.Get(30 * 96 + 20) == 62 && grove.Objects.Get(30 * 96 + 22) == 63 && glade.List[0].EvolveTo == -1);
		return s_SettlingFailures;
	}
}
