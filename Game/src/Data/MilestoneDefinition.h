#pragma once

#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <cstdint>
#include <string>

// The milestones as data: one row per first-time event in Assets/Data/Milestones.hytable; each gives talent points once.
namespace Hydro
{
	// What a milestone counts; it is reached once the count reaches Count.
	HY_ENUM()
	enum class MilestoneKind : uint8_t
	{
		Tiles,   // tiles of the type with TileId = TargetId
		Objects, // objects of ObjectKind TargetId (0 ground plants, 1 shrubs, 2 trees, 3 rocks)
		Springs, // springs dug (riverbed over groundwater)
		Level,   // the highest land, in levels (Count is the level)
		Animals, // animals of the species with AnimalId = TargetId living on the map; -1 all
		Species, // species with at least one animal on the map: of AnimalKind TargetId (1 birds, 2 fish); -1 all
		RichSoil, // land tiles whose soil quality is at least TargetId percent
		Settlers, // people living in the settlements: of the people with PeopleId = TargetId; -1 all
		Ancient,  // ancient trees (objects whose row is Ancient: old woods)
		Habitat,  // water tiles of the WaterHabitat numbered TargetId (2 stream, 3 river, 4 lake, 7 joined to the sea, ...)
		SettlementLevel // the best level a settlement of the people with PeopleId = TargetId reached (1 a hamlet); -1 any
	};

	// Where the Milestones page lists a milestone.
	HY_ENUM()
	enum class MilestoneTrack : uint8_t
	{
		Auto,     // by its Kind
		Land,
		Water,
		Woods,
		Wildlife,
		Peoples
	};

	HY_DATA(Table, DisplayField(Name))
	struct MilestoneDefinition
	{
		HY_PROPERTY()
		std::string Name;
		HY_PROPERTY(Range(1, 1000), Tooltip("Unique, never reused: the run keeps the ids it reached"))
		int MilestoneId = 1;
		HY_PROPERTY()
		MilestoneKind Kind = MilestoneKind::Tiles;
		HY_PROPERTY(Range(-1, 255), Tooltip("Tiles: tile id; Objects: kind; Animals: AnimalId (-1 all); Species: kind; Habitat: number"))
		int TargetId = -1;
		HY_PROPERTY(Range(1, 1000000), Tooltip("How many it takes"))
		int Count = 1;
		HY_PROPERTY(Range(1, 9), Tooltip("Talent points it gives"))
		int Points = 1;
		HY_PROPERTY(Tooltip("One short line: what to do to reach it"))
		std::string Description;
		HY_PROPERTY(Tooltip("A discovery: a ? card (and no tips) until it is reached"))
		bool Hidden = false;
		HY_PROPERTY(Tooltip("Hidden: a riddle shown on its ? card instead of the Description; empty none"))
		std::string Hint;
		HY_PROPERTY()
		MilestoneTrack Track = MilestoneTrack::Auto;
	};
}
