# Hearthlands code map

One line per game file. Engine files are in Hydro's `docs/CodeMap.md`.

## Game/src

| File | What it is |
| --- | --- |
| `GameRegistration.*` | `RegisterGame()`: the compiled game's id (`hearthlands`), then the generated data and script reflection |
| `Generated/*` | Written by Hydro's reflection generator on every build; never edit |
| `Data/TerrainTypeDefinition.h` | `TerrainTypeDefinition` (`HY_DATA` table, `Assets/Data/TerrainTypes.hytable`): a terrain type's tile id, colour, priority, whether it is a brush from the start, and its generation ranges |
| `State/TileMap.h` | `TileMap`: the world's tile ids, `Get` / `Set` / `InBounds`, a revision that moves on every change, `Replace` |
| `State/HearthlandsSession.h` | `HearthlandsSession`: the run's state (the tile map, the unlocked brush tile ids, the land mask: paintable inside the coast), registered in `GameRegistration.cpp` |
| `Rules/TerrainGenRules.h` | `TerrainGenRules`: a tile map from a seed (layered value noise for height and moisture, type ranges from data); `ShapeCoast` rounds the map off with a wavy sea rim, `AddBeach` lines it with sand, `LandOf` (inside the coast; inland lakes count as land), `StepsToSea` |
| `Rules/BrushRules.h` | `BrushRules`: the brush's maths: `RayToGround`, `MapPoint` / `OnMap` (map-centred point to map tiles), `Stroke` (joins a drag), `Stamp` (round brush around a point, never on the sea), `StartingBrushes` |
| `Rules/CameraRules.h` | `CameraRules`: the world camera's maths: `Position` (target, distance, pitch), `ClampTarget` (stays over the map), `Zoom`, `MetresPerPixel` (drag) |
| `Views/TerrainView.h` | `TerrainView`: draws the tile map on the Terrain plane (the art atlas, else a palette made in code); the tile texture is the map inside a ring of the outside tile (water), the plane `OutsideMargin` tiles wider; re-uploads when the map's revision moves |
| `Scripts/World/WorldScript.h` | `WorldScript`: the World scene's director on the Terrain entity; makes the run's map (all `BaseTileId`, or generated from `Seed`; coast by `CoastWidth`, beach by `BeachWidth`; default 512 x 512 dirt island), the land mask and starting brushes, owns the `TerrainView` |
| `Scripts/World/BrushScript.h` | `BrushScript`: the player's brush on the Terrain entity; keys 1-9 pick an unlocked type, left mouse paints the land (camera ray through the UI pointer) |
| `Scripts/World/CameraScript.h` | `CameraScript`: the world camera on the Camera entity: WASD / arrows pan, Q / E or - / = zoom, right-drag drags the ground; kept over the map |

## Other

| File | What it is |
| --- | --- |
| `Project/Hearthlands.hyproj` | The project (game id `hearthlands`, asset folder `Assets/`) |
| `Project/Assets/Data/TerrainTypes.hytable` | The terrain types: sand 0, dirt 1, grass 2, water 3, snow 4 (dirt and grass are brushes from the start) |
| `Project/Assets/Textures/TerrainAtlas.png` | `tile_blend.frag`'s art: one 16 x 16 tile per id in id order; made by `scripts/art/make_terrain_atlas.py` |
| `Project/Assets/Scenes/World.hyscene` | The start scene: camera (`CameraScript`), sun, the Terrain plane (material shader `tile_blend.frag`, `WorldScript`) |
| `Project/Assets/Shaders/tile_blend.frag` | The terrain blend material shader (Hydro material shaders; follows `scripts/prototypes/terrain_blend.py`) |
| `docs/Handoff-TerrainShader.md` | How the terrain shader and its engine support fit together |
| `Tools/Tests/GameRulesTest.cpp` | `TestGameRules`: the rule tests, run by `bash scripts/check.sh --tests` |
| `scripts/Build.bat`, `Package.bat`, `RunOxygen.bat`, `check.sh` | Wrappers that find Hydro and run its scripts on this workspace |
| `scripts/art/make_terrain_atlas.py` | Draws `TerrainAtlas.png` (Pillow, seeded; `--preview` writes an enlarged copy to `scripts/art/out/`, ignored) |
| `scripts/prototypes/terrain_blend.py` | The terrain blend shader's reference prototype (numpy, Pillow; output in `scripts/prototypes/out/`, ignored) |
| `.github/workflows/build.yml` | CI: Windows MSVC Debug and Release against `engine.lock`, plus reflection, conventions, project and rule-test checks |
