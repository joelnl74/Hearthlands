# Hearthlands code map

One line per game file. Engine files are in Hydro's `docs/CodeMap.md`.

## Game/src

| File | What it is |
| --- | --- |
| `GameRegistration.*` | `RegisterGame()`: the compiled game's id (`hearthlands`), then the generated data and script reflection |
| `Generated/*` | Written by Hydro's reflection generator on every build; never edit |
| `Data/TerrainTypeDefinition.h` | `TerrainTypeDefinition` (`HY_DATA` table, `Assets/Data/TerrainTypes.hytable`): a terrain type's tile id, `TerrainRole` (Land, Water, Riverbed, Rock, Snow), brush name, colour, priority (0 to 15), unlocks (from the start, or after `UnlockCount` tiles of `UnlockTileId`), `Shelter` (moisture a tile walled in by it keeps), `FromLevel` / `BelowTileId` (rock and snow from a height, and what they turn back into), `CliffColor` (its walls) and generation ranges |
| `Data/ObjectDefinition.h` | `ObjectDefinition` (`HY_DATA` table, `Assets/Data/Objects.hytable`): bushes, trees, reeds, rocks per growth stage: sprite, `Variants` (a tile picks one) and size, `ObjectKind`, where it lives (tile, moisture, neighbours, `MinLevel` / `MaxLevel`), sprouting and spreading (`ChancePerLevel`: likelier higher up), spacing, growing into the next stage, start density |
| `Data/TerrainRuleDefinition.h` | `TerrainRuleDefinition` (`HY_DATA` table, `Assets/Data/TerrainRules.hytable`): land changing by itself: from, to, moisture range, neighbours, chance |
| `State/TileMap.h` | `TileMap`: the world's tile ids, `Get` / `Set` / `InBounds`, a revision that moves on every change, `Replace`, counts per id (`GetCount`), a change log (`TakeChanges`) |
| `State/HeightMap.h` | `HeightMap`: the land's level per tile (0 the ground, 0 off the map), `Get` / `Set`, a revision, `Replace`, a change log (`TakeChanges`) |
| `State/Hydrology.h` | `Hydrology`: per tile water head, source flag, `Carried` moisture (spread from fresh water), `Rain` (from the height), `Melt` (meltwater running there, 255 at the snow), `Groundwater` and `Moisture` (what the land has: carried or rain, more where wet grass walls a tile in), the active tiles for the next tick |
| `State/ObjectLayer.h` | `ObjectLayer`: the object id on each tile (0 none), a revision, a count |
| `State/Random.h` | `Random`: the game's seeded generator (xorshift64*), passed into rules |
| `State/HearthlandsSession.h` | `HearthlandsSession`: the run's state (the tile map, its heights and `LevelHeight`, the groundwater and how plainly it shows (`ClueClarity`, `ClueFrom`), the unlocked brush tile ids, the land mask: paintable inside the coast, the brush (`BrushSettings`), the clock, whether the pointer is over the HUD, whether the meltwater needs working out again), registered in `GameRegistration.cpp` |
| `State/SimulationClock.h` | `SimulationClock`: speed (0 = paused, 1, 3, 10), world seconds, ticks run; `GetDay` (60 world seconds a day) |
| `Rules/TerrainGenRules.h` | `TerrainGenRules`: a tile map from a seed (layered value noise for height and moisture, type ranges from data), `Levels` (heights from the same noise); `ShapeCoast` rounds the map off with a wavy sea rim, `AddBeach` lines it with sand, `LandOf` (inside the coast; inland lakes count as land), `StepsToSea` |
| `Rules/BrushRules.h` | `BrushRules`: the brush's maths: `RayToGround`, `MapPoint` / `OnMap` (map-centred point to map tiles), `Stroke` (joins a drag), `Stamp` (round brush around a point, never on the sea), `Radii` / `StepRadius` (sizes), `BrushTile`, `StartingBrushes` |
| `Rules/WaterRules.h` | `WaterRules`: water heads (strongest neighbour, or `MeltHead` next to a `Melt` tile, less a loss, smaller in wide channels), riverbed filling and drying (dug riverbed over groundwater springs; `FillHead`: more by a wide channel's banks, so river ends are rounded), moisture from fresh water (fading in curves), snowmelt (`MeltMoisture`), groundwater, rain (`RainPerLevel`) and shelter (a tile with 5 of 8 wet sheltering neighbours keeps their mean carried moisture plus their `Shelter`, never passed on); `Start`, `Step` (one tile per tick), `FlowHead`, `TargetMoisture`, `ShelteredMoisture`, `UpdateRain`, `UpdateMelt` |
| `Rules/GroundwaterRules.h` | `GroundwaterRules`: the hidden water per tile (`Generate`, its own noise in patches), `Springs` / `SpringHead` (a dug riverbed over it becomes a source), `Wetness` (damp above it), `Clue` (how plainly the surface shows it: clarity and threshold, which talents will move) |
| `Rules/HeightRules.h` | `HeightRules`: `Bump` (a Raise / Dig stroke's dome), `Stroke` / `Move` (a tile moves at most its bump per press), `Slump` (slopes within `MaxStep`; water, riverbed and sea hold), `LevelHeight` (metres a level for some art pixels on screen), `RayToLand` (where a ray meets the raised land) |
| `Rules/MountainRules.h` | `MountainRules`: rock and snow from the height; `LevelTile`, `SnowLine` (ragged per tile), `Settle` / `SettleTiles` (to rock or snow and back, rock to bare dirt), `Meltwater` (how much runs down the raised land within `MeltReach` of snow, fading; `MeltAt`) |
| `Rules/GrowthRules.h` | `GrowthRules`: the terrain rules on a random sample of land tiles (`Matches`, `Step`), `NewUnlocks`, `TicksDue` |
| `Rules/ObjectRules.h` | `ObjectRules`: the object layer's life: `Fits`, `Scatter` (start density), `Step` (sprout, spread, grow, die on random samples), `RemoveUnfit` (changed tiles), `Crowded` (spacing by kind) |
| `Rules/CameraRules.h` | `CameraRules`: the isometric orthographic camera's maths: `Rotation` (yaw, elevation), `Position`, `OrthographicSize` (whole pixel scales, `PixelScales`), `StepZoom`, `GroundDelta` (screen to ground for pan and drag), `ClampTarget` |
| `Views/HudView.h` | `HudView`: the HUD built in code on the HUD entity (a 1920 x 1080 canvas): brush bar (a button per unlocked type, size - / +), clock bar (day, pause, 1x / 3x / 10x), a controls line; `ContainsPointer` |
| `Views/BrushCursorView.h` | `BrushCursorView`: the brush's ring on the ground (a flat overlay sprite, its ring texture a memory asset) |
| `Views/ReliefView.h` | `ReliefView`: the raised land as stepped blocks: a mesh (memory asset) per 32 x 32 chunk of tops in the Terrain plane's material and cliff walls in `CliffColor`, banded per level; rebuilt a few chunks at a time when heights or tiles change |
| `Views/ObjectsView.h` | `ObjectsView`: the object layer as upright sprites at their tile's height; a pool (at most 6000) under an "Objects" entity assigned to the tiles the camera sees, refreshed at most 10 times a second; each tile keeps one of its object's variants (by its hash) |
| `Views/TerrainView.h` | `TerrainView`: draws the tile map on the Terrain plane (the art atlas, else a palette made in code); the tile texture (id, priority and level, role and groundwater clue, moisture) is the map inside a ring of the outside tile (water), the plane `OutsideMargin` tiles wider; re-uploads what changed (tiles, moisture) at most 15 times a second |
| `Scripts/World/WorldScript.h` | `WorldScript`: the World scene's director on the Terrain entity; makes the run's map (all `BaseTileId`, or generated from `Seed` with hills from `HillsFrom` / `LevelsPerHeight`; coast by `CoastWidth`, beach by `BeachWidth`; default 512 x 512 dirt island), its heights, the groundwater (`GroundwaterScale`; clues from `ClueClarity` / `ClueFrom`), the land mask and starting brushes, owns the `TerrainView` (with the moisture overlay), the `ReliefView` and the `ObjectsView` |
| `Scripts/World/BrushScript.h` | `BrushScript`: the player's brush on the Terrain entity: left mouse paints the session's brush on the land (camera ray through the UI pointer onto the raised land), not through the HUD; Raise (the rock type) lifts a dome per press, Dig lowers raised land and digs riverbed on the ground, slopes slump (`MaxStep`) and rock and snow settle at once; owns the `BrushCursorView` |
| `Scripts/World/SimulationScript.h` | `SimulationScript`: on the Terrain entity; advances the clock, runs the ticks due (changed heights: rock, snow and rain; water; meltwater and its moisture at most every other tick; growth; objects by height), unlocks brushes |
| `Scripts/World/HudScript.h` | `HudScript`: on the HUD entity; owns the `HudView`; buttons and keys (1-9, [ ], P, M) set the session's brush, clock speed and moisture overlay; a notice for each new brush |
| `Scripts/World/CameraScript.h` | `CameraScript`: the isometric orthographic camera on the Camera entity (yaw 45, elevation 30): WASD / arrows pan, Q / E or - / = zoom in whole pixel steps, right-drag drags the ground; kept over the map |

## Other

| File | What it is |
| --- | --- |
| `Project/Hearthlands.hyproj` | The project (game id `hearthlands`, asset folder `Assets/`) |
| `Project/Assets/Data/TerrainTypes.hytable` | The terrain types: sand 0, dirt 1, grass 2 (unlocks at 400 grass, shelter 0.15), water 3, snow 4 (from level 9, back to rock), riverbed 5 (the Dig brush), rock 6 (the Raise brush, unlocks at 1,500 grass; from level 4, back to dirt); dirt and Dig from the start; rock and snow follow the height, never painted; a cliff colour each |
| `Project/Assets/Data/Objects.hytable` | The objects: bushes (plain, berry, flowering), reeds, oak / birch / pine in stages, rocks and boulders; on and by the mountains: foothill pines (grass next to rock), rock pines (high grass, levels 2 and 3), scree and crags; oaks up to level 2, birches to 3, pines likelier higher up; grass tufts, ferns, willows by water, dead trees and bushes on dry dirt (they rot away as it gets wet) |
| `Project/Assets/Sprites/Objects/` | Object sprites, 32 pixels a tile (PixelLab, picked from `Art/Objects`): `<Object>.png` is the row's `Sprite`, `<Object>_02.png` and on its `Variants`; `Reeds.png` is still a 24-pixel placeholder |
| `Project/Assets/Data/TerrainRules.hytable` | The terrain rules: grass spreads on moist dirt, sprouts on wet dirt, dries back to dirt |
| `Project/Assets/Fonts/OpenSans.fnt` (+ `_0.png`) | The HUD's font: Open Sans SemiBold 26 px, smooth (page filtered Linear), Apache 2.0 (`OpenSans-LICENSE.txt`); made by `scripts/fonts/make_font.py` |
| `Project/Assets/Textures/TerrainAtlas.png` | `tile_blend.frag`'s art: 32 x 32 tiles, a row of 8 variants per id, rows 16 + id for the wet look (moist land, shallow water); made by `scripts/art/make_terrain_atlas.py` from `Art/Terrain` |
| `Project/Assets/Scenes/World.hyscene` | The start scene: camera (`CameraScript`), sun, HUD (`HudScript`), the Terrain plane (material shader `tile_blend.frag`, `WorldScript`) |
| `Project/Assets/Shaders/tile_blend.frag` | The terrain blend material shader (Hydro material shaders; follows `scripts/prototypes/terrain_blend.py`) |
| `docs/Handoff-TerrainShader.md` | How the terrain shader and its engine support fit together |
| `Tools/Tests/GameRulesTest.cpp` | `TestGameRules`: the rule tests, run by `bash scripts/check.sh --tests` |
| `scripts/Build.bat`, `Package.bat`, `RunOxygen.bat`, `check.sh` | Wrappers that find Hydro and run its scripts on this workspace |
| `scripts/fonts/make_font.py` | A smooth BMFont (ASCII, one page) from a TTF with Pillow; default Hydro's Open Sans SemiBold 26 px |
| `Art/Terrain/`, `Art/Objects/` | Source art (PixelLab, picked and renamed): ground textures and object sprites; `Art/Terrain/Temp/` holds temporary cliff, lip, foam, shallow water, wet bank and rock ground pieces; `Source/` the full sheets |
| `scripts/art/make_terrain_atlas.py` | Builds `TerrainAtlas.png` from `Art/Terrain` (which texture how often per row; variants share their row's main colour; `--preview` writes an enlarged copy to `scripts/art/out/`, ignored) |
| `scripts/art/make_temp_terrain_art.py` | Makes `Art/Terrain/Temp/` from the palette and `Art/Terrain` (`--preview` adds a flat mock-up) |
| `scripts/prototypes/terrain_blend.py` | The terrain blend shader's reference prototype (numpy, Pillow; `terrain_art.png` with the atlas: variants, wet ground, shallow water, foam, a raised block blending only within its level; `terrain_groundwater.png`: groundwater clues at the start and revealed; output in `scripts/prototypes/out/`, ignored) |
| `.github/workflows/build.yml` | CI: Windows MSVC Debug and Release against `engine.lock`, plus reflection, conventions, project and rule-test checks |
