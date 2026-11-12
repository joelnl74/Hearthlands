// Tools/Tests/TalentRulesTest.cpp
// Talents and milestones (Game/src/Rules/TalentRules.h) on fixed numbers; run from GameRulesTest.cpp's TestGameRules.

#include "hypch.h"

#include "Rules/BrushRules.h"
#include "Rules/ObjectRules.h"
#include "Rules/TalentRules.h"

#include <cstdio>
#include <vector>

namespace Hydro
{
	static int s_TalentFailures = 0;

	static void TalentCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_TalentFailures++;
			std::printf("FAIL  TalentRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define TALENT_CHECK(condition) TalentCheck((condition), #condition, __LINE__, checks)

	static TalentDefinition MakeTalent(int id, int cost, int required, TalentEffect effect, float amount)
	{
		TalentDefinition talent;
		talent.TalentId = id;
		talent.Cost = cost;
		talent.RequiresTalentId = required;
		talent.Effect = effect;
		talent.Amount = amount;
		return talent;
	}

	static MilestoneDefinition MakeMilestone(int id, MilestoneKind kind, int target, int count, int points)
	{
		MilestoneDefinition milestone;
		milestone.MilestoneId = id;
		milestone.Kind = kind;
		milestone.TargetId = target;
		milestone.Count = count;
		milestone.Points = points;
		return milestone;
	}

	int TestTalentRules(int& checks)
	{
		std::printf("Game rules: TalentRules\n");

		// Count: tiles per id, objects per kind, the highest level, springs.
		TileMap map(10, 10, 1);
		for (int x = 0; x < 6; x++)
		{
			map.Set(x, 0, 2);
		}
		std::vector<ObjectDefinition> objects(2);
		objects[0].ObjectId = 7;
		objects[0].Kind = ObjectKind::Tree;
		objects[1].ObjectId = 8;
		objects[1].Kind = ObjectKind::Shrub;
		const std::vector<int> byId = ObjectRules::IndexById(objects);
		ObjectLayer layer;
		layer.Reset(100);
		layer.Set(11, 7);
		layer.Set(12, 7);
		layer.Set(13, 8);
		HeightMap heights(10, 10);
		heights.Set(4, 4, 5);
		TalentRules::Counts counts = TalentRules::Count(map, layer, objects, byId, heights, 2);
		TALENT_CHECK(counts.Tiles[2] == 6 && counts.Tiles[1] == 94 && counts.Objects[(int)ObjectKind::Tree] == 2);
		TALENT_CHECK(counts.Objects[(int)ObjectKind::Shrub] == 1 && counts.TopLevel == 5 && counts.Springs == 2);

		// Progress and Reach: each milestone gives its points once, when its count is reached.
		const std::vector<MilestoneDefinition> milestones = { MakeMilestone(1, MilestoneKind::Tiles, 2, 5, 1),
			MakeMilestone(2, MilestoneKind::Objects, (int)ObjectKind::Tree, 3, 1), MakeMilestone(3, MilestoneKind::Springs, -1, 1, 2),
			MakeMilestone(4, MilestoneKind::Level, -1, 4, 1) };
		TALENT_CHECK(TalentRules::Progress(milestones[0], counts) == 6 && TalentRules::Progress(milestones[1], counts) == 2);
		TALENT_CHECK(TalentRules::Progress(milestones[2], counts) == 2 && TalentRules::Progress(milestones[3], counts) == 5);
		TalentState state;
		std::vector<int> reached = TalentRules::Reach(state, milestones, counts);
		TALENT_CHECK(reached.size() == 3 && state.Points == 4 && state.Earned == 4 && TalentRules::Contains(state.Reached, 2) == false);
		TALENT_CHECK(TalentRules::Reach(state, milestones, counts).empty() && state.Points == 4); // once each
		layer.Set(14, 7);
		counts = TalentRules::Count(map, layer, objects, byId, heights, 2);
		reached = TalentRules::Reach(state, milestones, counts);
		TALENT_CHECK(reached.size() == 1 && reached[0] == 2 && state.Points == 5);

		// Check and Buy: cost, requirement, owned once; bonuses add up per effect; UnlockTile adds a brush, not a bonus.
		const std::vector<TalentDefinition> talents = { MakeTalent(1, 1, -1, TalentEffect::MoistureReach, 1.0f),
			MakeTalent(2, 2, 1, TalentEffect::MoistureReach, 2.0f), MakeTalent(3, 3, -1, TalentEffect::ClueClarity, 0.25f),
			MakeTalent(4, 1, -1, TalentEffect::UnlockTile, 0.0f), MakeTalent(5, 9, -1, TalentEffect::BrushSize, 4.0f) };
		TALENT_CHECK(TalentRules::Check(talents, state, 2) == TalentRules::Verdict::NeedsTalent);
		TALENT_CHECK(TalentRules::Check(talents, state, 5) == TalentRules::Verdict::TooDear);
		TALENT_CHECK(TalentRules::Check(talents, state, 99) == TalentRules::Verdict::Unknown);
		TALENT_CHECK(TalentRules::Buy(talents, state, 1) == TalentRules::Verdict::Buyable && state.Points == 4);
		TALENT_CHECK(TalentRules::Buy(talents, state, 1) == TalentRules::Verdict::Owned && state.Points == 4);
		TALENT_CHECK(TalentRules::Buy(talents, state, 2) == TalentRules::Verdict::Buyable && state.Points == 2);
		TALENT_CHECK(state.Get(TalentEffect::MoistureReach) == 3.0f && state.Get(TalentEffect::ClueClarity) == 0.0f);
		TALENT_CHECK(TalentRules::Buy(talents, state, 3) == TalentRules::Verdict::TooDear && state.Points == 2);
		TALENT_CHECK(TalentRules::Buy(talents, state, 4) == TalentRules::Verdict::Buyable && state.Points == 1);
		const std::vector<uint8_t> tiles = TalentRules::UnlockedTiles(talents, state);
		TALENT_CHECK(tiles.size() == 1 && tiles[0] == 0 && state.Get(TalentEffect::UnlockTile) == 0.0f);
		TALENT_CHECK(state.Owned.size() == 3 && state.Owned[0] == 1 && state.Owned[2] == 4);

		// The largest brush: Radii up to `largest` (8 without talents, 12 and 16 with them).
		TALENT_CHECK(BrushRules::StepRadius(8.0f, 1) == 8.0f && BrushRules::StepRadius(8.0f, 1, 12.0f) == 12.0f);
		TALENT_CHECK(BrushRules::StepRadius(12.0f, 1, 16.0f) == 16.0f && BrushRules::StepRadius(16.0f, 0, 8.0f) == 8.0f);
		return s_TalentFailures;
	}
}
