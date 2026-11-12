#pragma once

#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <string>

// How land changes by itself: one row per change in Assets/Data/TerrainRules.hytable. Each tick the simulation looks at
// a sample of land tiles; a tile that meets a row's conditions turns into its ToTileId with the row's Chance.
namespace Hydro
{
	HY_DATA(Table, DisplayField(Name))
	struct TerrainRuleDefinition
	{
		HY_PROPERTY()
		std::string Name;
		HY_PROPERTY(Range(0, 255))
		int FromTileId = 1;
		HY_PROPERTY(Range(0, 255))
		int ToTileId = 2;
		HY_PROPERTY(Range(0, 1), Tooltip("The tile's moisture (0 dry to 1 next to fresh water) must be at least this"))
		float MinMoisture = 0.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("... and at most this"))
		float MaxMoisture = 1.0f;
		HY_PROPERTY(Range(-1, 255), Tooltip("Needs MinNeighbours of its 8 neighbours to be this id; -1 needs none"))
		int NeighbourTileId = -1;
		HY_PROPERTY(Range(1, 8))
		int MinNeighbours = 1;
		HY_PROPERTY(Range(0, 1), Tooltip("The chance it happens each time the tile is looked at"))
		float Chance = 0.1f;
		HY_PROPERTY(Range(-1, 1), Tooltip("How much the soil moves the chance: + more on rich soil (up to 1 + this), - more on poor"))
		float SoilEffect = 0.0f;
	};
}
