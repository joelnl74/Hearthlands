#pragma once

#include "Data/PeopleDefinition.h"
#include "Rules/SettlementRules.h"
#include "Rules/SoilRules.h"
#include "State/Random.h"
#include "State/SettlementState.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// Farming on painted farmland (PeopleDefinition::FarmTileId) near a settlement: farmers sow in spring and harvest what is ripe;
	// crops grow faster on rich, moist soil, frost withers them and each harvest wears the soil. Pure.
	class FarmRules
	{
	public:
		// The farmland within a settlement's reach.
		struct Fields
		{
			int Total = 0;
			int Empty = 0;   // farmland with nothing sown
			int Growing = 0; // sown, not yet ripe
			int Ripe = 0;
		};

		// What a farming step did.
		struct Work
		{
			int Sown = 0;
			int Harvested = 0;
			int Withered = 0;
			float Food = 0.0f;
		};

		static bool Farms(const PeopleDefinition& people, const Settlement& settlement)
		{
			return SettlementRules::Farming(people, settlement);
		}

		// Where the Farmland brush may paint (the player paints only where a people farms): 1 within the reach of a
		// settlement that farms that tile (`Farms`; it grows with the level), else 0. `reach` is sized to the map.
		static void Reach(const SettlementState& state, const std::vector<PeopleDefinition>& peoples, int fieldTileId, int width,
			int height, std::vector<uint8_t>& reach)
		{
			reach.assign((size_t)width * (size_t)height, 0);
			for (const Settlement& settlement : state.List)
			{
				const bool known = settlement.People >= 0 && settlement.People < (int)peoples.size();
				if (known == false)
				{
					continue;
				}
				const PeopleDefinition& people = peoples[(size_t)settlement.People];
				if (people.FarmTileId != fieldTileId || Farms(people, settlement) == false)
				{
					continue;
				}
				const int radius = SettlementRules::Reach(people, settlement.Level);
				for (int dy = -radius; dy <= radius; dy++)
				{
					for (int dx = -radius; dx <= radius; dx++)
					{
						const int x = settlement.X + dx;
						const int y = settlement.Y + dy;
						if (dx * dx + dy * dy <= radius * radius && x >= 0 && y >= 0 && x < width && y < height)
						{
							reach[(size_t)y * (size_t)width + (size_t)x] = 1;
						}
					}
				}
			}
		}

		// Whether a tile may take farmland: any other tile id always, farmland only within `reach`.
		static bool MayPaint(const std::vector<uint8_t>& reach, int fieldTileId, int tileId, int x, int y, int width)
		{
			if (tileId != fieldTileId || fieldTileId < 0)
			{
				return true;
			}
			const size_t index = (size_t)y * (size_t)width + (size_t)x;
			return index < reach.size() && reach[index] != 0;
		}

		// The crop stage (index in CropObjectIds) of an object; -1 not a crop.
		static int Stage(const PeopleDefinition& people, uint8_t objectId)
		{
			for (size_t i = 0; i < people.CropObjectIds.size() && objectId != 0; i++)
			{
				if (people.CropObjectIds[i] == (int)objectId)
				{
					return (int)i;
				}
			}
			return -1;
		}

		// The farmland tiles within the settlement's reach, nearest first (ring by ring).
		static std::vector<size_t> Farmland(const SettlementRules::World& world, const PeopleDefinition& people,
			const Settlement& settlement)
		{
			std::vector<size_t> tiles;
			const TileMap& map = *world.Map;
			const int radius = SettlementRules::Reach(people, settlement.Level);
			for (int ring = 0; ring <= radius; ring++)
			{
				for (int dy = -ring; dy <= ring; dy++)
				{
					for (int dx = -ring; dx <= ring; dx++)
					{
						const int x = settlement.X + dx;
						const int y = settlement.Y + dy;
						const bool edge = std::abs(dx) == ring || std::abs(dy) == ring;
						const bool inReach = edge && dx * dx + dy * dy <= radius * radius && map.InBounds(x, y);
						if (inReach && map.Get(x, y) == (uint8_t)people.FarmTileId)
						{
							tiles.push_back(SettlementRules::IndexOf(world, x, y));
						}
					}
				}
			}
			return tiles;
		}

		static Fields Count(const SettlementRules::World& world, const PeopleDefinition& people, const std::vector<size_t>& farmland)
		{
			Fields fields;
			const int last = (int)people.CropObjectIds.size() - 1;
			for (size_t index : farmland)
			{
				const uint8_t object = world.Objects->Get(index);
				const int stage = Stage(people, object);
				fields.Total++;
				fields.Empty += object == 0 ? 1 : 0;
				fields.Growing += stage >= 0 && stage < last ? 1 : 0;
				fields.Ripe += stage == last ? 1 : 0;
			}
			return fields;
		}

		// How well a field grows: its soil against plain dirt's (0.25 to 2.5) times its moisture (0.1 dry to 1.5 wet).
		static float Fertility(const SettlementRules::World& world, size_t index)
		{
			const float quality = world.Soil != nullptr && world.SoilSettings != nullptr
				? SoilRules::QualityAt(*world.Soil, index, *world.SoilSettings) : SoilRules::Neutral;
			return std::clamp(quality / SoilRules::Neutral, 0.25f, 2.5f);
		}

		static float Wetness(const SettlementRules::World& world, size_t index)
		{
			const float moisture = index < world.Water->Moisture.size() ? (float)world.Water->Moisture[index] / 255.0f : 0.35f;
			return std::clamp(moisture / 0.35f, 0.1f, 1.5f);
		}

		// `days` of the crops: each stage grows by chance (CropDays over the stages, faster on fertile, moist fields and in a
		// growing season); in frost the unripe wither and the ripe rot. Returns how many were lost to frost.
		static int Grow(const SettlementRules::World& world, const PeopleDefinition& people, const std::vector<size_t>& farmland,
			float days, Random& random)
		{
			const int last = (int)people.CropObjectIds.size() - 1;
			if (last < 1)
			{
				return 0;
			}
			const float stageDays = std::max(0.1f, people.CropDays / (float)last);
			int withered = 0;
			for (size_t index : farmland)
			{
				const int stage = Stage(people, world.Objects->Get(index));
				if (stage < 0)
				{
					continue;
				}
				if (world.Frost)
				{
					if (random.Float() < FrostPerDay * days)
					{
						world.Objects->Set(index, 0);
						withered++;
					}
					continue;
				}
				const float chance = days / stageDays * Fertility(world, index) * Wetness(world, index) * std::max(0.0f, world.Growth);
				if (stage < last && random.Float() < chance)
				{
					world.Objects->Set(index, (uint8_t)people.CropObjectIds[(size_t)stage + 1]);
				}
			}
			return withered;
		}

		// `farmerDays` of work (carried in settlement.FarmWork): the ripe fields nearest the centre harvested first, then in
		// a sowing season empty ones sown. A harvest gives CropFood times the field's soil (0.5 to 2) and wears the soil.
		static Work Tend(const SettlementRules::World& world, const PeopleDefinition& people, Settlement& settlement,
			const std::vector<size_t>& farmland, float farmerDays, int most = 1 << 30)
		{
			Work work;
			const int last = (int)people.CropObjectIds.size() - 1;
			if (last < 1)
			{
				return work;
			}
			settlement.FarmWork += farmerDays;
			const float harvestDay = std::max(0.01f, people.HarvestPerDay);
			const float sowDay = std::max(0.01f, people.SowPerDay);
			for (size_t index : farmland)
			{
				if (settlement.FarmWork < 1.0f / harvestDay)
				{
					break;
				}
				if (Stage(people, world.Objects->Get(index)) != last)
				{
					continue;
				}
				work.Food += people.CropFood * std::clamp(Fertility(world, index), 0.5f, 2.0f);
				world.Objects->Set(index, 0);
				Wear(world, index, people.SoilWear);
				settlement.FarmWork -= 1.0f / harvestDay;
				work.Harvested++;
			}
			int worked = 0; // fields with a crop: they sow no more than `most` (their people's FieldsWorked, more with a barn)
			for (size_t index : farmland)
			{
				worked += Stage(people, world.Objects->Get(index)) >= 0 ? 1 : 0;
			}
			for (size_t index : farmland)
			{
				if (world.Sowing == false || settlement.FarmWork < 1.0f / sowDay || worked >= most)
				{
					break;
				}
				if (world.Objects->Get(index) != 0)
				{
					continue;
				}
				world.Objects->Set(index, (uint8_t)people.CropObjectIds[0]);
				settlement.FarmWork -= 1.0f / sowDay;
				work.Sown++;
				worked++;
			}
			settlement.FarmWork = std::clamp(settlement.FarmWork, 0.0f, 1.0f); // idle farmers don't save up work
			return work;
		}

		// A harvest takes `wear` (0 to 1) of the field's soil richness.
		static void Wear(const SettlementRules::World& world, size_t index, float wear)
		{
			if (world.Soil == nullptr || index >= world.Soil->Fertility.size() || wear <= 0.0f)
			{
				return;
			}
			const int taken = (int)(wear * 255.0f + 0.5f);
			world.Soil->Fertility[index] = (uint8_t)std::max(0, (int)world.Soil->Fertility[index] - taken);
			world.Soil->Revision++;
		}

		// `days` of farming at a settlement (before its SettlementRules::Update, which reads the fields to give out jobs):
		// the crops grow; its farmers (from the last jobs) harvest and sow; the fields are counted. The harvest goes to its store.
		static Work Step(Settlement& settlement, const SettlementRules::World& world, const std::vector<PeopleDefinition>& peoples,
			float days, Random& random)
		{
			Work work;
			const bool known = settlement.People >= 0 && settlement.People < (int)peoples.size();
			const PeopleDefinition* people = known ? &peoples[(size_t)settlement.People] : nullptr;
			if (people == nullptr || people->FarmTileId < 0 || people->CropObjectIds.size() < 2)
			{
				settlement.Fields = 0;
				settlement.FieldsGrowing = 0;
				settlement.FieldsRipe = 0;
				return work;
			}
			const std::vector<size_t> farmland = Farmland(world, *people, settlement);
			work.Withered = Grow(world, *people, farmland, days, random);
			if (Farms(*people, settlement))
			{
				const float farmers = (float)SettlementRules::Workers(settlement, VillagerJob::Farmer);
				const int barn = world.Buildings != nullptr ? BuildingRules::Of(*world.Buildings, settlement).Fields : 0;
				settlement.FieldsMost = std::max(0, people->FieldsWorked + barn);
				const Work tended = Tend(world, *people, settlement, farmland, farmers * days, settlement.FieldsMost);
				work.Sown = tended.Sown;
				work.Harvested = tended.Harvested;
				work.Food = tended.Food;
				settlement.Food += tended.Food;
			}
			const Fields fields = Count(world, *people, farmland);
			settlement.Fields = fields.Total;
			settlement.FieldsGrowing = fields.Growing;
			settlement.FieldsRipe = fields.Ripe;
			return work;
		}

		// Farmers the fields want: harvesting what is ripe and, in a sowing season, sowing what is empty, in a day.
		static int FarmersWanted(const PeopleDefinition& people, const Fields& fields, bool sowing)
		{
			const float harvest = (float)fields.Ripe / std::max(0.01f, people.HarvestPerDay);
			const float sow = sowing ? (float)fields.Empty / std::max(0.01f, people.SowPerDay) : 0.0f;
			return (int)std::ceil(harvest + sow);
		}

		static constexpr float FrostPerDay = 0.5f; // share of the crops frost takes a day
	};
}
