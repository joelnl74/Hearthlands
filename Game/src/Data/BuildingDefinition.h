#pragma once

#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <cstdint>
#include <string>

// The buildings a people puts up besides its shelters, as data: one row per people and building in
// Assets/Data/Buildings.hytable. A talent makes one known; the settlement builds it itself when it needs it (BuildingRules).
namespace Hydro
{
	// What a building is for. Without it, what it gives is only a little (a small store, the meat a hunter eats that day,
	// few fields worked); the building lifts that cap.
	HY_ENUM()
	enum class BuildingKind : uint8_t
	{
		Store,        // food lasts: StoreDays more days of food fit in the store (built when the store is nearly full)
		HuntingLodge, // the hunt kept: HuntKeep more of each animal's meat (built once they hunt)
		Barn,         // more fields: Fields more fields the farmers work (built when there is more farmland than they work)
		FishingLodge  // fishing: Fishers villagers fish (without it, none; built once fish live in water within reach)
	};

	// Where a building goes up.
	HY_ENUM()
	enum class BuildingSite : uint8_t
	{
		Ring, // RingFrom to RingTo tiles from the centre
		Shore // beside water within the settlement's reach, where most fish are (from RingFrom tiles out)
	};

	HY_DATA(Table, DisplayField(Name))
	struct BuildingDefinition
	{
		HY_PROPERTY()
		std::string Name;
		HY_PROPERTY(Range(1, 1000), Tooltip("Unique, never reused: saves keep a settlement's buildings by it"))
		int BuildingId = 1;
		HY_PROPERTY(Range(1, 255), Tooltip("The people that builds it (PeopleId in Peoples.hytable)"))
		int PeopleId = 1;
		HY_PROPERTY()
		BuildingKind Kind = BuildingKind::Store;
		HY_PROPERTY(Range(1, 255), Tooltip("The object put up (a Building in Objects.hytable)"))
		int ObjectId = 1;
		HY_PROPERTY(Range(-1, 1000), Tooltip("The talent that makes it known (-1: known from the start)"))
		int RequiresTalentId = -1;
		HY_PROPERTY(Range(0, 10), Tooltip("The settlement's level from which they build it (0 the first)"))
		int MinLevel = 0;
		HY_PROPERTY(Range(0, 100), Tooltip("Wood it takes"))
		float Wood = 6.0f;
		HY_PROPERTY()
		BuildingSite Site = BuildingSite::Ring;
		HY_PROPERTY(Range(2, 30), Tooltip("Tiles from the centre it stands at, at least ..."))
		int RingFrom = 2;
		HY_PROPERTY(Range(2, 30), Tooltip("... and at most"))
		int RingTo = 4;
		HY_PROPERTY(Range(0, 100), Tooltip("Store: days of food more the store holds"))
		float StoreDays = 0.0f;
		HY_PROPERTY(Range(0, 5), Tooltip("Hunting lodge: share more meat from each animal hunted (0.5: half again)"))
		float HuntKeep = 0.0f;
		HY_PROPERTY(Range(0, 200), Tooltip("Barn: fields more the farmers work"))
		int Fields = 0;
		HY_PROPERTY(Range(0, 20), Tooltip("Fishing lodge: villagers who fish, at most"))
		int Fishers = 0;
		HY_PROPERTY(Tooltip("One short line for the settlement panel and the talent"))
		std::string Description;
	};
}
