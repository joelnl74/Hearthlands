#pragma once

#include "Hydro/Scene/GameSession.h"
#include "State/HeightMap.h"
#include "State/Hydrology.h"
#include "State/ObjectLayer.h"
#include "State/Random.h"
#include "State/SeasonState.h"
#include "State/GameTimings.h"
#include "State/SettlementState.h"
#include "State/TalentState.h"
#include "State/SimulationClock.h"
#include "State/SoilState.h"
#include "State/TileMap.h"
#include "State/WildlifeState.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
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
		TalentState Talents;                // points, owned talents, reached milestones and what the talents add up to
		std::vector<uint32_t> MilestoneProgress; // per milestone row (table order): its count now (SimulationScript)
		BrushSettings Brush;
		SimulationClock Clock;
		SeasonState Season;                 // the season now, blended (SimulationScript, every frame)
		bool PointerOverHud = false;        // this frame's: the pointer is over a HUD panel, so the brush doesn't paint
		bool PointerOverMenu = false;       // this frame's: over the World Menu's button or the open menu (WorldMenuScript)
		bool PointerOnLand = false;         // this frame's: the pointer is over the land, at PointerTile (BrushScript)
		glm::vec2 PointerTile{ -1.0f };     // map tiles, fractional
		bool Painting = false;              // this frame's: the brush paints (animals near it run)
		Hydrology Water;                    // water heads and moisture (empty until SimulationScript starts it)
		ObjectLayer Objects;                // what stands on each tile (bushes, trees, rocks)
		WildlifeState Wildlife;             // the wild animals per region of the map (SimulationScript)
		SoilState Soil;                     // the soil under each tile: richness and sand (SimulationScript, BrushScript)
		std::vector<uint32_t> DryCreeks;    // the generated creeks' bed tiles: sealed until their head is dug (WorldScript)
		SettlementState Settlements;        // the peoples' settlements, come on their own (SimulationScript)
		int SelectedSettlement = 0;         // the settlement whose detail panel is open (its Id; 0 none)
		int FieldTileId = -1;               // the farmland tile (SimulationScript; -1 none)
		std::vector<uint8_t> FarmReach;     // 1 where farmland may be painted: near a settlement that farms (FarmRules::Reach)
		GameTimings Timings;               // where the frame's time goes (logged by WorldScript every few seconds)
		int RenameRequested = 0;            // a settlement to take the next name from its people's list (SimulationScript)
		bool FocusRequested = false;        // the camera moves to FocusTile once (CameraScript)
		glm::vec2 FocusTile{ 0.0f };
		glm::vec2 CameraTile{ 0.0f };       // the map tile the camera looks at, and its zoom step (CameraScript; saved)
		int CameraZoom = -1;                // -1: the camera's own starting zoom
		bool Loading = false;               // a save was just loaded: the world's scene starts again (scripts wait for it)
		std::vector<std::string> Notices;   // short news for the HUD, oldest first (HudScript shows and clears them)
		Random Rng{ 1 };                    // the simulation's dice, seeded from the world's seed
		bool ShowMoisture = false;          // the moisture overlay (M)
		bool ShowSoil = false;              // the soil overlay (F): poor to rich
		bool ShowGroundwater = false;       // the groundwater overlay (G, with the Groundwater sight talent)
		bool MountainsChanged = false;      // the land's height changed: where meltwater runs is worked out again soon
	};
}
