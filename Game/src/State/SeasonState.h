#pragma once

#include <glm/glm.hpp>

#include <string>

namespace Hydro
{
	// The season now: which one, how far into the next it has blended, and the blended values the simulation and the
	// look use (SeasonRules::At, every frame). Neutral without seasons.
	struct SeasonState
	{
		std::string Name;   // the season's (Index), for the HUD
		int Index = 0;      // the season's row (in Order) ...
		int Next = 0;       // ... and the one after it
		float Blend = 0.0f; // 0 all Index, 1 all Next
		int Day = 1;        // the day of the season, from 1
		int Year = 1;
		float SnowShift = 0.0f;
		float Flow = 1.0f;
		float Melt = 1.0f;
		float Growth = 1.0f;
		float Life = 1.0f;
		float Harvest = 1.0f; // wild food (blended)
		bool Sowing = true;   // the season's (not blended): farmers sow
		bool Frost = false;   // crops wither
		float Migrants = 1.0f;
		float Burrowed = 0.0f;
		float GroundSnow = 0.0f;
		float Ice = 0.0f;
		float Fade = 0.0f;
		glm::vec3 Tint{ 1.0f };
		glm::vec3 LeafTint{ 1.0f };
	};
}
