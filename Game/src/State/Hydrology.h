#pragma once

#include <cstdint>
#include <vector>

namespace Hydro
{
	// The world's water and moisture, per tile like TileMap's tiles: run state on the session, changed by WaterRules.
	struct Hydrology
	{
		uint8_t WaterTileId = 3;       // the types with the Water and Riverbed roles (set by WaterRules::Start)
		uint8_t BedTileId = 5;
		std::vector<float> Head;       // water strength on Water tiles (0 elsewhere); drops along a river, dries at 0
		std::vector<uint8_t> Source;   // 1: holds its head and never dries (the sea and generated lakes)
		std::vector<uint8_t> Carried;  // moisture spread from fresh water: 0 dry to 255 next to it
		std::vector<uint8_t> Moisture; // what the land has: Carried, more where wet grass walls a tile in or rain falls (WaterRules)
		std::vector<uint8_t> Rain;     // moisture the land's height alone gives it (highland rain); empty = none
		std::vector<uint8_t> Groundwater; // the hidden water under each tile (GroundwaterRules); empty = none
		std::vector<uint8_t> Melt;     // meltwater running there, 255 at the snow (MountainRules::Meltwater); empty = none
		std::vector<uint32_t> Active;  // tiles to look at on the next tick
		std::vector<uint8_t> Queued;   // 1 while a tile is in Active

		void Activate(uint32_t index)
		{
			if (index < Queued.size() && Queued[index] == 0)
			{
				Queued[index] = 1;
				Active.push_back(index);
			}
		}
	};
}
