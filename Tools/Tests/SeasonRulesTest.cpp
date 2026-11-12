// Tools/Tests/SeasonRulesTest.cpp
// The year (SeasonRules) and the season's snowline (MountainRules) on fixed numbers; run from TestGameRules.

#include "hypch.h"

#include "Rules/MountainRules.h"
#include "Rules/SeasonRules.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace Hydro
{
	static int s_SeasonFailures = 0;

	static void SeasonCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_SeasonFailures++;
			std::printf("FAIL  SeasonRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define SEASON_CHECK(condition) SeasonCheck((condition), #condition, __LINE__, checks)

	static SeasonDefinition MakeSeason(const char* name, int order, float days, float flow, float snow)
	{
		SeasonDefinition season;
		season.Name = name;
		season.Order = order;
		season.Days = days;
		season.Flow = flow;
		season.GroundSnow = snow;
		return season;
	}

	static bool Near(float a, float b)
	{
		return std::abs(a - b) < 1e-4f;
	}

	int TestSeasonRules(int& checks)
	{
		std::printf("Game rules: SeasonRules\n");

		// Sorted by Order; without seasons, neutral.
		const std::vector<SeasonDefinition> seasons = SeasonRules::Sorted({ MakeSeason("Winter", 3, 4.0f, 0.8f, 1.0f),
			MakeSeason("Spring", 0, 4.0f, 1.2f, 0.0f), MakeSeason("Autumn", 2, 4.0f, 1.0f, 0.0f),
			MakeSeason("Summer", 1, 4.0f, 0.8f, 0.0f) });
		SEASON_CHECK(seasons[0].Name == "Spring" && seasons[3].Name == "Winter");
		const SeasonState none = SeasonRules::At({}, 3.0, 1.0f);
		SEASON_CHECK(none.Name.empty() && none.Flow == 1.0f && none.GroundSnow == 0.0f);

		// A 16-day year: each season's own values until its last day, then a smooth blend into the next.
		SeasonState state = SeasonRules::At(seasons, 0.5, 1.0f);
		SEASON_CHECK(state.Name == "Spring" && state.Index == 0 && state.Next == 1 && state.Day == 1 && state.Year == 1);
		SEASON_CHECK(state.Blend == 0.0f && state.Flow == 1.2f);
		state = SeasonRules::At(seasons, 3.5, 1.0f); // halfway through spring's last day
		SEASON_CHECK(state.Name == "Spring" && state.Day == 4 && Near(state.Blend, 0.5f) && Near(state.Flow, 1.0f));
		state = SeasonRules::At(seasons, 13.0, 1.0f);
		SEASON_CHECK(state.Name == "Winter" && state.Day == 2 && state.GroundSnow == 1.0f && state.Next == 0);
		state = SeasonRules::At(seasons, 15.75, 1.0f); // winter into spring
		SEASON_CHECK(state.Index == 3 && state.Next == 0 && state.GroundSnow < 0.2f && state.GroundSnow > 0.0f);
		state = SeasonRules::At(seasons, 16.5, 1.0f);
		SEASON_CHECK(state.Name == "Spring" && state.Year == 2 && state.Day == 1 && state.GroundSnow == 0.0f);
		SEASON_CHECK(SeasonRules::Step(1.23f, 20.0f) == 1.25f && SeasonRules::Step(-2.6f, 4.0f) == -2.5f);

		// The snowline moves with the season, but snow never lies on land below rock (grass under it stays grass).
		MountainRules::Settings mountains; // rock from 4, snow from 9
		mountains.Ragged = 0.0f;
		SEASON_CHECK(MountainRules::LevelTile(7, 3, 3, mountains) == 6);
		mountains.SnowShift = -3.0f;
		SEASON_CHECK(MountainRules::LevelTile(7, 3, 3, mountains) == 4 && MountainRules::LevelTile(6, 3, 3, mountains) == 4);
		mountains.SnowShift = -8.0f;
		SEASON_CHECK(MountainRules::LevelTile(4, 3, 3, mountains) == 4 && MountainRules::LevelTile(3, 3, 3, mountains) == -1);
		mountains.SnowShift = 2.0f;
		SEASON_CHECK(MountainRules::LevelTile(10, 3, 3, mountains) == 6 && MountainRules::LevelTile(11, 3, 3, mountains) == 4);

		// The animals' season: the migrants' share and the sleepers', blended like the rest.
		std::vector<SeasonDefinition> year = seasons;
		year[3].Migrants = 0.0f;
		year[3].Burrowed = 1.0f;
		const SeasonState late = SeasonRules::At(year, 11.5, 1.0f); // autumn's last day, half into winter
		SEASON_CHECK(Near(late.Migrants, 0.5f) && Near(late.Burrowed, 0.5f));
		SEASON_CHECK(SeasonRules::At(year, 0.5, 1.0f).Migrants == 1.0f && SeasonRules::At(year, 0.5, 1.0f).Burrowed == 0.0f);
		return s_SeasonFailures;
	}
}
