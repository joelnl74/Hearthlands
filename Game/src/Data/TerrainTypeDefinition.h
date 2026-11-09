#pragma once

#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <glm/glm.hpp>

#include <string>

// The terrain types as data: one row per type in Assets/Data/TerrainTypes.hytable. The tile map stores TileId.
namespace Hydro
{
	HY_DATA(Table, DisplayField(Name))
	struct TerrainTypeDefinition
	{
		HY_PROPERTY(Tooltip("Shown in the brush and overlays"))
		std::string Name;
		HY_PROPERTY(Range(0, 255), Tooltip("The number the tile map stores; unique per type, never reused for another type"))
		int TileId = 0;
		HY_PROPERTY(Color, Tooltip("Its colour until it has a texture"))
		glm::vec3 Color{ 1.0f };
		HY_PROPERTY(Range(0, 100), Tooltip("Wins blend ties and overlapping ranges; high for water and roads, so thin lines stay joined"))
		int Priority = 0;
		HY_PROPERTY(Tooltip("A brush from the start of a run; the others unlock as the world grows"))
		bool StartsUnlocked = false;
		HY_PROPERTY(Tooltip("Generation: placed where the height (0 to 1) and moisture (0 to 1) are both in range"))
		bool Generated = true;
		HY_PROPERTY(Range(0, 1))
		float MinHeight = 0.0f;
		HY_PROPERTY(Range(0, 1))
		float MaxHeight = 1.0f;
		HY_PROPERTY(Range(0, 1))
		float MinMoisture = 0.0f;
		HY_PROPERTY(Range(0, 1))
		float MaxMoisture = 1.0f;
	};
}
