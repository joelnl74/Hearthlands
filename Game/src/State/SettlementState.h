#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// What a villager does, chosen by the settlement each day (SettlementRules::AssignJobs): the player never sets it.
	enum class VillagerJob : uint8_t
	{
		Resting,
		Gatherer,   // berries and plants
		Hunter,     // game in the region
		Woodcutter, // fells trees while they need wood
		Builder,    // puts up shelters once there is wood
		Guard,      // stands watch while tension with a disliked people is high
		Hurt,       // can't work until healed
		Forester,   // plants saplings where they can grow (elves)
		Farmer      // sows and harvests the farmland the player painted (humans, from a hamlet)
	};

	// What the player asks a settlement to lean towards; the villagers still choose their jobs.
	enum class SettlementFocus : uint8_t
	{
		None,
		Food,
		Wood,
		Building
	};

	struct Villager
	{
		int Id = 0;              // unique in its settlement
		std::string Name;
		VillagerJob Job = VillagerJob::Resting;
		float HurtDays = 0.0f;   // days until healed (Hurt while above 0)
		float Days = 0.0f;       // lived in this settlement
	};

	// One settlement: a people who came on their own, its villagers, stores, level and what its land offers.
	struct Settlement
	{
		int Id = 0;
		std::string Name;
		int NameIndex = 0;       // into its people's SettlementNames (Rename moves it on)
		int X = 0;               // the centre's tile (fire, lantern, mine)
		int Y = 0;
		int People = 0;          // the row of the people living there (Peoples table order)
		int Level = 0;           // into its people's Levels
		float Count = 0.0f;      // people (villagers follow it, rounded)
		float Food = 0.0f;       // in store
		float Wood = 0.0f;
		float Felling = 0.0f;    // trees owed (fractions carried to the next update)
		float Building = 0.0f;   // builders' days towards the next shelter
		float Planting = 0.0f;   // saplings owed (fractions carried)
		float FarmWork = 0.0f;   // farmers' days owed (fractions carried)
		int Fields = 0;          // farmland within reach, counted on the last update ...
		int FieldsGrowing = 0;   // ... sown and growing
		int FieldsRipe = 0;      // ... ripe
		int Harvests = 0;        // updates in which it harvested (the first brings news)
		int Shelters = 0;        // counted on the last update
		int Room = 0;            // people its centre and shelters hold, at most the level's MaxPeople
		int OldShelters = 0;     // shelters from below its level (tents in a hamlet): builders rebuild them with spare wood
		float Water = 0.0f;      // what the land offers, 0 to 1 against their needs (the last update's survey)
		float FoodNearby = 0.0f;
		float WoodNearby = 0.0f;
		int Needed = 0;          // the people's NeedObjects within reach (old trees), counted
		int NeededTiles = 0;     // the people's NeedTiles within reach (mountain rock), counted
		std::vector<Villager> Villagers;
		int NextVillager = 1;
		int VillagerNames = 0;   // the next of its people's VillagerNames
		SettlementFocus Focus = SettlementFocus::None;
		float Tension = 0.0f;    // 0 to 1 with the nearest disliked settlement (FeudRules)
		int FeudWith = 0;        // that settlement's Id; 0 none near
		float Days = 0.0f;       // since they came
		int EvolveTo = -1;       // the row of a branch of its people it could become (wood elves); -1 none
		int EvolveHave = 0;      // what that branch needs near, counted (old trees) ...
		int EvolveWant = 0;      // ... and how many
		bool EvolveAllowed = false; // a talent lets that branch arise
		std::vector<int> Buildings; // BuildingIds standing near the centre, found on the last update (BuildingRules)
		int Planned = 0;            // the BuildingId the builders put up next (0 none): known, allowed and needed
		int FieldsMost = 0;         // fields the farmers work: their people's FieldsWorked and a barn's (FarmRules)
	};

	// Where a settlement stood until it emptied: new groups keep away while the memory lasts.
	struct Ruin
	{
		int X = 0;
		int Y = 0;
		float Days = 0.0f; // left
	};

	// The run's settlements: run state on the session, changed by SettlingRules, SettlementRules and FeudRules.
	struct SettlementState
	{
		std::vector<Settlement> List;
		int NextId = 1;
		std::vector<float> SearchWait; // per people row: days until they look for a new place again (none was found)
		std::vector<int> Founded;      // per people row: settlements founded this run (the next takes the next name)
		std::vector<Ruin> Ruins;
		uint64_t Revision = 0;         // moves on every change the panel shows
	};
}
