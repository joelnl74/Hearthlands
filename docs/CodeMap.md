# Hearthlands code map

One line per game file. Engine files are in Hydro's `docs/CodeMap.md`.

## Game/src

| File | What it is |
| --- | --- |
| `GameRegistration.*` | `RegisterGame()`: the compiled game's id (`hearthlands`), then the generated data and script reflection |
| `Generated/*` | Written by Hydro's reflection generator on every build; never edit |
| `Data/TerrainTypeDefinition.h` | `TerrainTypeDefinition` (`HY_DATA` table, `Assets/Data/TerrainTypes.hytable`): a terrain type's tile id, `TerrainRole` (Land, Water, Riverbed), brush name, colour, priority, unlocks (from the start, or after `UnlockCount` tiles of `UnlockTileId`) and generation ranges |
| `Data/ObjectDefinition.h` | `ObjectDefinition` (`HY_DATA` table, `Assets/Data/Objects.hytable`): bushes, trees, reeds, rocks per growth stage: sprite and size, `ObjectKind`, where it lives (tile, moisture, neighbours), sprouting, spacing, spreading, growing into the next stage, start density |
| `Data/TerrainRuleDefinition.h` | `TerrainRuleDefinition` (`HY_DATA` table, `Assets/Data/TerrainRules.hytable`): land changing by itself: from, to, moisture range, neighbours, chance |
| `State/TileMap.h` | `TileMap`: the world's tile ids, `Get` / `Set` / `InBounds`, a revision that moves on every change, `Replace`, counts per id (`GetCount`), a change log (`TakeChanges`) |
| `State/Hydrology.h` | `Hydrology`: per tile water head, source flag and moisture, the active tiles for the next tick |
| `State/ObjectLayer.h` | `ObjectLayer`: the object id on each tile (0 none), a revision, a count |
| `State/Random.h` | `Random`: the game's seeded generator (xorshift64*), passed into rules |
| `State/HearthlandsSession.h` | `HearthlandsSession`: the run's state (the tile map, the unlocked brush tile ids, the land mask: paintable inside the coast, the brush (`BrushSettings`), the clock, whether the pointer is over the HUD), registered in `GameRegistration.cpp` |
| `State/SimulationClock.h` | `SimulationClock`: speed (0 = paused, 1, 3, 10), world seconds, ticks run; `GetDay` (60 world seconds a day) |
| `Rules/TerrainGenRules.h` | `TerrainGenRules`: a tile map from a seed (layered value noise for height and moisture, type ranges from data); `ShapeCoast` rounds the map off with a wavy sea rim, `AddBeach` lines it with sand, `LandOf` (inside the coast; inland lakes count as land), `StepsToSea` |
| `Rules/BrushRules.h` | `BrushRules`: the brush's maths: `RayToGround`, `MapPoint` / `OnMap` (map-centred point to map tiles), `Stroke` (joins a drag), `Stamp` (round brush around a point, never on the sea), `Radii` / `StepRadius` (sizes), `BrushTile`, `StartingBrushes` |
| `Rules/WaterRules.h` | `WaterRules`: water heads (strongest neighbour less a loss, smaller in wide channels), riverbed filling and drying, moisture from fresh water; `Start`, `Step` (one tile per tick), `FlowHead`, `TargetMoisture` |
| `Rules/GrowthRules.h` | `GrowthRules`: the terrain rules on a random sample of land tiles (`Matches`, `Step`), `NewUnlocks`, `TicksDue` |
| `Rules/ObjectRules.h` | `ObjectRules`: the object layer's life: `Fits`, `Scatter` (start density), `Step` (sprout, spread, grow, die on random samples), `RemoveUnfit` (changed tiles), `Crowded` (spacing by kind) |
| `Rules/CameraRules.h` | `CameraRules`: the isometric orthographic camera's maths: `Rotation` (yaw, elevation), `Position`, `OrthographicSize` (whole pixel scales, `PixelScales`), `StepZoom`, `GroundDelta` (screen to ground for pan and drag), `ClampTarget` |
| `Views/HudView.h` | `HudView`: the HUD built in code on the HUD entity (a 1920 x 1080 canvas): brush bar (a button per unlocked type, size - / +), clock bar (day, pause, 1x / 3x / 10x), a controls line; `ContainsPointer` |
| `Views/BrushCursorView.h` | `BrushCursorView`: the brush's ring on the ground (a flat overlay sprite, its ring texture a memory asset) |
| `Views/ObjectsView.h` | `ObjectsView`: the object layer as upright sprites; a pool (at most 6000) under an "Objects" entity assigned to the tiles the camera sees, refreshed at most 10 times a second |
| `Views/TerrainView.h` | `TerrainView`: draws the tile map on the Terrain plane (the art atlas, else a palette made in code); the tile texture is the map inside a ring of the outside tile (water), the plane `OutsideMargin` tiles wider; re-uploads when the map's revision moves |
| `Scripts/World/WorldScript.h` | `WorldScript`: the World scene's director on the Terrain entity; makes the run's map (all `BaseTileId`, or generated from `Seed`; coast by `CoastWidth`, beach by `BeachWidth`; default 512 x 512 dirt island), the land mask and starting brushes, owns the `TerrainView` (with the moisture overlay) and the `ObjectsView` |
| `Scripts/World/BrushScript.h` | `BrushScript`: the player's brush on the Terrain entity: left mouse paints the session's brush on the land (camera ray through the UI pointer), not through the HUD; owns the `BrushCursorView` |
| `Scripts/World/SimulationScript.h` | `SimulationScript`: on the Terrain entity; advances the clock, runs the ticks due (water, growth, objects), unlocks brushes |
| `Scripts/World/HudScript.h` | `HudScript`: on the HUD entity; owns the `HudView`; buttons and keys (1-9, [ ], P, M) set the session's brush, clock speed and moisture overlay; a notice for each new brush |
| `Scripts/World/CameraScript.h` | `CameraScript`: the isometric orthographic camera on the Camera entity (yaw 45, elevation 30): WASD / arrows pan, Q / E or - / = zoom in whole pixel steps, right-drag drags the ground; kept over the map |

## Other

| File | What it is |
| --- | --- |
| `Project/Hearthlands.hyproj` | The project (game id `hearthlands`, asset folder `Assets/`) |
| `Project/Assets/Data/TerrainTypes.hytable` | The terrain types: sand 0, dirt 1, grass 2 (unlocks at 400 grass), water 3, snow 4, riverbed 5 (the Dig brush); dirt and Dig from the start |
| `Project/Assets/Data/Objects.hytable` | The objects: bushes (plain, berry, flowering), reeds, oak / birch / pine in stages, rocks and boulders |
| `Project/Assets/Sprites/Objects/` | Placeholder object sprites (cleaned from a ChatGPT sheet: 4.0x grid, shared palette), 24 pixels a tile |
| `Project/Assets/Data/TerrainRules.hytable` | The terrain rules: grass spreads on moist dirt, sprouts on wet dirt, dries back to dirt |
| `Project/Assets/Fonts/OpenSans.fnt` (+ `_0.png`) | The HUD's font: Open Sans SemiBold 26 px, smooth (page filtered Linear), Apache 2.0 (`OpenSans-LICENSE.txt`); made by `scripts/fonts/make_font.py` |
| `Project/Assets/Textures/TerrainAtlas.png` | `tile_blend.frag`'s art: one 16 x 16 tile per id in id order; made by `scripts/art/make_terrain_atlas.py` |
| `Project/Assets/Scenes/World.hyscene` | The start scene: camera (`CameraScript`), sun, HUD (`HudScript`), the Terrain plane (material shader `tile_blend.frag`, `WorldScript`) |
| `Project/Assets/Shaders/tile_blend.frag` | The terrain blend material shader (Hydro material shaders; follows `scripts/prototypes/terrain_blend.py`) |
| `docs/Handoff-TerrainShader.md` | How the terrain shader and its engine support fit together |
| `Tools/Tests/GameRulesTest.cpp` | `TestGameRules`: the rule tests, run by `bash scripts/check.sh --tests` |
| `scripts/Build.bat`, `Package.bat`, `RunOxygen.bat`, `check.sh` | Wrappers that find Hydro and run its scripts on this workspace |
| `scripts/fonts/make_font.py` | A smooth BMFont (ASCII, one page) from a TTF with Pillow; default Hydro's Open Sans SemiBold 26 px |
| `scripts/art/make_terrain_atlas.py` | Draws `TerrainAtlas.png` (Pillow, seeded; `--preview` writes an enlarged copy to `scripts/art/out/`, ignored) |
| `scripts/prototypes/terrain_blend.py` | The terrain blend shader's reference prototype (numpy, Pillow; output in `scripts/prototypes/out/`, ignored) |
| `.github/workflows/build.yml` | CI: Windows MSVC Debug and Release against `engine.lock`, plus reflection, conventions, project and rule-test checks |
