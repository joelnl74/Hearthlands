#pragma once

#include "Data/PeopleDefinition.h"
#include "Rules/SettlementRules.h"
#include "State/Random.h"
#include "State/SettlementState.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Hydro
{
	// Peoples who can't stand each other (Dislikes) build tension living near, faster the closer: quarrels, raids, fights,
	// at the top deaths. Violence vents the attackers' tension and angers the attacked; guards drive raids off. Pure.
	class FeudRules
	{
	public:
		enum class IncidentKind : uint8_t
		{
			Quarrel,  // tension rose past QuarrelFrom
			Stolen,   // From's raiders took Amount food from To
			Burned,   // From's raiders burned a shelter of To
			Repelled, // To's guards drove From's raiders off
			Fight,    // villagers of both were hurt (Amount: how many)
			Death,    // Who, of To, was killed by From's people
			Calmed    // From's tension fell back below QuarrelFrom: To is far enough (or gone)
		};

		struct Incident
		{
			IncidentKind Kind = IncidentKind::Quarrel;
			int From = 0; // settlement ids
			int To = 0;
			std::string Who;
			float Amount = 0.0f;
		};

		static constexpr float QuarrelFrom = 0.25f;
		static constexpr float RaidFrom = 0.5f;
		static constexpr float FightFrom = 0.75f;
		static constexpr float DeathFrom = 0.95f;
		static constexpr float FadePerDay = 0.05f;
		static constexpr float DeathsPerDay = 0.12f; // at the very top of the tension
		// Violence vents the attackers' tension (a raid, a fight, a death), and angers the attacked (revenge).
		static constexpr float RaidVent = 0.04f;
		static constexpr float FightVent = 0.06f;
		static constexpr float DeathVent = 0.2f;
		static constexpr float Revenge = 0.05f;

		// Whether the people at `row` dislike the people at `other`.
		static bool Dislikes(const std::vector<PeopleDefinition>& peoples, int row, int other)
		{
			if (row < 0 || other < 0 || row >= (int)peoples.size() || other >= (int)peoples.size())
			{
				return false;
			}
			const std::vector<int>& dislikes = peoples[(size_t)row].Dislikes;
			return std::find(dislikes.begin(), dislikes.end(), peoples[(size_t)other].PeopleId) != dislikes.end();
		}

		// The nearest settlement the one at `index` dislikes within its people's DislikeRadius (its index; -1 none) and how
		// far it is.
		static int Nearest(const SettlementState& state, const std::vector<PeopleDefinition>& peoples, size_t index, float& distance)
		{
			const Settlement& settlement = state.List[index];
			if (settlement.People < 0 || settlement.People >= (int)peoples.size())
			{
				return -1;
			}
			const int radius = peoples[(size_t)settlement.People].DislikeRadius;
			int found = -1;
			distance = (float)radius;
			for (size_t i = 0; i < state.List.size(); i++)
			{
				const Settlement& other = state.List[i];
				if (i == index || Dislikes(peoples, settlement.People, other.People) == false)
				{
					continue;
				}
				const float dx = (float)(other.X - settlement.X);
				const float dy = (float)(other.Y - settlement.Y);
				const float apart = std::sqrt(dx * dx + dy * dy);
				if (apart <= distance)
				{
					distance = apart;
					found = (int)i;
				}
			}
			return found;
		}

		// Tension a day `distance` tiles from a disliked settlement: TensionPerDay right beside it, falling with the square
		// of the distance to a seventh at the edge of DislikeRadius; `calm` (FeudCalm talents) takes its share off.
		static float Rate(const PeopleDefinition& people, float distance, float calm)
		{
			const float radius = (float)std::max(1, people.DislikeRadius);
			const float close = std::clamp(1.0f - distance / radius, 0.0f, 1.0f);
			return people.TensionPerDay * (0.15f + 0.85f * close * close) * std::clamp(1.0f - calm, 0.0f, 1.0f);
		}

		// `days` of feuding between every settlement and its nearest disliked neighbour: tension, then incidents.
		static std::vector<Incident> Step(SettlementState& state, const SettlementRules::World& world,
			const std::vector<PeopleDefinition>& peoples, float calm, float days, Random& random)
		{
			std::vector<Incident> incidents;
			for (size_t i = 0; i < state.List.size(); i++)
			{
				float distance = 0.0f;
				const int foe = Nearest(state, peoples, i, distance);
				Settlement& settlement = state.List[i];
				const float before = settlement.Tension;
				if (foe < 0)
				{
					settlement.Tension = std::max(0.0f, settlement.Tension - FadePerDay * days);
					if (before >= QuarrelFrom && settlement.Tension < QuarrelFrom)
					{
						incidents.push_back({ IncidentKind::Calmed, settlement.Id, settlement.FeudWith, std::string(), 0.0f });
					}
					settlement.FeudWith = settlement.Tension > 0.0f ? settlement.FeudWith : 0;
					continue;
				}
				Settlement& other = state.List[(size_t)foe];
				settlement.FeudWith = other.Id;
				settlement.Tension = std::min(1.0f, settlement.Tension + Rate(peoples[(size_t)settlement.People], distance, calm) * days);
				if (before < QuarrelFrom && settlement.Tension >= QuarrelFrom)
				{
					incidents.push_back({ IncidentKind::Quarrel, settlement.Id, other.Id, std::string(), 0.0f });
				}
				Act(settlement, other, world, peoples, days, random, incidents);
			}
			return incidents;
		}

		// What the settlement's tension drives it to against `other`, by chance in `days`.
		static void Act(Settlement& settlement, Settlement& other, const SettlementRules::World& world,
			const std::vector<PeopleDefinition>& peoples, float days, Random& random, std::vector<Incident>& incidents)
		{
			const float tension = settlement.Tension;
			if (tension >= RaidFrom && random.Float() < 0.6f * tension * days)
			{
				Raid(settlement, other, world, peoples, random, incidents);
				Vent(settlement, other, RaidVent);
			}
			if (tension >= FightFrom && random.Float() < 0.5f * tension * days)
			{
				const int hurt = Hurt(settlement, 1, random) + Hurt(other, 1 + (int)random.Below(2), random);
				if (hurt > 0)
				{
					incidents.push_back({ IncidentKind::Fight, settlement.Id, other.Id, std::string(), (float)hurt });
				}
				Vent(settlement, other, FightVent);
			}
			if (tension >= DeathFrom && random.Float() < DeathsPerDay * days && other.Villagers.empty() == false)
			{
				// One of theirs is killed: a guard or the hurt first.
				size_t victim = (size_t)random.Below((uint32_t)other.Villagers.size());
				for (size_t v = 0; v < other.Villagers.size(); v++)
				{
					const VillagerJob job = other.Villagers[v].Job;
					victim = job == VillagerJob::Guard || job == VillagerJob::Hurt ? v : victim;
				}
				const std::string who = other.Villagers[victim].Name;
				SettlementRules::Lose(other, victim);
				incidents.push_back({ IncidentKind::Death, settlement.Id, other.Id, who, 1.0f });
				Vent(settlement, other, DeathVent);
			}
		}

		// After violence: the attackers' tension eases by `vent`, the attacked's rises by Revenge (if they feud back).
		static void Vent(Settlement& settlement, Settlement& other, float vent)
		{
			settlement.Tension = std::max(0.0f, settlement.Tension - vent);
			if (other.Tension > 0.0f)
			{
				other.Tension = std::min(1.0f, other.Tension + Revenge);
			}
		}

		// A raid on `other`: the raiders (a third of those who can fight) against its guards; won, they take food or burn a
		// shelter near its centre.
		static void Raid(Settlement& settlement, Settlement& other, const SettlementRules::World& world,
			const std::vector<PeopleDefinition>& peoples, Random& random, std::vector<Incident>& incidents)
		{
			const float raiders = std::max(1.0f, (float)Able(settlement) / 3.0f);
			const float guards = (float)SettlementRules::Workers(other, VillagerJob::Guard);
			if (random.Float() >= raiders / (raiders + 1.5f * guards))
			{
				incidents.push_back({ IncidentKind::Repelled, settlement.Id, other.Id, std::string(), 0.0f });
				return;
			}
			const PeopleDefinition& people = peoples[(size_t)other.People];
			if (random.Float() < 0.35f && other.Shelters > 0 && Burn(other, people, world))
			{
				other.Shelters--;
				incidents.push_back({ IncidentKind::Burned, settlement.Id, other.Id, std::string(), 1.0f });
				return;
			}
			const float stolen = std::min(other.Food * 0.5f, raiders * 4.0f);
			if (stolen > 0.0f)
			{
				other.Food -= stolen;
				settlement.Food += stolen;
				incidents.push_back({ IncidentKind::Stolen, settlement.Id, other.Id, std::string(), stolen });
			}
		}

		static int Able(const Settlement& settlement)
		{
			int able = 0;
			for (const Villager& villager : settlement.Villagers)
			{
				able += villager.HurtDays > 0.0f ? 0 : 1;
			}
			return able;
		}

		// Hurts up to `count` of the settlement's able villagers (guards first) for 3 to 7 days. Returns how many.
		static int Hurt(Settlement& settlement, int count, Random& random)
		{
			int hurt = 0;
			for (int pass = 0; pass < 2 && hurt < count; pass++)
			{
				for (Villager& villager : settlement.Villagers)
				{
					const bool first = pass == 1 || villager.Job == VillagerJob::Guard;
					if (hurt < count && villager.HurtDays <= 0.0f && first)
					{
						villager.HurtDays = 3.0f + random.Float() * 4.0f;
						villager.Job = VillagerJob::Hurt;
						hurt++;
					}
				}
			}
			return hurt;
		}

		// Burns one of the settlement's shelters near its centre (the first found, ring by ring). False: none.
		static bool Burn(Settlement& settlement, const PeopleDefinition& people, const SettlementRules::World& world)
		{
			const int reach = SettlementRules::HomeReach(people, settlement.Level);
			for (int ring = 1; ring <= reach; ring++)
			{
				for (int dy = -ring; dy <= ring; dy++)
				{
					for (int dx = -ring; dx <= ring; dx++)
					{
						const int x = settlement.X + dx;
						const int y = settlement.Y + dy;
						if ((std::abs(dx) != ring && std::abs(dy) != ring) || world.Map->InBounds(x, y) == false)
						{
							continue;
						}
						const size_t index = SettlementRules::IndexOf(world, x, y);
						if (SettlementRules::ShelterHolds(people, world.Objects->Get(index)) > 0)
						{
							world.Objects->Set(index, 0);
							return true;
						}
					}
				}
			}
			return false;
		}
	};
}
