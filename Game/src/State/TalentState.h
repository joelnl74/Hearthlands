#pragma once

#include "Data/TalentDefinition.h"

#include <cstdint>
#include <vector>

namespace Hydro
{
	// The run's talents: points from milestones, what they bought, and what the owned talents add up to (TalentRules).
	struct TalentState
	{
		static constexpr int EffectCount = (int)TalentEffect::Evolve + 1;

		int Points = 0;           // unspent
		int Earned = 0;           // given by milestones so far
		std::vector<int> Owned;   // TalentId, in the order bought
		std::vector<int> Reached; // MilestoneId, in the order reached
		int SpringsFound = 0;     // springs dug this run (a milestone counts them)
		float Bonus[EffectCount] = {}; // per TalentEffect: the owned talents' amounts added up (TalentRules::Recount)
		float Settling[256] = {};      // per PeopleId: the best Settlers amount owned (0: they don't settle yet)
		bool Evolving[256] = {};       // per PeopleId: an Evolve talent lets that branch arise

		float Get(TalentEffect effect) const
		{
			return (int)effect < EffectCount ? Bonus[(int)effect] : 0.0f;
		}

		// Whether the branch with this PeopleId may arise from the people it branches from.
		bool MayEvolve(int peopleId) const
		{
			return peopleId > 0 && peopleId < 256 && Evolving[peopleId];
		}

		// Chance a day a new group of the people with this PeopleId comes (0: no talent lets them settle).
		float SettleChance(int peopleId) const
		{
			return peopleId > 0 && peopleId < 256 ? Settling[peopleId] : 0.0f;
		}
	};
}
