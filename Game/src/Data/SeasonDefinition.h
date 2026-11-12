#pragma once

#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <glm/glm.hpp>

#include <string>

// The seasons as data: one row per season in Assets/Data/Seasons.hytable, in Order through the year. SeasonRules blends
// each into the next over its last days; the simulation and the look read the blend (SeasonState).
namespace Hydro
{
	HY_DATA(Table, DisplayField(Name))
	struct SeasonDefinition
	{
		HY_PROPERTY()
		std::string Name;
		HY_PROPERTY(Range(0, 15), Tooltip("Its place in the year, from 0"))
		int Order = 0;
		HY_PROPERTY(Range(1, 60), Tooltip("How many days it lasts"))
		float Days = 5.0f;
		HY_PROPERTY(Range(-8, 8), Tooltip("Levels the snowline moves: down (less) in winter, up in summer; never below rock"))
		float SnowShift = 0.0f;
		HY_PROPERTY(Range(0.2, 3), Tooltip("How far rivers and springs reach: 1 as set, more in a wet spring"))
		float Flow = 1.0f;
		HY_PROPERTY(Range(0, 3), Tooltip("Meltwater from the snow: its head and the moisture below it, times this"))
		float Melt = 1.0f;
		HY_PROPERTY(Range(0, 3), Tooltip("How fast the land changes (grass spreading, drying), times this"))
		float Growth = 1.0f;
		HY_PROPERTY(Range(0, 3), Tooltip("How fast plants and trees sprout and spread, times this"))
		float Life = 1.0f;
		HY_PROPERTY(Range(0, 2), Tooltip("Wild food the plants give (berries), times this: little in winter"))
		float Harvest = 1.0f;
		HY_PROPERTY(Tooltip("Farmers sow in this season"))
		bool Sowing = true;
		HY_PROPERTY(Tooltip("Frost: crops not yet ripe wither, ripe ones left in the field rot"))
		bool Frost = false;
		HY_PROPERTY(Range(0, 1), Tooltip("Share of the migrating animals (Migrates) the land holds: few in winter"))
		float Migrants = 1.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("Share of the hibernating animals (Hibernates) asleep, out of sight"))
		float Burrowed = 0.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("Snow lying on the ground (a look only): 0 none, 1 nearly all"))
		float GroundSnow = 0.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("Ice on the water, from the shores in (a look only)"))
		float Ice = 0.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("Green turning to straw on the land (a look only): autumn's dry grass"))
		float Fade = 0.0f;
		HY_PROPERTY(Color, Tooltip("The land's colour, times this"))
		glm::vec3 Tint{ 1.0f };
		HY_PROPERTY(Color, Tooltip("Leafy plants and trees (Deciduous objects), times this: autumn colours, bare winter"))
		glm::vec3 LeafTint{ 1.0f };
	};
}
