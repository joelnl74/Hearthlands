# Hearthlands: notes for Claude

Hearthlands is a world-shaping simulation built with Hydro, a workspace of its own (`hydro-workspace.json`, game id `hearthlands`): its game code (`Game/src`, the `HearthlandsGame` library), its project (`Project/Hearthlands.hyproj` with `Assets/`), its tools and prototypes (`scripts/`), docs (`docs/`: the design in `docs/Concept.md`, game code map in `docs/CodeMap.md`) and rule tests (`Tools/Tests/`). The engine (Hydro, Oxygen, Runtime) and its rules are in the Hydro repository: read its `CLAUDE.md` and `docs/` first, and follow the `hydro-engine-architecture` skill for game code too.

## Building and checking

- `scripts\Build.bat [Debug|Release|Dist]` builds this workspace (Hydro is `HYDRO_DIR`, else `..\Hydro` next to this repository); solution `build\Hydro.sln`, binaries in `bin\`. `scripts\RunOxygen.bat` opens the project in this workspace's Oxygen.
- `scripts\Package.bat [Release|Distribution]` makes `Builds\Hearthlands-<Kind>\` (also Oxygen's File > Build).
- `bash scripts/check.sh [files...]`: Hydro's g++ syntax check, conventions and project validation on this workspace; `bash scripts/check.sh --tests`: the data tests with this game's rule tests.

## Game code

- The world is a grid of tile ids (State), changed by pure rules (Rules) and drawn by one blend shader (see `docs/Concept.md`). Tiles are never entities; settlers, animals and buildings may be.
- The simulation runs at a fixed tick from rules that take the state, data and a `Random&` and return results: unit-test every rule in `Tools/Tests/GameRulesTest.cpp`.
- Terrain types, species and what they build or change are data (`HY_DATA` tables in `Project/Assets/Data/`), not code.
- `scripts/prototypes/terrain_blend.py` is the reference for the terrain blend shader; keep them in step.

## Working from a cloud session

- **Reading**: clone `https://github.com/joelnl74/Hydro` and `https://github.com/joelnl74/Hearthlands` side by side (`Hydro/`, `Hearthlands/`). They only have what is pushed; ask the user what is uncommitted.
- **Checking**: `bash scripts/check.sh [files...]` and `bash scripts/check.sh --tests` (Hydro next to this repository, or `HYDRO_DIR`). Not a build: say the change still needs an MSVC Debug build (`scripts\Build.bat Debug`).
- **Engine changes** go to the Hydro repository first; then `engine.lock` here is set to that Hydro commit, in the same game commit that needs it.
- **Pushing**: never commit or push without an explicit yes; scratch branches are `claude/<topic>`; never push to `main`.
