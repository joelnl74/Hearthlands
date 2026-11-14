#pragma once

#include "Data/PeopleDefinition.h"
#include "Rules/SettlementRules.h"
#include "State/Random.h"
#include "State/SettlementState.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// Settlements come on their own once a talent lets a people settle: the first group soon to their best place, later
	// groups less often per settlement they have (SettleDecay), and full settlements branch out nearby. Pure.
	class SettlingRules
	{
	public:
		// A settlement founded in a step: by a new group, or by a group from settlement From.
		struct Arrival
		{
			int Id = 0;
			int Row = -1;  // the people's row
			int From = 0;  // the settlement they came from; 0 a new group
		};

		// A settlement that became a branch of its people (elves into wood elves).
		struct Evolution
		{
			int Id = 0;
			int From = -1; // people rows
			int To = -1;
		};

		struct Report
		{
			std::vector<Arrival> Arrivals;
			std::vector<int> NoPlace; // peoples (rows) who looked for a place and found none
			std::vector<Evolution> Evolved;
		};

		// What Block x Block tiles hold, summed: the coarse map Find scores places on; Totals sums the cells above and left
		// of each corner ((Columns + 1) x (Rows + 1)), so any box of blocks adds up at once.
		struct Blocks
		{
			int X0 = 0;                 // the first block's first tile
			int Y0 = 0;
			int Columns = 0;
			int Rows = 0;
			std::vector<SettlementRules::Survey> Cells;
			std::vector<SettlementRules::Survey> Totals;
		};

		static constexpr int Block = 4;          // tiles a side of a block
		static constexpr int Candidates = 12;    // best blocks looked at tile by tile
		static constexpr float NoPlaceWait = 6.0f; // days a people waits after finding no place
		static constexpr float RuinDays = 60.0f;   // days new groups keep away from where a settlement emptied
		static constexpr float FoeWeight = 2.0f;   // how much a disliked neighbour close by spoils a place (closeness squared)

		// The land's sums per block over the window [x0, x1) x [y0, y1) (clamped to the map, out to whole blocks).
		static Blocks Sum(const SettlementRules::World& world, const PeopleDefinition& people, int x0, int y0, int x1, int y1)
		{
			const TileMap& map = *world.Map;
			Blocks blocks;
			blocks.X0 = std::max(0, x0) / Block * Block;
			blocks.Y0 = std::max(0, y0) / Block * Block;
			const int right = std::min(map.GetWidth(), std::max(x1, blocks.X0));
			const int bottom = std::min(map.GetHeight(), std::max(y1, blocks.Y0));
			blocks.Columns = (right - blocks.X0 + Block - 1) / Block;
			blocks.Rows = (bottom - blocks.Y0 + Block - 1) / Block;
			blocks.Cells.assign((size_t)blocks.Columns * (size_t)blocks.Rows, SettlementRules::Survey{});
			for (int y = blocks.Y0; y < bottom; y++)
			{
				for (int x = blocks.X0; x < right; x++)
				{
					SettlementRules::Survey& cell = blocks.Cells[(size_t)((y - blocks.Y0) / Block) * (size_t)blocks.Columns
						+ (size_t)((x - blocks.X0) / Block)];
					const size_t index = SettlementRules::IndexOf(world, x, y);
					const uint8_t tile = map.Get(x, y);
					const bool inland = world.Land->empty() || (index < world.Land->size() && (*world.Land)[index] != 0);
					cell.FreshWater += tile == world.Water->WaterTileId && inland ? 1 : 0;
					cell.NeededTiles += std::find(people.NeedTileIds.begin(), people.NeedTileIds.end(), (int)tile)
						!= people.NeedTileIds.end() ? 1 : 0;
					const ObjectDefinition* object = SettlementRules::Object(world, world.Objects->Get(index));
					const bool covered = object != nullptr && (object->Kind == ObjectKind::Shrub || object->Kind == ObjectKind::Tree);
					cell.Meadow += WildlifeRules::Walkable(tile, *world.Water) && covered == false ? 1 : 0;
					if (object != nullptr)
					{
						cell.PlantFood += object->Food;
						cell.Wood += object->Wood;
						cell.Trees += object->Wood > 0.0f ? 1 : 0;
						cell.Needed += std::find(people.NeedObjectIds.begin(), people.NeedObjectIds.end(), object->ObjectId)
							!= people.NeedObjectIds.end() ? 1 : 0;
					}
				}
			}
			return blocks;
		}

		// Whether (x, y) keeps `spacing` tiles from every settlement's centre and every fresh ruin.
		static bool Spaced(const SettlementState& state, int x, int y, int spacing)
		{
			for (const Settlement& settlement : state.List)
			{
				const int dx = settlement.X - x;
				const int dy = settlement.Y - y;
				if (dx * dx + dy * dy < spacing * spacing)
				{
					return false;
				}
			}
			for (const Ruin& ruin : state.Ruins)
			{
				const int dx = ruin.X - x;
				const int dy = ruin.Y - y;
				if (dx * dx + dy * dy < spacing * spacing)
				{
					return false;
				}
			}
			return true;
		}

		// How much the nearest settlement of a people `people` dislikes spoils (x, y): 0 none within its DislikeRadius, up to
		// FoeWeight right beside it (by closeness squared). `peoples`: the table, to know whom they dislike.
		static float FoePenalty(const SettlementState& state, const std::vector<PeopleDefinition>& peoples, const PeopleDefinition& people,
			int x, int y)
		{
			float worst = 0.0f;
			const float radius = (float)std::max(1, people.DislikeRadius);
			for (const Settlement& settlement : state.List)
			{
				const bool known = settlement.People >= 0 && settlement.People < (int)peoples.size();
				if (known == false || std::find(people.Dislikes.begin(), people.Dislikes.end(), peoples[(size_t)settlement.People].PeopleId)
					== people.Dislikes.end())
				{
					continue;
				}
				const float dx = (float)(settlement.X - x);
				const float dy = (float)(settlement.Y - y);
				const float close = std::max(0.0f, 1.0f - std::sqrt(dx * dx + dy * dy) / radius);
				worst = std::max(worst, close * close);
			}
			return worst * FoeWeight;
		}

		// The settlement at `index` goes (emptied or its centre gone): its place is a ruin new groups keep away from.
		static void Remove(SettlementState& state, size_t index)
		{
			if (index >= state.List.size())
			{
				return;
			}
			state.Ruins.push_back({ state.List[index].X, state.List[index].Y, RuinDays });
			state.List.erase(state.List.begin() + (std::ptrdiff_t)index);
			state.Revision++;
		}

		// The best place for the people's centre, anywhere (fromX < 0) or within `reach` of (fromX, fromY): it meets their needs,
		// keeps SettleSpacing from settlements and ruins, and they like it most (less near peoples they dislike). False: none.
		static bool Find(const SettlementRules::World& world, const SettlementState& state, const PeopleDefinition& people, int fromX,
			int fromY, int reach, int& outX, int& outY, const std::vector<PeopleDefinition>& peoples = {})
		{
			const TileMap& map = *world.Map;
			const bool local = fromX >= 0;
			const int margin = reach + people.Radius + Block;
			Blocks blocks = local ? Sum(world, people, fromX - margin, fromY - margin, fromX + margin + 1, fromY + margin + 1)
				: Sum(world, people, 0, 0, map.GetWidth(), map.GetHeight());
			Total(blocks);
			struct Candidate
			{
				float Score = 0.0f;
				int X = 0;
				int Y = 0;
			};
			std::vector<Candidate> best;
			const int span = std::max(0, (int)std::lround(0.886f * (float)people.Radius / (float)Block - 0.5f)); // the circle's area
			for (int by = 0; by < blocks.Rows; by++)
			{
				for (int bx = 0; bx < blocks.Columns; bx++)
				{
					const int x = blocks.X0 + bx * Block + Block / 2;
					const int y = blocks.Y0 + by * Block + Block / 2;
					const int fromDx = x - fromX;
					const int fromDy = y - fromY;
					const int apart = fromDx * fromDx + fromDy * fromDy;
					const int spacing = people.SettleSpacing;
					if (map.InBounds(x, y) == false || (local && (apart > reach * reach || apart < spacing * spacing))
						|| Spaced(state, x, y, spacing) == false)
					{
						continue;
					}
					SettlementRules::Survey survey = Around(blocks, bx, by, span);
					survey.Game = SettlementRules::Game(world, people, x, y);
					if (SettlementRules::Meets(people, survey) == false)
					{
						continue;
					}
					const float penalty = peoples.empty() ? 0.0f : FoePenalty(state, peoples, people, x, y);
					best.push_back({ SettlementRules::Liking(people, survey) - penalty, x, y });
				}
			}
			std::stable_sort(best.begin(), best.end(), [](const Candidate& a, const Candidate& b) { return a.Score > b.Score; });
			for (size_t i = 0; i < best.size() && i < (size_t)Candidates; i++)
			{
				// The block's centre, then the tiles round it: the first a centre can go on whose own survey meets their needs.
				for (int ring = 0; ring <= Block; ring++)
				{
					for (int dy = -ring; dy <= ring; dy++)
					{
						for (int dx = -ring; dx <= ring; dx++)
						{
							const int x = best[i].X + dx;
							const int y = best[i].Y + dy;
							const bool edge = std::abs(dx) == ring || std::abs(dy) == ring;
							const bool fits = SettlementRules::Buildable(world, x, y) && SettlementRules::HighEnough(world, people, x, y);
							if (edge == false || fits == false || Spaced(state, x, y, people.SettleSpacing) == false)
							{
								continue;
							}
							if (SettlementRules::Meets(people, SettlementRules::Look(world, people, x, y)))
							{
								outX = x;
								outY = y;
								return true;
							}
						}
					}
				}
			}
			return false;
		}

		// The blocks within `span` of (bx, by) (a square as large as the people's Radius circle), summed from the totals.
		static SettlementRules::Survey Around(const Blocks& blocks, int bx, int by, int span)
		{
			const int x0 = std::max(0, bx - span);
			const int y0 = std::max(0, by - span);
			const int x1 = std::min(blocks.Columns, bx + span + 1);
			const int y1 = std::min(blocks.Rows, by + span + 1);
			const size_t stride = (size_t)blocks.Columns + 1;
			const SettlementRules::Survey& a = blocks.Totals[(size_t)y1 * stride + (size_t)x1];
			const SettlementRules::Survey& b = blocks.Totals[(size_t)y0 * stride + (size_t)x1];
			const SettlementRules::Survey& c = blocks.Totals[(size_t)y1 * stride + (size_t)x0];
			const SettlementRules::Survey& d = blocks.Totals[(size_t)y0 * stride + (size_t)x0];
			SettlementRules::Survey survey;
			survey.FreshWater = a.FreshWater - b.FreshWater - c.FreshWater + d.FreshWater;
			survey.PlantFood = a.PlantFood - b.PlantFood - c.PlantFood + d.PlantFood;
			survey.Wood = a.Wood - b.Wood - c.Wood + d.Wood;
			survey.Trees = a.Trees - b.Trees - c.Trees + d.Trees;
			survey.Meadow = a.Meadow - b.Meadow - c.Meadow + d.Meadow;
			survey.Needed = a.Needed - b.Needed - c.Needed + d.Needed;
			survey.NeededTiles = a.NeededTiles - b.NeededTiles - c.NeededTiles + d.NeededTiles;
			return survey;
		}

		// The totals of `blocks.Cells` (Blocks::Totals).
		static void Total(Blocks& blocks)
		{
			const size_t stride = (size_t)blocks.Columns + 1;
			blocks.Totals.assign(stride * ((size_t)blocks.Rows + 1), SettlementRules::Survey{});
			for (int y = 0; y < blocks.Rows; y++)
			{
				for (int x = 0; x < blocks.Columns; x++)
				{
					const SettlementRules::Survey& cell = blocks.Cells[(size_t)y * (size_t)blocks.Columns + (size_t)x];
					const SettlementRules::Survey& up = blocks.Totals[(size_t)y * stride + (size_t)x + 1];
					const SettlementRules::Survey& left = blocks.Totals[((size_t)y + 1) * stride + (size_t)x];
					const SettlementRules::Survey& corner = blocks.Totals[(size_t)y * stride + (size_t)x];
					SettlementRules::Survey& total = blocks.Totals[((size_t)y + 1) * stride + (size_t)x + 1];
					total.FreshWater = cell.FreshWater + up.FreshWater + left.FreshWater - corner.FreshWater;
					total.PlantFood = cell.PlantFood + up.PlantFood + left.PlantFood - corner.PlantFood;
					total.Wood = cell.Wood + up.Wood + left.Wood - corner.Wood;
					total.Trees = cell.Trees + up.Trees + left.Trees - corner.Trees;
					total.Meadow = cell.Meadow + up.Meadow + left.Meadow - corner.Meadow;
					total.Needed = cell.Needed + up.Needed + left.Needed - corner.Needed;
					total.NeededTiles = cell.NeededTiles + up.NeededTiles + left.NeededTiles - corner.NeededTiles;
				}
			}
		}

		static int CountOf(const SettlementState& state, int row)
		{
			int count = 0;
			for (const Settlement& settlement : state.List)
			{
				count += settlement.People == row ? 1 : 0;
			}
			return count;
		}

		// Chance a day a new group of the people at `row` comes, with `chance` from their talents: their first soon
		// (ArrivePerDay at least), then `chance` less by SettleDecay for each settlement they have; 0 without a talent.
		static float GroupChance(const SettlementState& state, const PeopleDefinition& people, int row, float chance)
		{
			if (chance <= 0.0f)
			{
				return 0.0f;
			}
			const int have = CountOf(state, row);
			const float decay = std::clamp(people.SettleDecay, 0.0f, 1.0f);
			return have == 0 ? std::max(chance, people.ArrivePerDay) : chance * std::pow(decay, (float)have);
		}

		// `days` of settling: new groups by their chances (per people row: TalentState::SettleChance of their PeopleId; for a
		// branch people (EvolvesFromId) above 0 only while it may arise), settlements becoming branches, branching out.
		static Report Step(SettlementState& state, const SettlementRules::World& world, const std::vector<PeopleDefinition>& peoples,
			const std::vector<float>& chances, float days, Random& random)
		{
			Report report;
			state.SearchWait.resize(peoples.size(), 0.0f);
			for (size_t i = 0; i < state.Ruins.size();)
			{
				state.Ruins[i].Days -= days;
				if (state.Ruins[i].Days <= 0.0f)
				{
					state.Ruins.erase(state.Ruins.begin() + (std::ptrdiff_t)i);
					continue;
				}
				i++;
			}
			for (size_t row = 0; row < peoples.size(); row++)
			{
				state.SearchWait[row] = std::max(0.0f, state.SearchWait[row] - days);
				const PeopleDefinition& people = peoples[row];
				const float given = row < chances.size() && people.EvolvesFromId == 0 ? chances[row] : 0.0f; // a branch only evolves
				const float chance = GroupChance(state, people, (int)row, given);
				if (chance <= 0.0f || state.SearchWait[row] > 0.0f || random.Float() >= chance * days)
				{
					continue;
				}
				int x = 0;
				int y = 0;
				if (Find(world, state, people, -1, -1, 0, x, y, peoples) == false)
				{
					state.SearchWait[row] = NoPlaceWait;
					report.NoPlace.push_back((int)row);
					continue;
				}
				const int spread = std::max(0, people.GroupMax - people.GroupMin);
				const int group = std::max(1, people.GroupMin) + (int)random.Below((uint32_t)spread + 1u);
				const int id = SettlementRules::Found(state, world, peoples, (int)row, x, y, group);
				if (id != 0)
				{
					report.Arrivals.push_back({ id, (int)row, 0 });
				}
			}
			Evolve(state, world, peoples, chances, days, random, report);
			Branch(state, world, peoples, chances, days, random, report);
			return report;
		}

		// The first people that branches from the people at `row` (EvolvesFromId is its PeopleId); -1 none.
		static int BranchOf(const std::vector<PeopleDefinition>& peoples, int row)
		{
			for (size_t other = 0; other < peoples.size() && row >= 0 && row < (int)peoples.size(); other++)
			{
				if (peoples[other].EvolvesFromId > 0 && peoples[other].EvolvesFromId == peoples[(size_t)row].PeopleId)
				{
					return (int)other;
				}
			}
			return -1;
		}

		// Settlements whose land meets a branch of their people's needs (old trees for wood elves) become that branch, by
		// chance (EvolvePerDay), while it may arise (its chance above 0). Every settlement keeps what its branch needs counted.
		static void Evolve(SettlementState& state, const SettlementRules::World& world, const std::vector<PeopleDefinition>& peoples,
			const std::vector<float>& chances, float days, Random& random, Report& report)
		{
			for (Settlement& settlement : state.List)
			{
				const int branch = BranchOf(peoples, settlement.People);
				settlement.EvolveTo = branch;
				if (branch < 0)
				{
					continue;
				}
				const PeopleDefinition& to = peoples[(size_t)branch];
				const SettlementRules::Survey survey = SettlementRules::Look(world, to, settlement.X, settlement.Y);
				const bool objects = to.NeedObjects > 0;
				settlement.EvolveHave = objects ? survey.Needed : survey.NeededTiles;
				settlement.EvolveWant = objects ? to.NeedObjects : to.NeedTiles;
				settlement.EvolveAllowed = (size_t)branch < chances.size() && chances[(size_t)branch] > 0.0f;
				if (settlement.EvolveAllowed && SettlementRules::Meets(to, survey) && random.Float() < to.EvolvePerDay * days)
				{
					const int from = settlement.People;
					Become(settlement, world, peoples, branch);
					report.Evolved.push_back({ settlement.Id, from, branch });
				}
			}
		}

		// The settlement's people become the people at `row`: its centre and shelters turn into theirs, level by level (a
		// bower into a leaf hut); its name, villagers and stores stay.
		static void Become(Settlement& settlement, const SettlementRules::World& world, const std::vector<PeopleDefinition>& peoples,
			int row)
		{
			const PeopleDefinition& from = peoples[(size_t)settlement.People];
			const PeopleDefinition& to = peoples[(size_t)row];
			const int reach = SettlementRules::HomeReach(from, settlement.Level);
			for (int dy = -reach; dy <= reach; dy++)
			{
				for (int dx = -reach; dx <= reach; dx++)
				{
					const int x = settlement.X + dx;
					const int y = settlement.Y + dy;
					if (world.Map->InBounds(x, y) == false)
					{
						continue;
					}
					const size_t index = SettlementRules::IndexOf(world, x, y);
					const uint8_t id = world.Objects->Get(index);
					for (size_t level = 0; level < from.Levels.size() && id != 0; level++)
					{
						const int shelter = SettlementRules::LevelOf(to, (int)level).ShelterObjectId;
						if (from.Levels[level].ShelterObjectId == (int)id && shelter > 0 && shelter < 256)
						{
							world.Objects->Set(index, (uint8_t)shelter);
							break;
						}
					}
				}
			}
			const int centre = SettlementRules::CentreObject(world, to);
			if (centre > 0 && centre < 256)
			{
				world.Objects->Set(SettlementRules::IndexOf(world, settlement.X, settlement.Y), (uint8_t)centre);
			}
			settlement.People = row;
			settlement.Level = std::min(settlement.Level, std::max(0, (int)to.Levels.size() - 1));
			settlement.EvolveTo = BranchOf(peoples, row);
		}

		// Full settlements (at least BranchFromLevel, no room left, food for two days) send a group of GroupMin to the
		// best place within BranchReach, now and then (BranchPerDay), while the people may settle.
		static void Branch(SettlementState& state, const SettlementRules::World& world, const std::vector<PeopleDefinition>& peoples,
			const std::vector<float>& chances, float days, Random& random, Report& report)
		{
			const size_t count = state.List.size();
			for (size_t i = 0; i < count; i++)
			{
				const Settlement& settlement = state.List[i];
				const int row = settlement.People;
				if (row < 0 || row >= (int)peoples.size() || (size_t)row >= chances.size() || chances[(size_t)row] <= 0.0f)
				{
					continue;
				}
				const PeopleDefinition& people = peoples[(size_t)row];
				const int group = std::max(1, people.GroupMin);
				const bool full = settlement.Count + 0.5f >= (float)settlement.Room && settlement.Room > 0;
				const bool fed = settlement.Food >= settlement.Count * people.FoodPerPerson * 2.0f;
				const bool enough = (int)settlement.Villagers.size() >= group * 2;
				if (settlement.Level < people.BranchFromLevel || full == false || fed == false || enough == false
					|| random.Float() >= people.BranchPerDay * days)
				{
					continue;
				}
				int x = 0;
				int y = 0;
				if (Find(world, state, people, settlement.X, settlement.Y, people.BranchReach, x, y, peoples) == false)
				{
					continue;
				}
				Settlement& from = state.List[i];
				const std::vector<Villager> movers(from.Villagers.end() - group, from.Villagers.end());
				const int fromId = from.Id;
				const float food = std::min(from.Food * 0.3f, (float)group * people.FoodPerPerson * 2.0f);
				const int id = SettlementRules::Found(state, world, peoples, row, x, y, group, movers);
				if (id == 0)
				{
					continue;
				}
				Settlement& source = state.List[i]; // the list grew: find it again
				source.Villagers.resize(source.Villagers.size() - (size_t)group);
				source.Count = std::max(0.0f, source.Count - (float)group);
				source.Food -= food;
				state.List.back().Food = food;
				report.Arrivals.push_back({ id, row, fromId });
			}
		}
	};
}
