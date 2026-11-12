#pragma once

#include <cstdint>
#include <vector>

namespace Hydro
{
	// The wild animals as numbers: the map in square regions, per species (the Animals table's rows, in table order) how
	// many live in each region and how many its land holds. Run state on the session, changed by WildlifeRules.
	struct WildlifeState
	{
		int RegionSize = 16;    // tiles across a region
		int RegionsX = 0;       // regions across and down (the map's edge regions may be smaller)
		int RegionsY = 0;
		std::vector<std::vector<float>> Population; // [species][region]: animals living there
		std::vector<std::vector<float>> Capacity;   // [species][region]: what its land holds (before seasons, talents, prey)
		std::vector<uint8_t> Discovered;            // [species]: 1 once any have lived on the map (WildlifeRules::Discover)
		std::vector<int> Found;                     // species rows discovered after the world began, in order (for notices)
		uint32_t NextRegion = 0;                    // where the next update goes on
		uint64_t Revision = 0;                      // moves on every update

		size_t GetRegionCount() const
		{
			return (size_t)RegionsX * (size_t)RegionsY;
		}

		// The region a tile is in; the region count past the last region (or for negative tiles).
		size_t RegionOf(int x, int y) const
		{
			if (x < 0 || y < 0 || RegionSize <= 0 || x / RegionSize >= RegionsX || y / RegionSize >= RegionsY)
			{
				return GetRegionCount();
			}
			return (size_t)(y / RegionSize) * (size_t)RegionsX + (size_t)(x / RegionSize);
		}

		bool IsDiscovered(size_t species) const
		{
			return species < Discovered.size() && Discovered[species] != 0;
		}

		float Get(size_t species, size_t region) const
		{
			return species < Population.size() && region < Population[species].size() ? Population[species][region] : 0.0f;
		}

		float Total(size_t species) const
		{
			float total = 0.0f;
			if (species < Population.size())
			{
				for (float count : Population[species])
				{
					total += count;
				}
			}
			return total;
		}
	};
}
