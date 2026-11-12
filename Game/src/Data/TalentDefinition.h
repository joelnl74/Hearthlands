#pragma once

#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <cstdint>
#include <string>

// The talents as data: one row per talent in Assets/Data/Talents.hytable, bought with points from milestones
// (Assets/Data/Milestones.hytable, MilestoneDefinition.h). TalentRules adds up what the owned ones do.
namespace Hydro
{
	// The column a talent stands in.
	HY_ENUM()
	enum class TalentBranch : uint8_t
	{
		Water,   // rivers, moisture, springs
		Land,    // brushes and terrain types
		Life,    // growth of the land and its plants
		Insight, // seeing what is hidden
		Peoples  // who settles the land, how often, and how they get along
	};

	// What a talent changes; Amount says how much. The owned talents' amounts add up per effect.
	HY_ENUM()
	enum class TalentEffect : uint8_t
	{
		None,
		UnlockTile,    // the brush of the type with TileId = Amount
		MoistureReach, // + tiles fresh water wets the land around it
		SpringHead,    // + head of springs (tiles of narrow channel they fill)
		SpringFrom,    // - groundwater (0 to 1) a dug riverbed needs to spring
		ClueClarity,   // + how plainly the ground shows groundwater (0 to 1)
		ClueFrom,      // - groundwater (0 to 1) from which the ground shows it
		BrushSize,     // + the largest brush radius, in tiles
		GrowthSpeed,   // + share more land looked at each tick (grass, dirt, ...): 0.5 = half again as fast
		LifeSpeed,     // + share more plants and trees looked at each tick
		AnimalRoom,    // + share more animals the land holds: 0.5 = half again as many
		AnimalSight,   // + share more of the animals shown near the camera
		SoilRichness,  // + richness (0 to 1) the soil builds up to where grass, shrubs and trees grow
		Settlers,      // TargetId's people settle on their own: Amount = chance a day a group comes to a new best place
		ForestCreep,   // 1: a forest's shade reaches bare ground too, so forests creep out on their own
		GroundwaterView, // 1: the Inspect bar's groundwater overlay (G)
		OldGrowth,     // 1: grown trees under a closed canopy become ancient in time (old woods)
		FeudCalm,      // + share less tension between peoples who can't stand each other (0.5 = half as fast)
		Evolve,        // 1: TargetId's people (a branch: EvolvesFromId) may arise from the people it branches from
		Building       // TargetId's people may build the building with BuildingId = Amount (Buildings.hytable)
	};

	HY_DATA(Table, DisplayField(Name))
	struct TalentDefinition
	{
		HY_PROPERTY()
		std::string Name;
		HY_PROPERTY(Range(1, 1000), Tooltip("Unique, never reused: the run keeps the ids it owns"))
		int TalentId = 1;
		HY_PROPERTY()
		TalentBranch Branch = TalentBranch::Water;
		HY_PROPERTY(Range(1, 8), Tooltip("Its place from the top in its branch's column"))
		int Row = 1;
		HY_PROPERTY(Range(1, 9), Tooltip("Points it costs"))
		int Cost = 1;
		HY_PROPERTY(Range(-1, 1000), Tooltip("A talent to own first; -1 none"))
		int RequiresTalentId = -1;
		HY_PROPERTY()
		TalentEffect Effect = TalentEffect::None;
		HY_PROPERTY(Range(-100, 100), Tooltip("How much (UnlockTile: the tile id)"))
		float Amount = 0.0f;
		HY_PROPERTY(Range(-1, 255), Tooltip("A milestone (MilestoneId) to reach before it can be learned; -1 none"))
		int RequiresMilestoneId = -1;
		HY_PROPERTY(Range(-1, 255), Tooltip("Settlers, Evolve: the PeopleId it is for (Settlers: their best learned Amount counts)"))
		int TargetId = -1;
		HY_PROPERTY(Tooltip("One short line for the talent panel"))
		std::string Description;
	};
}
