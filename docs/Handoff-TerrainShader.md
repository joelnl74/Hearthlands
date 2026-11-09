# Hearthlands: terrain shader handoff

State as of Nov 9, 2026, 12:45 JST. For whoever draws the tile map next (milestone 1, step 4 in `docs/Concept.md`). Read
`CLAUDE.md`, `docs/Concept.md` and Hydro's `CLAUDE.md` first.

## Engine side: done, on a local Hydro branch

Hydro branch **`materials/custom-fragment-shader`** (on the user's machine, based on Hydro `main` `36aee63`). The changes
are in its working tree, **not committed or pushed yet**. When it is committed, point `engine.lock` here at that commit
(or build with `HYDRO_DIR` pointing at the checkout) in the same Hearthlands commit that uses it.

What it adds (details in Hydro's `docs/Architecture.md`, "Material shaders", "Memory assets", "Shader assets"):

| Piece | What it does |
| --- | --- |
| `AssetType::Shader` | A `.frag` in the project's asset folder is an asset (handle in a `.hymeta`, like an image). The Project window shows it; the material inspector has a **Shader** field |
| `MaterialData::Shader` | A material (a `.hymat` or a mesh slot's inline settings) can name that shader. Renderer3D compiles it with the engine's `mesh.vert` and gives it the material's usual set 1. Saved as `ShaderHandle` |
| `mesh_material.glsl` | The contract a material shader includes: `mesh.vert`'s outputs (`fragUV`, `fragColor`, `fragNormal`, `entityID`, `fragWorldPos`), set 1 (`material.tint`, `.emission`, `.tiling`, `.alphaCutoff`, `.normalStrength`; `albedoTexture`, `emissiveTexture`, `normalTexture`) and the outputs (`outColor`, `outEntityID`) |
| Includes | `#include "x.glsl"` is looked up next to the shader, then in Hydro's shader folder, so `lighting.glsl`, `surface_derivatives.glsl` and `mesh_material.glsl` just work |
| `AssetManager::AddMemoryAsset(Ref<Asset>)` | Gives a texture made in code a handle a material can name. `RemoveMemoryAsset(handle)` when done. Never saved |
| Fallback and reload | A shader that doesn't compile is logged and its meshes are drawn with the Standard shader until the file is saved again. Hot reload works like for engine shaders |

Checked on Linux: g++ syntax check of the changed engine and editor files, glslang on every engine shader and on
`tile_blend.frag`, the conventions lint. **Not checked:** an MSVC build (`scripts\Build.bat Debug` in Hydro) and running
it on the GPU. Expect to fix small things on the first Windows build.

Limits (Hydro's Known gaps): fragment stage only (always `mesh.vert`); the Standard material's fixed set 1 (one uniform
block, three textures), so no shader parameters of its own; no own blend, depth or cull state; no extra passes; meshes
only (no sprites); shadows use the Standard caster. `Texture2D::SetData` waits for the GPU (fine for brush strokes,
not for re-uploading the whole map every frame).

## The shader: `Project/Assets/Shaders/tile_blend.frag` (written, not committed)

Follows `scripts/prototypes/terrain_blend.py` (5 x 5 tiles, Gaussian weights with sigma 0.75, 2 x 2 clustered noise,
priority only breaks exact ties). Lit, shadowed and fogged like the Standard material; light steps with the art pixels
when the view's pixel snap is on. Compiles with glslang; a numpy emulation of it on a small map looks like the
prototype's "smooth + noise" panel. TileMap row 0 is the top, the plane's +Y edge: the Plane's UV v points down
(`MeshFactory` flips it for images), so the shader uses `-fragUV.y` (fixed Nov 9, 2026; the brush was mirrored before).

How it reads the material:

| Field | Holds |
| --- | --- |
| Albedo | The tile atlas, sRGB: one square column per tile id, in id order. Until there is art, make it in code: `TypeCount x 1` texels from `TerrainTypeDefinition::Color` (flat colours) |
| Emissive | The tile map, made in code: RGBA (linear, `ImageFormat::RGBA`), one texel per tile, rows as `TileMap` stores them; R = tile id, G = the type's `Priority` |
| Tiling | x = art pixels per tile for the blend (start with 16), y = edge noise (start with 0.18) |
| Tint | Multiplies the colour (white) |
| Filter | Nearest |

Up to 16 tile ids (`MaxTypes` in the shader). The mesh is a built-in **Plane** whose `Size` is the map in tiles
(1 tile = 1 m); its UVs are in metres from its centre, which the shader turns into tile coordinates.

## Next steps

1. **Commit the Hydro branch** (the user's call), then set `engine.lock` to it. Build Hydro Debug on Windows and fix
   anything MSVC finds.
2. **Scene setup** (`Project/Assets/Scenes/World.hyscene`, then the start scene): render features with shared depth,
   `Meshes` on and forward lighting (Hydro's 3D preset), a camera looking down at the map, a sun. One entity
   `Terrain` with a `MeshComponent`: `Shape` Plane, `Size` = (width, height, 1), `CastShadows` off, the inline material's
   **Shader** = `tile_blend.frag`, Tiling (16, 0.18), Filter Nearest. Leave Albedo and Emissive empty: the view fills them.
3. **`Views/TerrainView.h`** (a plain view class owned by a script, per the Scripting plan): `Bind(Entity terrain)`,
   `Present(map, terrain types)` (the `TerrainTypes.hytable` rows), `Release()`.
   - First `Present`: make the palette texture (`ImageFormat::SRGB`, `TypeCount x 1`, colour per tile id) and the tile
     texture (`ImageFormat::RGBA`, width x height, pixel = `id | priority << 8 | 0xff << 24`), each through
     `CreateRef<Texture2D>(specification, pixels)` and `AssetManager::AddMemoryAsset`. Set the slot's
     `Inline.Albedo` / `Inline.Emissive` to the two handles and the plane's `Size` to the map's.
   - Later calls: when `TileMap::GetRevision()` moved, upload only the changed rectangle with `Texture2D::SetData`
     (the brush knows it; a size change or `Replace` re-makes the texture and its handle).
   - `Release()` calls `RemoveMemoryAsset` on both handles; the script calls it in `OnDestroy`.
4. **`Scripts/WorldScript`** (on the Terrain entity or a director): generates the map in `OnCreate` with
   `TerrainGenRules` into `HearthlandsSession::Get().Map`, owns the `TerrainView`, calls `Present` in `OnUpdate`.
   The shader asset is a script property (`Asset(Shader)` now works in `HY_PROPERTY`) only if the script sets the
   material itself; with the material set in the scene (step 2) it isn't needed.
5. **Brush** (milestone 1, step 5): ray from the mouse to the z = 0 plane, tile = floor(hit + size / 2) with Y flipped
   to the row (`row = height - 1 - y`), `TileMap::Set`, mark the dirty rectangle for the view.
6. Add the new files to `docs/CodeMap.md` (`Views/TerrainView.h`, `Scripts/WorldScript.*`,
   `Project/Assets/Shaders/tile_blend.frag`), and a Concept note that the shader exists.

## Gotchas

- The material is set up at runtime (memory handles). Don't save the scene in Play, and don't run the view outside Play:
  a saved scene would keep handles that don't exist next time (the inspector shows "(made at runtime)").
- The view only shows the terrain in Play; in the editor the plane draws the shader with white textures (all id 0).
- Diagonal one-tile lines still pinch into beads (`docs/Concept.md`): priority only settles exact ties. If water must
  stay joined, have generation and the brush add a tile at diagonal steps, or give water a bigger weight in the shader.
- Shader errors are in Oxygen's console; the plane then draws Standard (white) until the file compiles.
- Another session was working on milestone steps 1 to 3 at the same time (`TileMap`, `TerrainGenRules`,
  `TerrainTypeDefinition`, uncommitted on `main`). This handoff only added `docs/Handoff-TerrainShader.md` and
  `Project/Assets/Shaders/tile_blend.frag`; nothing else in this repository was touched.
- Never commit or push without the user's yes; branches are `claude/<topic>`, never `main`.
