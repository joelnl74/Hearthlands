// Tools/Tests/GameRulesTest.cpp
// The game's rules (Game/src/Rules) on fixed numbers: no scene, no data files, every random draw given. Built into
// Hydro's DataRoundTripTest by scripts/test_data.sh (bash scripts/check.sh --tests). No rules yet: add a section per
// rules file as they arrive.

#include "hypch.h"

#include <cstdio>

namespace Hydro
{
	static int s_RuleFailures = 0;

	static void RuleCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_RuleFailures++;
			std::printf("FAIL  GameRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define RULE_CHECK(condition) RuleCheck((condition), #condition, __LINE__, checks)

	int TestGameRules(int& checks)
	{
		std::printf("Game rules: none yet\n");
		RULE_CHECK(true);

		return s_RuleFailures;
	}
}
