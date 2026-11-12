#pragma once

#include "Data/AnimalDefinition.h"
#include "Data/ObjectDefinition.h"
#include "Rules/HabitatRules.h"
#include "State/HeightMap.h"
#include "State/Hydrology.h"
#include "State/ObjectLayer.h"
#include "State/Random.h"
#include "State/TileMap.h"
#include "State/WildlifeState.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// The animals' numbers per region: what its land holds of each species (Measure: suiting tiles, cover, water) and how many
	// live there (Grow: births, leaving, hunting, spreading, newcomers). Update does a few regions a tick. Pure.
	class WildlifeRules
	{
	public:
		static constexpr float LeavePerDay = 0.5f; // share of the animals above what the land holds that leave (or die) a day
		static constexpr float Gone = 0.05f;       // fewer than this is none
		static constexpr float DiscoverAt = 1.0f;  // a species is discovered once this many live on the map

		// What the season and the talents change: births (Life), the migrants' share here, what the land holds.
		struct Factors
		{
			float Life = 1.0f;
			float Migrants = 1.0f;
			float Capacity = 1.0f;
		};

		// Tiles, land, cover and water in a region, counted once for every species.
		struct Survey
		{
			int Tiles = 0;
			int Land = 0;    // not water or riverbed
			int Covered = 0; // land under a shrub or tree
			int Water = 0;
		};

		static WildlifeState Start(int width, int height, size_t species, int regionSize = 16)
		{
			WildlifeState state;
			state.RegionSize = regionSize > 0 ? regionSize : 16;
			state.RegionsX = width > 0 ? (width + state.RegionSize - 1) / state.RegionSize : 0;
			state.RegionsY = height > 0 ? (height + state.RegionSize - 1) / state.RegionSize : 0;
			state.Population.assign(species, std::vector<float>(state.GetRegionCount(), 0.0f));
			state.Capacity.assign(species, std::vector<float>(state.GetRegionCount(), 0.0f));
			state.Discovered.assign(species, 0);
			return state;
		}

		// Per object id: 1 for shrubs and trees (cover), 0 otherwise.
		static std::vector<uint8_t> CoverOf(const std::vector<ObjectDefinition>& objects)
		{
			std::vector<uint8_t> cover(256, 0);
			for (const ObjectDefinition& object : objects)
			{
				const bool covers = object.Kind == ObjectKind::Shrub || object.Kind == ObjectKind::Tree;
				if (object.ObjectId > 0 && object.ObjectId < 256 && covers)
				{
					cover[(size_t)object.ObjectId] = 1;
				}
			}
			return cover;
		}

		// The row of the species with this AnimalId, or -1.
		static int RowOf(const std::vector<AnimalDefinition>& animals, int animalId)
		{
			for (size_t i = 0; i < animals.size(); i++)
			{
				if (animals[i].AnimalId == animalId)
				{
					return (int)i;
				}
			}
			return -1;
		}

		// Whether a tile suits the species: its tile id (any land, or the water for fish), moisture and height; fish also its
		// habitat bits (Hydrology::Habitat). Other species' Habitat is the region's (Holds).
		static bool Suits(const AnimalDefinition& animal, uint8_t tile, uint8_t moisture, int level, const Hydrology& water,
			uint8_t habitat = 0)
		{
			if (animal.Kind == AnimalKind::Fish && HabitatRules::Matches(animal.Habitat, habitat) == false)
			{
				return false;
			}
			bool tileFits = false;
			if (animal.TileIds.empty())
			{
				const bool wet = tile == water.WaterTileId || tile == water.BedTileId;
				tileFits = animal.Kind == AnimalKind::Fish ? tile == water.WaterTileId : wet == false;
			}
			for (int id : animal.TileIds)
			{
				tileFits = tileFits || id == (int)tile;
			}
			const float wetness = (float)moisture / 255.0f;
			return tileFits && wetness >= animal.MinMoisture && wetness <= animal.MaxMoisture && level >= animal.MinLevel
				&& level <= animal.MaxLevel;
		}

		// A tile walkers can stand on: land (not water or riverbed).
		static bool Walkable(uint8_t tile, const Hydrology& water)
		{
			return tile != water.WaterTileId && tile != water.BedTileId;
		}

		// The tiles (x0, y0) to (x1, y1) inclusive of a region.
		static void Bounds(const WildlifeState& state, size_t region, int width, int height, int& x0, int& y0, int& x1, int& y1)
		{
			const int rx = state.RegionsX > 0 ? (int)(region % (size_t)state.RegionsX) : 0;
			const int ry = state.RegionsX > 0 ? (int)(region / (size_t)state.RegionsX) : 0;
			x0 = rx * state.RegionSize;
			y0 = ry * state.RegionSize;
			x1 = std::min(x0 + state.RegionSize, width) - 1;
			y1 = std::min(y0 + state.RegionSize, height) - 1;
		}

		static Survey Look(const TileMap& map, const Hydrology& water, const ObjectLayer& objects, const std::vector<uint8_t>& cover,
			int x0, int y0, int x1, int y1)
		{
			Survey survey;
			for (int y = y0; y <= y1; y++)
			{
				for (int x = x0; x <= x1; x++)
				{
					const size_t index = (size_t)y * (size_t)map.GetWidth() + (size_t)x;
					const uint8_t tile = map.Get(x, y);
					survey.Tiles++;
					survey.Water += tile == water.WaterTileId ? 1 : 0;
					if (Walkable(tile, water))
					{
						survey.Land++;
						const uint8_t object = objects.Get(index);
						survey.Covered += object < cover.size() && cover[object] != 0 ? 1 : 0;
					}
				}
			}
			return survey;
		}

		// What the region's land holds of the species: the tiles that suit it times its Density, none when its cover or
		// water is out of range, or (not fish) it has fewer than MinHabitat tiles of the species' Habitat.
		static float Holds(const AnimalDefinition& animal, const TileMap& map, const Hydrology& water, const HeightMap& heights,
			const Survey& survey, int x0, int y0, int x1, int y1)
		{
			const float covered = survey.Land > 0 ? (float)survey.Covered / (float)survey.Land : 0.0f;
			const float wet = survey.Tiles > 0 ? (float)survey.Water / (float)survey.Tiles : 0.0f;
			if (covered < animal.MinCover || covered > animal.MaxCover || wet < animal.MinWater)
			{
				return 0.0f;
			}
			const bool flat = heights.GetLevels().size() != map.GetTiles().size();
			const bool regional = animal.Kind != AnimalKind::Fish && animal.Habitat != WaterHabitat::None;
			int suits = 0;
			int habitat = 0;
			for (int y = y0; y <= y1; y++)
			{
				for (int x = x0; x <= x1; x++)
				{
					const size_t index = (size_t)y * (size_t)map.GetWidth() + (size_t)x;
					const uint8_t moisture = index < water.Moisture.size() ? water.Moisture[index] : (uint8_t)0;
					const uint8_t bits = index < water.Habitat.size() ? water.Habitat[index] : (uint8_t)0;
					suits += Suits(animal, map.Get(x, y), moisture, flat ? 0 : heights.Get(x, y), water, bits) ? 1 : 0;
					habitat += regional && bits != 0 && HabitatRules::Matches(animal.Habitat, bits) ? 1 : 0;
				}
			}
			if (regional && habitat < (animal.MinHabitat > 1 ? animal.MinHabitat : 1))
			{
				return 0.0f;
			}
			return (float)suits * animal.Density;
		}

		// Works out what a region's land holds of every species again.
		static void Measure(WildlifeState& state, const std::vector<AnimalDefinition>& animals, const TileMap& map,
			const Hydrology& water, const HeightMap& heights, const ObjectLayer& objects, const std::vector<uint8_t>& cover,
			size_t region)
		{
			if (region >= state.GetRegionCount())
			{
				return;
			}
			int x0 = 0;
			int y0 = 0;
			int x1 = 0;
			int y1 = 0;
			Bounds(state, region, map.GetWidth(), map.GetHeight(), x0, y0, x1, y1);
			const Survey survey = Look(map, water, objects, cover, x0, y0, x1, y1);
			for (size_t s = 0; s < animals.size() && s < state.Capacity.size(); s++)
			{
				state.Capacity[s][region] = Holds(animals[s], map, water, heights, survey, x0, y0, x1, y1);
			}
		}

		// What the region holds of the species now: its land's, times the talents and (migrants) the season; a hunter only
		// as much as its prey feeds.
		static float Room(const WildlifeState& state, const std::vector<AnimalDefinition>& animals, size_t species, size_t region,
			const Factors& factors)
		{
			const AnimalDefinition& animal = animals[species];
			float room = state.Capacity[species][region] * factors.Capacity * (animal.Migrates ? factors.Migrants : 1.0f);
			const int prey = animal.PreyId >= 0 ? RowOf(animals, animal.PreyId) : -1;
			if (prey >= 0 && (size_t)prey < state.Population.size())
			{
				const float fed = state.Population[(size_t)prey][region] / (animal.PreyPerAnimal > 0.0f ? animal.PreyPerAnimal : 1.0f);
				room = std::min(room, fed);
			}
			else if (animal.PreyId >= 0)
			{
				room = 0.0f; // its prey isn't in the table
			}
			return room;
		}

		// `days` of a region's life: each species grows towards what it holds (or leaves above it), hunters eat their prey,
		// some move to a neighbouring region with room, and a region with room and none may get a few from outside.
		static void Grow(WildlifeState& state, const std::vector<AnimalDefinition>& animals, size_t region, float days,
			const Factors& factors, Random& random)
		{
			if (region >= state.GetRegionCount() || days <= 0.0f)
			{
				return;
			}
			for (size_t s = 0; s < animals.size() && s < state.Population.size(); s++)
			{
				const AnimalDefinition& animal = animals[s];
				const float room = Room(state, animals, s, region, factors);
				float count = state.Population[s][region];
				if (count > room)
				{
					count -= (count - room) * std::min(1.0f, LeavePerDay * days);
				}
				else if (count > 0.0f)
				{
					const float rate = std::min(1.0f, animal.BirthsPerDay * factors.Life * days);
					count = std::min(room, count + count * rate * (1.0f - count / room));
				}
				const int prey = animal.PreyId >= 0 ? RowOf(animals, animal.PreyId) : -1;
				if (prey >= 0 && (size_t)prey < state.Population.size() && count > 0.0f)
				{
					float& eaten = state.Population[(size_t)prey][region];
					eaten = std::max(0.0f, eaten - count * animal.EatPerDay * days);
				}
				if (count < 0.5f && room >= 1.0f && random.Float() < animal.ArrivePerDay * days)
				{
					const int spread = std::max(0, animal.GroupMax - animal.GroupMin);
					const float group = (float)(std::max(1, animal.GroupMin) + (int)random.Below((uint32_t)spread + 1u));
					count = std::max(count, std::min(group, room));
				}
				if (count >= 1.0f && animal.SpreadPerDay > 0.0f)
				{
					const size_t next = Neighbour(state, region, random.Below(4));
					if (next < state.GetRegionCount())
					{
						const float free = Room(state, animals, s, next, factors) - state.Population[s][next];
						const float moving = std::min(count * std::min(1.0f, animal.SpreadPerDay * days), free);
						if (moving > 0.0f)
						{
							state.Population[s][next] += moving;
							count -= moving;
						}
					}
				}
				state.Population[s][region] = count < Gone ? 0.0f : count;
			}
		}

		// Measures and grows `regions` regions, going on from the last update round the map. tickDays: world days a tick;
		// each region's turn comes once in (all regions / regions) ticks, so it lives that many ticks' days at once.
		static void Update(WildlifeState& state, const std::vector<AnimalDefinition>& animals, const TileMap& map,
			const Hydrology& water, const HeightMap& heights, const ObjectLayer& objects, const std::vector<uint8_t>& cover,
			int regions, float tickDays, const Factors& factors, Random& random)
		{
			const size_t count = state.GetRegionCount();
			if (count == 0 || regions <= 0 || state.Population.size() != animals.size())
			{
				return;
			}
			const size_t batch = std::min((size_t)regions, count);
			const float days = tickDays * (float)((count + batch - 1) / batch);
			for (size_t i = 0; i < batch; i++)
			{
				const size_t region = state.NextRegion % count;
				Measure(state, animals, map, water, heights, objects, cover, region);
				Grow(state, animals, region, days, factors, random);
				state.NextRegion = (uint32_t)((region + 1) % count);
			}
			state.Revision++;
		}

		// A new world's animals: every region measured and holding StartShare of what it holds (hunters after their prey).
		static void Fill(WildlifeState& state, const std::vector<AnimalDefinition>& animals, const TileMap& map,
			const Hydrology& water, const HeightMap& heights, const ObjectLayer& objects, const std::vector<uint8_t>& cover,
			const Factors& factors)
		{
			for (size_t region = 0; region < state.GetRegionCount(); region++)
			{
				Measure(state, animals, map, water, heights, objects, cover, region);
			}
			for (int pass = 0; pass < 2; pass++)
			{
				for (size_t s = 0; s < animals.size() && s < state.Population.size(); s++)
				{
					if ((animals[s].PreyId >= 0) != (pass == 1))
					{
						continue;
					}
					for (size_t region = 0; region < state.GetRegionCount(); region++)
					{
						const float count = Room(state, animals, s, region, factors) * animals[s].StartShare;
						state.Population[s][region] = count < Gone ? 0.0f : count;
					}
				}
			}
			state.Discovered.assign(animals.size(), 0);
			for (size_t s = 0; s < animals.size() && s < state.Population.size(); s++)
			{
				state.Discovered[s] = state.Total(s) > 0.0f ? 1 : 0; // the world's own: no notices
			}
			state.Revision++;
		}

		// Marks the species with DiscoverAt or more living on the map for the first time. Returns their rows (also added to Found).
		static std::vector<int> Discover(WildlifeState& state, const std::vector<AnimalDefinition>& animals)
		{
			std::vector<int> found;
			if (state.Discovered.size() != animals.size())
			{
				state.Discovered.resize(animals.size(), 0);
			}
			for (size_t s = 0; s < animals.size() && s < state.Population.size(); s++)
			{
				if (state.Discovered[s] == 0 && state.Total(s) >= DiscoverAt)
				{
					state.Discovered[s] = 1;
					state.Found.push_back((int)s);
					found.push_back((int)s);
				}
			}
			return found;
		}

		// How many of a region's animals to show: its ShowShare (more with sight), the hibernating share out of sight in the
		// cold, at most MaxShown.
		static int Shown(const AnimalDefinition& animal, float count, float burrowed, float sight)
		{
			const float awake = animal.Hibernates ? std::max(0.0f, 1.0f - burrowed) : 1.0f;
			const int shown = (int)(count * animal.ShowShare * (1.0f + sight) * awake + 0.5f);
			return std::min(shown, animal.MaxShown);
		}

		// Per AnimalId (0 to 255) the animals living on the map, rounded down.
		static std::vector<uint32_t> Totals(const WildlifeState& state, const std::vector<AnimalDefinition>& animals)
		{
			std::vector<uint32_t> totals(256, 0);
			for (size_t s = 0; s < animals.size(); s++)
			{
				const int id = animals[s].AnimalId;
				if (id >= 0 && id < 256)
				{
					totals[(size_t)id] += (uint32_t)state.Total(s);
				}
			}
			return totals;
		}

	private:
		// The region beside `region` in one of 4 directions (0 east, 1 west, 2 south, 3 north); the region count off the map.
		static size_t Neighbour(const WildlifeState& state, size_t region, uint32_t direction)
		{
			const int rx = (int)(region % (size_t)state.RegionsX) + (direction == 0 ? 1 : (direction == 1 ? -1 : 0));
			const int ry = (int)(region / (size_t)state.RegionsX) + (direction == 2 ? 1 : (direction == 3 ? -1 : 0));
			if (rx < 0 || ry < 0 || rx >= state.RegionsX || ry >= state.RegionsY)
			{
				return state.GetRegionCount();
			}
			return (size_t)ry * (size_t)state.RegionsX + (size_t)rx;
		}
	};
}
