#pragma once

#include "Data/AnimalDefinition.h"
#include "Data/MilestoneDefinition.h"
#include "Data/ObjectDefinition.h"
#include "Data/TalentDefinition.h"
#include "State/HeightMap.h"
#include "State/ObjectLayer.h"
#include "State/TalentState.h"
#include "State/TileMap.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// Milestones and talents: what the world has to count (Count), which milestones that reaches (Reach: points, once
	// each), whether a talent can be bought and buying it (Buy), and what the owned talents add up to (Recount). Pure.
	class TalentRules
	{
	public:
		// What milestones look at, counted from the world now and then (Count).
		struct Counts
		{
			std::vector<uint32_t> Tiles = std::vector<uint32_t>(256, 0); // per tile id
			uint32_t Objects[4] = {};                                     // per ObjectKind
			int Springs = 0;
			int TopLevel = 0;
			std::vector<uint32_t> Animals = std::vector<uint32_t>(256, 0); // per AnimalId (AddAnimals)
			uint32_t AllAnimals = 0;
			int Species = 0;
			int SpeciesByKind[5] = {};                                    // per AnimalKind (AddAnimals with the table)
			std::vector<uint32_t> Habitats = std::vector<uint32_t>(16, 0); // water tiles per WaterHabitat (AddHabitats)
			std::vector<uint32_t> Soil = std::vector<uint32_t>(256, 0); // land tiles per soil quality byte (AddSoil)
			uint32_t People = 0;                                          // settlers (SettlementRules::People)
			std::vector<uint32_t> PeopleById = std::vector<uint32_t>(256, 0); // settlers per PeopleId
			std::vector<uint32_t> LevelById = std::vector<uint32_t>(256, 0); // per PeopleId: their best settlement's level (0 first)
			uint32_t BestLevel = 0;                                       // any people's
			uint32_t Ancient = 0;                                         // ancient trees (Ancient rows)
		};

		enum class Verdict
		{
			Buyable,
			Owned,
			NeedsTalent, // RequiresTalentId isn't owned yet
			NeedsMilestone, // RequiresMilestoneId isn't reached yet
			TooDear,     // fewer points than Cost
			Unknown      // no such talent
		};

		static Counts Count(const TileMap& map, const ObjectLayer& objects, const std::vector<ObjectDefinition>& definitions,
			const std::vector<int>& byId, const HeightMap& heights, int springs)
		{
			Counts counts;
			for (int id = 0; id < 256; id++)
			{
				counts.Tiles[(size_t)id] = map.GetCount((uint8_t)id);
			}
			for (uint8_t objectId : objects.GetIds())
			{
				const int row = objectId < byId.size() ? byId[objectId] : -1;
				counts.Ancient += objectId != 0 && row >= 0 && definitions[(size_t)row].Ancient ? 1u : 0u;
				if (objectId != 0 && row >= 0 && (int)definitions[(size_t)row].Kind < 4)
				{
					counts.Objects[(int)definitions[(size_t)row].Kind]++;
				}
			}
			for (uint8_t level : heights.GetLevels())
			{
				counts.TopLevel = level > counts.TopLevel ? level : counts.TopLevel;
			}
			counts.Springs = springs;
			return counts;
		}

		// The wild animals per AnimalId (WildlifeRules::Totals) into the counts: each, all, and the species living.
		static void AddAnimals(Counts& counts, const std::vector<uint32_t>& totals)
		{
			for (size_t id = 0; id < totals.size() && id < counts.Animals.size(); id++)
			{
				counts.Animals[id] = totals[id];
				counts.AllAnimals += totals[id];
				counts.Species += totals[id] > 0 ? 1 : 0;
			}
		}

		// The same, with the Animals table: also the species living per AnimalKind.
		static void AddAnimals(Counts& counts, const std::vector<uint32_t>& totals, const std::vector<AnimalDefinition>& animals)
		{
			AddAnimals(counts, totals);
			for (const AnimalDefinition& animal : animals)
			{
				const bool living = animal.AnimalId >= 0 && (size_t)animal.AnimalId < totals.size() && totals[(size_t)animal.AnimalId] > 0;
				counts.SpeciesByKind[(int)animal.Kind < 5 ? (int)animal.Kind : 0] += living ? 1 : 0;
			}
		}

		// The water tiles per WaterHabitat (HabitatRules::Count) into the counts.
		static void AddHabitats(Counts& counts, const std::vector<uint32_t>& habitats)
		{
			for (size_t h = 0; h < habitats.size() && h < counts.Habitats.size(); h++)
			{
				counts.Habitats[h] = habitats[h];
			}
		}

		// The land's soil qualities (SoilRules::Qualities, a byte per tile) into the counts; land: 1 inside the coast (empty all).
		static void AddSoil(Counts& counts, const std::vector<uint8_t>& qualities, const std::vector<uint8_t>& land)
		{
			for (size_t i = 0; i < qualities.size(); i++)
			{
				counts.Soil[qualities[i]] += land.empty() || (i < land.size() && land[i] != 0) ? 1u : 0u;
			}
		}

		// How far a milestone's count is (compare with its Count).
		static uint32_t Progress(const MilestoneDefinition& milestone, const Counts& counts)
		{
			switch (milestone.Kind)
			{
			case MilestoneKind::Tiles:
				return milestone.TargetId >= 0 && milestone.TargetId < 256 ? counts.Tiles[(size_t)milestone.TargetId] : 0u;
			case MilestoneKind::Objects:
				return milestone.TargetId >= 0 && milestone.TargetId < 4 ? counts.Objects[milestone.TargetId] : 0u;
			case MilestoneKind::Springs:
				return (uint32_t)(counts.Springs > 0 ? counts.Springs : 0);
			case MilestoneKind::Level:
				return (uint32_t)counts.TopLevel;
			case MilestoneKind::Animals:
				return milestone.TargetId >= 0 && milestone.TargetId < 256 ? counts.Animals[(size_t)milestone.TargetId] : counts.AllAnimals;
			case MilestoneKind::Species:
				if (milestone.TargetId >= 0 && milestone.TargetId < 5)
				{
					return (uint32_t)counts.SpeciesByKind[milestone.TargetId];
				}
				return (uint32_t)(counts.Species > 0 ? counts.Species : 0);
			case MilestoneKind::Habitat:
			{
				const size_t habitat = (size_t)(milestone.TargetId >= 0 ? milestone.TargetId : 0);
				return habitat < counts.Habitats.size() ? counts.Habitats[habitat] : 0u;
			}
			case MilestoneKind::Settlers:
				return milestone.TargetId > 0 && milestone.TargetId < 256 ? counts.PeopleById[(size_t)milestone.TargetId] : counts.People;
			case MilestoneKind::Ancient:
				return counts.Ancient;
			case MilestoneKind::SettlementLevel:
				return milestone.TargetId > 0 && milestone.TargetId < 256 ? counts.LevelById[(size_t)milestone.TargetId] : counts.BestLevel;
			case MilestoneKind::RichSoil:
			{
				const int from = (milestone.TargetId < 0 ? 0 : (milestone.TargetId > 100 ? 100 : milestone.TargetId)) * 255 / 100;
				uint32_t rich = 0;
				for (int value = from; value < 256; value++)
				{
					rich += counts.Soil[(size_t)value];
				}
				return rich;
			}
			}
			return 0u;
		}

		static bool Contains(const std::vector<int>& ids, int id)
		{
			for (int value : ids)
			{
				if (value == id)
				{
					return true;
				}
			}
			return false;
		}

		// Marks the milestones reached for the first time and gives their points. Returns their ids, in table order.
		static std::vector<int> Reach(TalentState& state, const std::vector<MilestoneDefinition>& milestones, const Counts& counts)
		{
			std::vector<int> reached;
			for (const MilestoneDefinition& milestone : milestones)
			{
				if (Contains(state.Reached, milestone.MilestoneId) || Progress(milestone, counts) < (uint32_t)milestone.Count)
				{
					continue;
				}
				state.Reached.push_back(milestone.MilestoneId);
				state.Points += milestone.Points;
				state.Earned += milestone.Points;
				reached.push_back(milestone.MilestoneId);
			}
			return reached;
		}

		static const TalentDefinition* Find(const std::vector<TalentDefinition>& talents, int talentId)
		{
			for (const TalentDefinition& talent : talents)
			{
				if (talent.TalentId == talentId)
				{
					return &talent;
				}
			}
			return nullptr;
		}

		static Verdict Check(const std::vector<TalentDefinition>& talents, const TalentState& state, int talentId)
		{
			const TalentDefinition* talent = Find(talents, talentId);
			if (talent == nullptr)
			{
				return Verdict::Unknown;
			}
			if (Contains(state.Owned, talentId))
			{
				return Verdict::Owned;
			}
			if (talent->RequiresTalentId >= 0 && Contains(state.Owned, talent->RequiresTalentId) == false)
			{
				return Verdict::NeedsTalent;
			}
			if (talent->RequiresMilestoneId >= 0 && Contains(state.Reached, talent->RequiresMilestoneId) == false)
			{
				return Verdict::NeedsMilestone;
			}
			return state.Points >= talent->Cost ? Verdict::Buyable : Verdict::TooDear;
		}

		// Buys the talent when it is buyable: its cost off the points, owned, the bonuses added up again.
		static Verdict Buy(const std::vector<TalentDefinition>& talents, TalentState& state, int talentId)
		{
			const Verdict verdict = Check(talents, state, talentId);
			if (verdict == Verdict::Buyable)
			{
				state.Points -= Find(talents, talentId)->Cost;
				state.Owned.push_back(talentId);
				Recount(talents, state);
			}
			return verdict;
		}

		// Bonus per effect: the owned talents' amounts added up (UnlockTile adds nothing: the brush is the effect); Settlers
		// per people: the best owned.
		static void Recount(const std::vector<TalentDefinition>& talents, TalentState& state)
		{
			for (float& bonus : state.Bonus)
			{
				bonus = 0.0f;
			}
			for (float& chance : state.Settling)
			{
				chance = 0.0f;
			}
			for (bool& evolving : state.Evolving)
			{
				evolving = false;
			}
			for (int talentId : state.Owned)
			{
				const TalentDefinition* talent = Find(talents, talentId);
				const int effect = talent != nullptr ? (int)talent->Effect : 0;
				if (talent != nullptr && talent->Effect == TalentEffect::Evolve)
				{
					state.Evolving[talent->TargetId > 0 && talent->TargetId < 256 ? talent->TargetId : 0] = true;
				}
				else if (talent != nullptr && talent->Effect == TalentEffect::Settlers)
				{
					// A later talent for the same people replaces the earlier: the best counts.
					const int people = talent->TargetId > 0 && talent->TargetId < 256 ? talent->TargetId : 0;
					state.Settling[people] = std::max(state.Settling[people], talent->Amount);
				}
				else if (talent != nullptr && talent->Effect != TalentEffect::UnlockTile && effect < TalentState::EffectCount)
				{
					state.Bonus[effect] += talent->Amount;
				}
			}
		}

		// The tile ids the owned talents unlock, in the order bought.
		static std::vector<uint8_t> UnlockedTiles(const std::vector<TalentDefinition>& talents, const TalentState& state)
		{
			std::vector<uint8_t> tiles;
			for (int talentId : state.Owned)
			{
				const TalentDefinition* talent = Find(talents, talentId);
				if (talent != nullptr && talent->Effect == TalentEffect::UnlockTile && talent->Amount >= 0.0f && talent->Amount < 256.0f)
				{
					tiles.push_back((uint8_t)talent->Amount);
				}
			}
			return tiles;
		}
	};
}
