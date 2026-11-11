#pragma once

#include "Hydro/Scene/GameSession.h"
#include "State/HeightMap.h"
#include "State/Hydrology.h"
#include "State/ObjectLayer.h"
#include "State/Random.h"
#include "State/SimulationClock.h"
#include "State/TileMap.h"

#include <cstdint>
#include <vector>

namespace Hydro
{
	// The brush the player paints with (HudScript picks it, BrushScript paints with it).
	struct BrushSettings
	{
		uint8_t TileId = 1;   // one of UnlockedTiles (BrushScript falls back to the first)
		float Radius = 1.5f;  // tiles, one of BrushRules::Radii
	};

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
		HeightMap Heights;                  // the land's height in levels per tile, like Map (sized with it by WorldScript)
		float LevelHeight = 0.2887f;        // metres a level stands (WorldScript::LevelHeight)
		std::vector<uint8_t> UnlockedTiles; // the tile ids the player can paint, ascending (empty until the world starts)
		std::vector<uint8_t> Land;          // per tile like Map's tiles: 1 inside the coast (paintable), 0 for the sea
		std::vector<uint8_t> Groundwater;   // per tile like Map's tiles: the hidden water under it (GroundwaterRules)
		float ClueClarity = 0.25f;          // how plainly the surface shows groundwater: 0 not, 1 plainly (talents raise it)
		float ClueFrom = 0.78f;             // groundwater (0 to 1) from which it shows at all (talents lower it: find more)
		BrushSettings Brush;
		SimulationClock Clock;
		bool PointerOverHud = false;        // this frame's: the pointer is over a HUD panel, so the brush doesn't paint
		Hydrology Water;                    // water heads and moisture (empty until SimulationScript starts it)
		ObjectLayer Objects;                // what stands on each tile (bushes, trees, rocks)
		Random Rng{ 1 };                    // the simulation's dice, seeded from the world's seed
		bool ShowMoisture = false;          // the moisture overlay (M)
		bool MountainsChanged = false;      // the land's height changed: where meltwater runs is worked out again soon
	};
}
