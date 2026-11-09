#pragma once

#include "Hydro/Scene/GameSession.h"
#include "State/TileMap.h"

#include <cstdint>
#include <vector>

namespace Hydro
{
	// Everything one run of the game keeps across its scenes. Made when the game starts and destroyed when it ends, so a new
	// run starts fresh without resetting anything by hand.
	class HearthlandsSession : public GameSession
	{
	public:
		static HearthlandsSession& Get()
		{
			return GameSession::Get<HearthlandsSession>();
		}

		TileMap Map;                        // the world's terrain tile ids
		std::vector<uint8_t> UnlockedTiles; // the tile ids the player can paint, ascending (empty until the world starts)
		std::vector<uint8_t> Land;          // per tile like Map's tiles: 1 inside the coast (paintable), 0 for the sea
	};
}
