#pragma once

#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>

// The terrain types as data: one row per type in Assets/Data/TerrainTypes.hytable. The tile map stores TileId.
namespace Hydro
{
	// What the simulation does with a type.
	HY_ENUM()
	enum class TerrainRole : uint8_t
	{
		Land,     // grows and changes by the terrain rules
		Water,    // carries water: a strength that drops along it (the sea and generated lakes hold it)
		Riverbed  // dug, empty channel: water flows into it from water it touches
	};

	HY_DATA(Table, DisplayField(Name))
	struct TerrainTypeDefinition
	{
		HY_PROPERTY(Tooltip("Shown in overlays"))
		std::string Name;
		HY_PROPERTY(Tooltip("The brush's label when it isn't the name (Dig for the riverbed)"))
		std::string BrushName;
		HY_PROPERTY(Range(0, 255), Tooltip("The number the tile map stores; unique per type, never reused for another type"))
		int TileId = 0;
		HY_PROPERTY()
		TerrainRole Role = TerrainRole::Land;
		HY_PROPERTY(Color, Tooltip("Its colour until it has a texture"))
		glm::vec3 Color{ 1.0f };
		HY_PROPERTY(Range(0, 100), Tooltip("Wins blend ties and overlapping ranges; high for water and roads, so thin lines stay joined"))
		int Priority = 0;
		HY_PROPERTY(Tooltip("A brush from the start of a run"))
		bool StartsUnlocked = false;
		HY_PROPERTY(Range(-1, 255), Tooltip("Becomes a brush once the map holds UnlockCount tiles of this id; -1 never"))
		int UnlockTileId = -1;
		HY_PROPERTY(Range(0, 100000))
		int UnlockCount = 0;
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
