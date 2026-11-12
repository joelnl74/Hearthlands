#pragma once

#include <cstdint>
#include <vector>

namespace Hydro
{
	// The soil under each tile, like TileMap's tiles: how rich it is and how sandy (0 to 255 each). Run state on the
	// session, changed by SoilRules (life and water over time) and by the brush (painting a type mixes its soil in).
	struct SoilState
	{
		std::vector<uint8_t> Fertility;
		std::vector<uint8_t> Sand;
		uint64_t Revision = 0; // moves on every change
	};
}
