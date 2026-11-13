#pragma once

#include "Data/AnimalDefinition.h"
#include "Data/BuildingDefinition.h"
#include "Data/ObjectDefinition.h"
#include "Data/PeopleDefinition.h"
#include "Rules/BuildingRules.h"
#include "Rules/ObjectRules.h"
#include "Rules/SettlementGround.h"
#include "Rules/SoilRules.h"
#include "Rules/WildlifeRules.h"
#include "State/HeightMap.h"
#include "State/Hydrology.h"
#include "State/ObjectLayer.h"
#include "State/Random.h"
#include "State/SettlementState.h"
#include "State/TileMap.h"
#include "State/WildlifeState.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// A settlement's life: villagers take jobs from its needs and the player's focus, eat, grow, build, reach the next
	// level (camp, hamlet, village) and leave when food runs out. The ground they work is SettlementGround; settling
	// SettlingRules; feuds FeudRules. Pure.
	class SettlementRules : public SettlementGround
	{
	public:
		// What the land around a settlement's centre offers.
		struct Survey
		{
			int FreshWater = 0;       // water tiles inside the coast (lakes, rivers; not the sea)
			float PlantFood = 0.0f;   // food the plants give a day
			float Game = 0.0f;        // animals they hunt in the centre's region
			float Wood = 0.0f;        // wood the trees would give
			int Trees = 0;            // trees that would give it
			int Meadow = 0;           // land tiles without shrubs or trees
			int Shelters = 0;         // their shelters (of any level) near the centre
			int ShelterRoom = 0;      // people those shelters hold
			int Needed = 0;           // objects of their NeedObjectIds within reach (old trees)
			int NeededTiles = 0;      // tiles of their NeedTileIds within reach (mountain rock)
			bool Sowing = true;       // the season lets farmers sow (Update sets it from the world)
			std::vector<int> Buildings; // their buildings (BuildingIds) within reach
			float StoreDays = 0.0f;   // days of food more the store holds (their buildings; Update sets it)
			float BuildWood = -1.0f;  // wood the planned building takes; below 0 none planned (Update sets it)
		};

		// What happened in an update, for notices.
		struct Report
		{
			bool LeveledUp = false;
			bool Abandoned = false; // everyone left: the settlement goes (its centre with it)
			bool Gone = false;      // the centre is gone: the settlement with it
			int Built = 0;
			int Felled = 0;
			int Planted = 0;
			int Upgraded = 0; // older shelters rebuilt as the level's
			int BuiltBuilding = 0; // a building (BuildingId) put up
		};

		// One thing a settlement needs to reach its next level: what it has and what it needs.
		struct Need
		{
			std::string Name;
			float Have = 0.0f;
			float Want = 0.0f;
		};

		// The name at `index` of a list of names (round), or `fallback`.
		static std::string Pick(const std::vector<std::string>& names, int index, const std::string& fallback)
		{
			return names.empty() ? fallback : names[(size_t)(index < 0 ? 0 : index) % names.size()];
		}

		// The next name from its people's names.
		static void Rename(Settlement& settlement, const std::vector<PeopleDefinition>& peoples)
		{
			const PeopleDefinition* people = settlement.People >= 0 && settlement.People < (int)peoples.size()
				? &peoples[(size_t)settlement.People] : nullptr;
			settlement.NameIndex++;
			const std::string fallback = "Camp " + std::to_string(settlement.Id);
			settlement.Name = people != nullptr ? Pick(people->SettlementNames, settlement.NameIndex, fallback) : fallback;
		}

		// A new villager, named from its people's VillagerNames in turn (each settlement starts at another name; a second
		// Aldric there is Aldric II).
		static Villager NewVillager(Settlement& settlement, const PeopleDefinition& people)
		{
			Villager villager;
			villager.Id = settlement.NextVillager++;
			const int index = settlement.VillagerNames++;
			const int count = (int)people.VillagerNames.size();
			const int round = count > 0 ? index / count : 0;
			const std::string numeral = round == 0 ? std::string()
				: (round == 1 ? " II" : (round == 2 ? " III" : " " + std::to_string(round + 1)));
			const std::string fallback = "Villager " + std::to_string(villager.Id);
			villager.Name = Pick(people.VillagerNames, index + settlement.Id * 7, fallback) + numeral;
			return villager;
		}

		// Villagers to match the people (rounded): new ones born or come, the latest leave first.
		static void SyncVillagers(Settlement& settlement, const PeopleDefinition& people)
		{
			const size_t want = (size_t)std::max(0, (int)(settlement.Count + 0.5f));
			while (settlement.Villagers.size() < want)
			{
				settlement.Villagers.push_back(NewVillager(settlement, people));
			}
			while (settlement.Villagers.size() > want)
			{
				settlement.Villagers.pop_back();
			}
		}

		// A people's group settles at (x, y): their centre goes up, `count` people come (or `movers`, a group from another
		// settlement), named from their names. Returns its id, or 0 where the centre can't go.
		static int Found(SettlementState& state, const World& world, const std::vector<PeopleDefinition>& peoples, int row, int x, int y,
			int count, const std::vector<Villager>& movers = {})
		{
			if (row < 0 || row >= (int)peoples.size())
			{
				return 0;
			}
			const PeopleDefinition& people = peoples[(size_t)row];
			const int centre = CentreObject(world, people);
			if (centre <= 0 || centre > 255 || Buildable(world, x, y) == false || HighEnough(world, people, x, y) == false)
			{
				return 0;
			}
			world.Objects->Set(IndexOf(world, x, y), (uint8_t)centre);
			Settlement settlement;
			settlement.Id = state.NextId++;
			settlement.X = x;
			settlement.Y = y;
			settlement.People = row;
			settlement.NameIndex = Founded(state, row);
			settlement.Name = Pick(people.SettlementNames, settlement.NameIndex, "Camp " + std::to_string(settlement.Id));
			for (const Villager& mover : movers)
			{
				Villager villager = mover;
				villager.Id = settlement.NextVillager++;
				villager.Job = VillagerJob::Resting;
				villager.Days = 0.0f;
				settlement.Villagers.push_back(villager);
			}
			settlement.Count = (float)(movers.empty() ? std::max(1, count) : (int)movers.size());
			SyncVillagers(settlement, people);
			settlement.Food = settlement.Count * people.FoodPerPerson * 2.0f; // what they bring
			settlement.Room = std::max((int)settlement.Count, LevelOf(people, 0).PeoplePerShelter);
			state.List.push_back(settlement);
			state.Revision++;
			return settlement.Id;
		}

		// How many settlements the people at `row` founded so far this run (for their names); counts this one.
		static int Founded(SettlementState& state, int row)
		{
			if ((int)state.Founded.size() <= row)
			{
				state.Founded.resize((size_t)row + 1, 0);
			}
			return state.Founded[(size_t)row]++;
		}

		// What the land within `radius` of a centre offers (below 1: the people's Radius, as when they look for a place to
		// settle); shelters count within `home` (below 1: ShelterReach).
		static Survey Look(const World& world, const PeopleDefinition& people, int centreX, int centreY, int radius = 0, int home = 0)
		{
			Survey survey;
			const TileMap& map = *world.Map;
			radius = radius > 0 ? radius : people.Radius;
			home = home > 0 ? home : ShelterReach;
			for (int dy = -radius; dy <= radius; dy++)
			{
				for (int dx = -radius; dx <= radius; dx++)
				{
					const int x = centreX + dx;
					const int y = centreY + dy;
					if (dx * dx + dy * dy > radius * radius || map.InBounds(x, y) == false)
					{
						continue;
					}
					const size_t index = IndexOf(world, x, y);
					const uint8_t tile = map.Get(x, y);
					const bool inland = world.Land->empty() || (index < world.Land->size() && (*world.Land)[index] != 0);
					const bool neededTile = std::find(people.NeedTileIds.begin(), people.NeedTileIds.end(), (int)tile)
						!= people.NeedTileIds.end();
					survey.NeededTiles += neededTile ? 1 : 0;
					survey.FreshWater += tile == world.Water->WaterTileId && inland ? 1 : 0;
					const ObjectDefinition* object = Object(world, world.Objects->Get(index));
					const bool covered = object != nullptr && (object->Kind == ObjectKind::Shrub || object->Kind == ObjectKind::Tree);
					survey.Meadow += WildlifeRules::Walkable(tile, *world.Water) && covered == false ? 1 : 0;
					if (object == nullptr)
					{
						continue;
					}
					survey.PlantFood += object->Food;
					survey.Wood += object->Wood;
					survey.Trees += object->Wood > 0.0f ? 1 : 0;
					if (object->Kind == ObjectKind::Building && world.Buildings != nullptr)
					{
						const int row = BuildingRules::RowOf(*world.Buildings, object->ObjectId, people.PeopleId);
						const int id = row >= 0 ? (*world.Buildings)[(size_t)row].BuildingId : 0;
						if (id > 0 && std::find(survey.Buildings.begin(), survey.Buildings.end(), id) == survey.Buildings.end())
						{
							survey.Buildings.push_back(id);
						}
					}
					const int holds = ShelterHolds(people, object->ObjectId);
					const bool inReach = dx * dx + dy * dy <= home * home;
					survey.Shelters += holds > 0 && inReach ? 1 : 0;
					survey.ShelterRoom += inReach ? holds : 0;
					const bool needed = std::find(people.NeedObjectIds.begin(), people.NeedObjectIds.end(), object->ObjectId)
						!= people.NeedObjectIds.end();
					survey.Needed += needed ? 1 : 0;
				}
			}
			survey.Game = Game(world, people, centreX, centreY);
			return survey;
		}

		// People a shelter object of theirs holds (the level whose ShelterObjectId it is); 0 not one of theirs.
		static int ShelterHolds(const PeopleDefinition& people, int objectId)
		{
			for (const PeopleLevel& level : people.Levels)
			{
				if (level.ShelterObjectId > 0 && level.ShelterObjectId == objectId)
				{
					return level.PeoplePerShelter;
				}
			}
			return 0;
		}

		// The animals they hunt in the centre's region.
		static float Game(const World& world, const PeopleDefinition& people, int x, int y)
		{
			if (world.Wildlife == nullptr || world.Animals == nullptr)
			{
				return 0.0f;
			}
			const size_t region = world.Wildlife->RegionOf(x, y);
			float game = 0.0f;
			for (int animalId : people.HuntAnimalIds)
			{
				const int row = WildlifeRules::RowOf(*world.Animals, animalId);
				game += row >= 0 ? world.Wildlife->Get((size_t)row, region) : 0.0f;
			}
			return game;
		}

		// How well the land meets a people's needs, 0 to 1 each: water (6 fresh water tiles is plenty), food for their
		// largest group working it, wood for two first shelters.
		static void Scores(const PeopleDefinition& people, const Survey& survey, float& water, float& food, float& wood)
		{
			water = std::min(1.0f, (float)survey.FreshWater / 6.0f);
			const float group = std::max(1.0f, (float)people.GroupMax) * std::max(0.01f, people.FoodPerPerson);
			const float workers = std::max(1.0f, (float)people.GroupMax * 0.5f); // about half of them find food
			const float hunted = std::min(survey.Game * 0.25f, workers * people.HuntPerPerson) * people.MeatPerAnimal;
			food = std::min(1.0f, (std::min(survey.PlantFood, workers * people.GatherPerPerson) + hunted) / group);
			const float shelter = LevelOf(people, 0).WoodPerShelter;
			wood = shelter <= 0.0f ? 1.0f : std::min(1.0f, survey.Wood / (shelter * 2.0f));
		}

		// The share (0 to 1) of what they need nearby the land has (NeedObjects of NeedObjectIds, NeedTiles of NeedTileIds;
		// the smaller); 1 when they need nothing.
		static float NeededShare(const PeopleDefinition& people, const Survey& survey)
		{
			const float objects = people.NeedObjects > 0 ? std::min(1.0f, (float)survey.Needed / (float)people.NeedObjects) : 1.0f;
			const float tiles = people.NeedTiles > 0 ? std::min(1.0f, (float)survey.NeededTiles / (float)people.NeedTiles) : 1.0f;
			return std::min(objects, tiles);
		}

		static bool Meets(const PeopleDefinition& people, const Survey& survey)
		{
			float water = 0.0f;
			float food = 0.0f;
			float wood = 0.0f;
			Scores(people, survey, water, food, wood);
			return water >= people.NeedWater && food >= people.NeedFood && wood >= people.NeedWood
				&& NeededShare(people, survey) >= 1.0f;
		}

		// How much the people like a place that meets their needs: water, forest and meadow by their Likes.
		static float Liking(const PeopleDefinition& people, const Survey& survey)
		{
			const float area = (float)std::max(1, people.Radius * people.Radius * 3);
			return people.LikesWater * std::min(1.0f, (float)survey.FreshWater / 6.0f)
				+ people.LikesForest * std::min(1.0f, (float)survey.Trees * 4.0f / area)
				+ people.LikesMeadow * std::min(1.0f, (float)survey.Meadow / area);
		}

		// People the centre and the shelters hold, at most the level's MaxPeople.
		static int RoomFor(const PeopleDefinition& people, int level, int shelterRoom)
		{
			return std::min(LevelOf(people, level).MaxPeople, LevelOf(people, 0).PeoplePerShelter + std::max(0, shelterRoom));
		}

		static int Workers(const Settlement& settlement, VillagerJob job)
		{
			int count = 0;
			for (const Villager& villager : settlement.Villagers)
			{
				count += villager.Job == job ? 1 : 0;
			}
			return count;
		}

		// Whether they should build: no room for the next person (below the level's MaxPeople), or the next level wants
		// more shelters than they have.
		static bool WantShelter(const Settlement& settlement, const PeopleDefinition& people)
		{
			const PeopleLevel& level = LevelOf(people, settlement.Level);
			const bool crowded = settlement.Count + 1.0f > (float)settlement.Room && settlement.Room < level.MaxPeople;
			const bool levelWants = HasNextLevel(people, settlement.Level)
				&& settlement.Shelters < LevelOf(people, settlement.Level + 1).Shelters;
			return level.ShelterObjectId > 0 && (crowded || levelWants);
		}

		// Each villager's job for the day: food to eat, guards in tension, a woodcutter and a builder while they need shelters,
		// food to store, then the player's focus; the rest rest. A job is kept while still wanted; the hurt stay hurt.
		static void AssignJobs(Settlement& settlement, const PeopleDefinition& people, const Survey& survey)
		{
			int able = 0;
			for (const Villager& villager : settlement.Villagers)
			{
				able += villager.HurtDays > 0.0f ? 0 : 1;
			}
			int slots[JobCount] = {};
			int left = able;
			const float gatherYield = std::max(0.01f, people.GatherPerPerson);
			const float huntYield = std::max(0.01f, people.HuntPerPerson * people.MeatPerAnimal);
			const int gatherMost = (int)std::ceil(survey.PlantFood / gatherYield);
			const int huntMost = people.HuntPerPerson > 0.0f ? (int)std::ceil(survey.Game * 0.25f / people.HuntPerPerson) : 0;
			const float eat = settlement.Count * people.FoodPerPerson;
			const float store = std::max(1.0f, settlement.Count) * people.FoodPerPerson
				* (LevelOf(people, settlement.Level).StoreDays + survey.StoreDays);
			float made = 0.0f;
			// Food enough to eat, then more while the store isn't full (more still with the focus on food): each worker to
			// whichever of gathering and hunting gives more (dwarves hunt first, elves gather), as far as the land gives it.
			auto feed = [&](float want)
			{
				while (left > 0 && made < want)
				{
					const int gathering = slots[(int)VillagerJob::Gatherer];
					const float gather = gathering < gatherMost
						? std::min(gatherYield, survey.PlantFood - (float)gathering * gatherYield) : 0.0f;
					const float hunt = slots[(int)VillagerJob::Hunter] < huntMost ? huntYield : 0.0f;
					if (gather <= 0.0f && hunt <= 0.0f)
					{
						return;
					}
					const VillagerJob job = gather >= hunt ? VillagerJob::Gatherer : VillagerJob::Hunter;
					slots[(int)job]++;
					made += std::max(gather, hunt);
					left--;
				}
			};
			feed(eat);
			if (settlement.Tension >= GuardFrom && settlement.FeudWith != 0)
			{
				const float share = people.GuardShare * std::min(1.0f, settlement.Tension / 0.7f);
				const int guards = std::min(left, std::max(1, (int)std::ceil((float)able * share)));
				slots[(int)VillagerJob::Guard] += guards;
				left -= guards;
			}
			if (Farming(people, settlement) && left > 0)
			{
				// Farmers for what is ripe and, in a sowing season, what is empty (FarmRules counted the fields) as far as
				// they work fields (more with a barn).
				const int worked = settlement.FieldsMost > 0 ? std::min(settlement.Fields, settlement.FieldsMost) : settlement.Fields;
				const int empty = std::max(0, worked - settlement.FieldsGrowing - settlement.FieldsRipe);
				const float work = (float)settlement.FieldsRipe / std::max(0.01f, people.HarvestPerDay)
					+ (survey.Sowing ? (float)empty / std::max(0.01f, people.SowPerDay) : 0.0f);
				const int farmers = std::min(left, (int)std::ceil(work));
				slots[(int)VillagerJob::Farmer] += farmers;
				left -= farmers;
			}
			const PeopleLevel& level = LevelOf(people, settlement.Level);
			// Something to build: a shelter (new, or an older one to upgrade), else the planned building.
			const bool shelter = WantShelter(settlement, people) || settlement.OldShelters > 0;
			const bool project = shelter || survey.BuildWood >= 0.0f;
			const float wood = shelter ? level.WoodPerShelter : survey.BuildWood;
			const bool needWood = project && wood > 0.0f && settlement.Wood < wood && survey.Trees > 0 && people.FellPerDay > 0.0f;
			const bool canBuild = project && settlement.Wood >= wood;
			if (needWood && left > 0)
			{
				const int cutters = std::min(left, 1 + (settlement.Focus == SettlementFocus::Wood || able >= 10 ? 1 : 0));
				slots[(int)VillagerJob::Woodcutter] += cutters;
				left -= cutters;
			}
			if (canBuild && left > 0)
			{
				const int builders = std::min(left, 1 + (settlement.Focus == SettlementFocus::Building ? 1 : 0));
				slots[(int)VillagerJob::Builder] += builders;
				left -= builders;
			}
			if (Plants(people) && left > 0)
			{
				const int foresters = std::min(left, std::max(1, (int)std::lround((float)able * people.ForesterShare)));
				slots[(int)VillagerJob::Forester] += foresters;
				left -= foresters;
			}
			feed(settlement.Food < store ? eat * (settlement.Focus == SettlementFocus::Food ? 1.6f : 1.25f) : eat);
			// The focus takes the rest.
			const bool woodRoom = settlement.Wood < level.WoodPerShelter * 3.0f && survey.Trees > 0 && people.FellPerDay > 0.0f;
			if (settlement.Focus == SettlementFocus::Food)
			{
				feed(1e9f);
			}
			else if (settlement.Focus == SettlementFocus::Wood && woodRoom)
			{
				slots[(int)VillagerJob::Woodcutter] += left;
				left = 0;
			}
			else if (settlement.Focus == SettlementFocus::Building && level.ShelterObjectId > 0 && settlement.Room < level.MaxPeople)
			{
				const VillagerJob job = canBuild ? VillagerJob::Builder : VillagerJob::Woodcutter;
				if (job == VillagerJob::Builder || woodRoom)
				{
					slots[(int)job] += left;
					left = 0;
				}
			}
			slots[(int)VillagerJob::Resting] += left;

			// Keep a job while it is still wanted; then fill what is left in the order above.
			std::vector<bool> placed(settlement.Villagers.size(), false);
			for (size_t i = 0; i < settlement.Villagers.size(); i++)
			{
				Villager& villager = settlement.Villagers[i];
				if (villager.HurtDays > 0.0f)
				{
					villager.Job = VillagerJob::Hurt;
					placed[i] = true;
				}
				else if (villager.Job != VillagerJob::Hurt && slots[(int)villager.Job] > 0)
				{
					slots[(int)villager.Job]--;
					placed[i] = true;
				}
			}
			const VillagerJob order[] = { VillagerJob::Gatherer, VillagerJob::Hunter, VillagerJob::Guard, VillagerJob::Farmer,
				VillagerJob::Woodcutter, VillagerJob::Builder, VillagerJob::Forester, VillagerJob::Resting };
			size_t next = 0;
			for (size_t i = 0; i < settlement.Villagers.size(); i++)
			{
				if (placed[i])
				{
					continue;
				}
				while (next < std::size(order) && slots[(int)order[next]] == 0)
				{
					next++;
				}
				settlement.Villagers[i].Job = next < std::size(order) ? order[next] : VillagerJob::Resting;
				if (next < std::size(order))
				{
					slots[(int)order[next]]--;
				}
			}
		}

		// What the settlement needs to reach its next level (empty at the top): people, shelters, food in store, and the
		// people's own needs (old trees, mountain rock).
		static std::vector<Need> NextNeeds(const Settlement& settlement, const PeopleDefinition& people)
		{
			std::vector<Need> needs;
			if (HasNextLevel(people, settlement.Level) == false)
			{
				return needs;
			}
			const PeopleLevel& next = LevelOf(people, settlement.Level + 1);
			const float living = (float)(int)(settlement.Count + 0.5f);
			needs.push_back({ "People", living, (float)next.People });
			if (next.Shelters > 0)
			{
				needs.push_back({ "Shelters", (float)settlement.Shelters, (float)next.Shelters });
			}
			if (next.FoodDays > 0.0f)
			{
				const float want = std::ceil(next.FoodDays * std::max((float)next.People, living) * people.FoodPerPerson);
				needs.push_back({ "Food in store", std::floor(settlement.Food), want });
			}
			if (next.NeedObjects > 0)
			{
				needs.push_back({ people.NeedObjectsName.empty() ? std::string("Needed nearby") : Capital(people.NeedObjectsName),
					(float)settlement.Needed, (float)next.NeedObjects });
			}
			if (next.NeedTiles > 0)
			{
				needs.push_back({ people.NeedTilesName.empty() ? std::string("Needed land") : Capital(people.NeedTilesName),
					(float)settlement.NeededTiles, (float)next.NeedTiles });
			}
			return needs;
		}

		static bool NeedsMet(const std::vector<Need>& needs)
		{
			for (const Need& need : needs)
			{
				if (need.Have < need.Want)
				{
					return false;
				}
			}
			return needs.empty() == false;
		}

		static std::string Capital(std::string text)
		{
			if (text.empty() == false && text[0] >= 'a' && text[0] <= 'z')
			{
				text[0] = (char)(text[0] - 'a' + 'A');
			}
			return text;
		}

		// `days` of a settlement's life: the hurt heal, everyone gets a job, they eat what is gathered and hunted, grow with
		// food and room, fell and build, reach the next level when its needs are met, and leave when food runs out.
		static Report Update(Settlement& settlement, const World& world, const std::vector<PeopleDefinition>& peoples, float days,
			Random& random)
		{
			Report report;
			if (settlement.People < 0 || settlement.People >= (int)peoples.size())
			{
				report.Gone = true;
				return report;
			}
			const PeopleDefinition& people = peoples[(size_t)settlement.People];
			const size_t centre = IndexOf(world, settlement.X, settlement.Y);
			const int centreObject = CentreObject(world, people);
			if (centreObject <= 0 || world.Objects->Get(centre) != (uint8_t)centreObject)
			{
				report.Gone = true;
				return report;
			}
			settlement.Days += days;
			Survey survey = Look(world, people, settlement.X, settlement.Y, Reach(people, settlement.Level),
				HomeReach(people, settlement.Level));
			survey.PlantFood *= std::max(0.0f, world.Harvest); // wild food by the season: little in winter
			survey.Sowing = world.Sowing;
			Scores(people, survey, settlement.Water, settlement.FoodNearby, settlement.WoodNearby);
			settlement.Needed = survey.Needed;
			settlement.NeededTiles = survey.NeededTiles;
			settlement.Shelters = survey.Shelters;
			settlement.Room = RoomFor(people, settlement.Level, survey.ShelterRoom);
			settlement.OldShelters = CountOld(world, people, settlement);
			settlement.Buildings = survey.Buildings;
			const BuildingRules::Bonus bonus = world.Buildings != nullptr ? BuildingRules::Of(*world.Buildings, settlement)
				: BuildingRules::Bonus{};
			survey.StoreDays = bonus.StoreDays;
			const BuildingDefinition* planned = Plan(settlement, world, people, survey);
			survey.BuildWood = planned != nullptr ? planned->Wood : -1.0f;
			for (Villager& villager : settlement.Villagers)
			{
				villager.Days += days;
				villager.HurtDays = std::max(0.0f, villager.HurtDays - days);
			}
			AssignJobs(settlement, people, survey);

			// Food: what the gatherers and hunters bring, then they eat.
			const float gatherers = (float)Workers(settlement, VillagerJob::Gatherer);
			const float hunters = (float)Workers(settlement, VillagerJob::Hunter);
			const float gathered = std::min(gatherers * people.GatherPerPerson, survey.PlantFood) * days;
			const float hunted = Hunt(world, people, settlement, hunters * people.HuntPerPerson * days);
			const float meat = people.MeatPerAnimal * (1.0f + bonus.HuntKeep); // a hunting lodge keeps more of each animal
			settlement.Food += gathered + hunted * meat - settlement.Count * people.FoodPerPerson * days;
			const PeopleLevel& level = LevelOf(people, settlement.Level);
			if (settlement.Food < 0.0f)
			{
				settlement.Food = 0.0f;
				settlement.Count -= people.LeavePerDay * days;
			}
			else if (settlement.Food >= settlement.Count * people.FoodPerPerson * 2.0f && settlement.Count < (float)settlement.Room)
			{
				const float born = std::max(0.02f, settlement.Count * people.BirthsPerDay) * days;
				settlement.Count = std::min((float)settlement.Room, settlement.Count + born);
			}
			settlement.Food = std::min(settlement.Food, std::max(1.0f, settlement.Count) * people.FoodPerPerson
				* (level.StoreDays + bonus.StoreDays));

			// Wood and shelters: the woodcutters fell, the builders build once there is a shelter's wood.
			const float cutters = (float)Workers(settlement, VillagerJob::Woodcutter);
			if (cutters > 0.0f)
			{
				settlement.Felling += cutters * people.FellPerDay * days;
				while (settlement.Felling >= 1.0f && Fell(world, people, settlement, random))
				{
					settlement.Felling -= 1.0f;
					report.Felled++;
				}
				settlement.Felling = std::min(settlement.Felling, 1.0f);
			}
			const float builders = (float)Workers(settlement, VillagerJob::Builder);
			settlement.Building += builders * days;
			const bool ready = settlement.Building >= 1.0f && settlement.Wood >= level.WoodPerShelter && level.ShelterObjectId > 0;
			const int home = HomeReach(people, settlement.Level);
			if (ready && WantShelter(settlement, people) && Build(world, level.ShelterObjectId, settlement, random, home))
			{
				settlement.Wood -= level.WoodPerShelter;
				settlement.Building = 0.0f;
				settlement.Shelters++;
				settlement.Room = std::min(level.MaxPeople, settlement.Room + level.PeoplePerShelter);
				report.Built++;
			}
			else if (planned != nullptr && WantShelter(settlement, people) == false && settlement.Building >= 1.0f
				&& settlement.Wood >= planned->Wood && PutUp(world, *planned, settlement, random))
			{
				settlement.Wood -= planned->Wood; // the planned building before rebuilding older shelters
				settlement.Building = 0.0f;
				settlement.Buildings.push_back(planned->BuildingId);
				settlement.Planned = 0;
				report.BuiltBuilding = planned->BuildingId;
			}
			else if (ready && settlement.OldShelters > 0 && Upgrade(world, people, settlement))
			{
				settlement.Wood -= level.WoodPerShelter; // an older shelter rebuilt as this level's
				settlement.Building = 0.0f;
				settlement.OldShelters--;
				report.Upgraded++;
			}
			settlement.Building = builders > 0.0f ? std::min(settlement.Building, 1.0f) : 0.0f;

			// Saplings: the foresters plant where they can grow (elves keep the forest they live by).
			const float foresters = (float)Workers(settlement, VillagerJob::Forester);
			settlement.Planting += foresters * people.PlantPerDay * days;
			while (settlement.Planting >= 1.0f && Plant(world, people, settlement, random))
			{
				settlement.Planting -= 1.0f;
				report.Planted++;
			}
			settlement.Planting = std::min(settlement.Planting, 1.0f);

			if (NeedsMet(NextNeeds(settlement, people)))
			{
				settlement.Level++;
				settlement.Room = RoomFor(people, settlement.Level, survey.ShelterRoom);
				report.LeveledUp = true;
				report.Upgraded += Upgrade(world, people, settlement) ? 1 : 0; // the new level shows at once in one building
			}
			SyncVillagers(settlement, people);
			if (settlement.Count < 0.5f)
			{
				settlement.Count = 0.0f;
				settlement.Villagers.clear();
				world.Objects->Set(centre, 0); // they take down what they put up
				report.Abandoned = true;
			}
			return report;
		}

		// A villager lost (a death in a feud): gone from the settlement and its count.
		static bool Lose(Settlement& settlement, size_t villager)
		{
			if (villager >= settlement.Villagers.size())
			{
				return false;
			}
			settlement.Villagers.erase(settlement.Villagers.begin() + (std::ptrdiff_t)villager);
			settlement.Count = std::max(0.0f, settlement.Count - 1.0f);
			return true;
		}

		// Whether the settlement's people farm there now (they farm, and it is of their FarmFromLevel: a hamlet).
		static bool Farming(const PeopleDefinition& people, const Settlement& settlement)
		{
			return people.FarmTileId >= 0 && people.CropObjectIds.size() >= 2 && settlement.Level >= people.FarmFromLevel;
		}

		// What the settlement builds next (BuildingRules::Wanted): its store when nearly full, a hunting lodge once they
		// hunt, a barn when there is farmland beyond what they work. Sets Planned.
		static const BuildingDefinition* Plan(Settlement& settlement, const World& world, const PeopleDefinition& people,
			const Survey& survey)
		{
			settlement.Planned = 0;
			if (world.Buildings == nullptr)
			{
				return nullptr;
			}
			static const std::vector<int> none;
			const std::vector<int>& owned = world.Talents != nullptr ? *world.Talents : none;
			const float store = std::max(1.0f, settlement.Count) * people.FoodPerPerson
				* (LevelOf(people, settlement.Level).StoreDays + survey.StoreDays);
			BuildingRules::Needs needs;
			needs.StoreFull = settlement.Food >= store * 0.8f;
			needs.Hunting = Workers(settlement, VillagerJob::Hunter) > 0;
			needs.MoreFields = settlement.FieldsMost > 0 && settlement.Fields > settlement.FieldsMost;
			const BuildingDefinition* wanted = BuildingRules::Wanted(*world.Buildings, people, settlement, owned, needs);
			settlement.Planned = wanted != nullptr ? wanted->BuildingId : 0;
			return wanted;
		}

		// All the settlements' people.
		static float People(const SettlementState& state)
		{
			float people = 0.0f;
			for (const Settlement& settlement : state.List)
			{
				people += settlement.Count;
			}
			return people;
		}

		static const char* JobName(VillagerJob job)
		{
			switch (job)
			{
			case VillagerJob::Gatherer: return "Gatherer";
			case VillagerJob::Hunter: return "Hunter";
			case VillagerJob::Woodcutter: return "Woodcutter";
			case VillagerJob::Builder: return "Builder";
			case VillagerJob::Guard: return "Guard";
			case VillagerJob::Hurt: return "Hurt";
			case VillagerJob::Forester: return "Forester";
			case VillagerJob::Farmer: return "Farmer";
			default: return "Resting";
			}
		}

		static constexpr int JobCount = 9;        // VillagerJob values
		static constexpr float GuardFrom = 0.35f; // tension from which some stand guard
	};
}
