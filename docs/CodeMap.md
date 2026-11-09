# Hearthlands code map

One line per game file. Engine files are in Hydro's `docs/CodeMap.md`.

## Game/src

| File | What it is |
| --- | --- |
| `GameRegistration.*` | `RegisterGame()`: the compiled game's id (`hearthlands`), then the generated data and script reflection |
| `Generated/*` | Written by Hydro's reflection generator on every build; never edit |
| `Data/` | Authored content types (`HY_DATA`): terrain types, species, buildings (none yet) |
| `State/` | Run state on the game session: the tile map, populations (none yet) |
| `Rules/` | Pure simulation rules, unit-tested in `Tools/Tests/GameRulesTest.cpp` (none yet) |
| `Views/` | Presenting state: the tile map renderer, overlays (none yet) |
| `Scripts/` | Orchestration: brushes and input, the simulation tick (none yet) |

## Other

| File | What it is |
| --- | --- |
| `Project/Hearthlands.hyproj` | The project (game id `hearthlands`, asset folder `Assets/`) |
| `Tools/Tests/GameRulesTest.cpp` | `TestGameRules`: the rule tests, run by `bash scripts/check.sh --tests` |
| `scripts/Build.bat`, `Package.bat`, `RunOxygen.bat`, `check.sh` | Wrappers that find Hydro and run its scripts on this workspace |
| `scripts/prototypes/terrain_blend.py` | The terrain blend shader's reference prototype (numpy, Pillow; output in `scripts/prototypes/out/`, ignored) |
| `.github/workflows/build.yml` | CI: Windows MSVC Debug and Release against `engine.lock`, plus reflection, conventions, project and rule-test checks |
