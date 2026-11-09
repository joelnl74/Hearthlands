# Hearthlands: concept

Initial concept (Nov 9, 2026). A starting point to grow from, not a spec.

## Pitch

A small simulation where you shape the land and peoples and creatures settle it. Raise hills, dig rivers, plant
forests. Humans are drawn to good farmland and found villages. Elves settle old forests. Animals follow food and
water. Every one of them changes the land in turn: fields, roads, towns, felled woods. You can help a species thrive
or push it out, and the world reacts to the land you give it.

## Pillars

- **The land is the game.** Everything reads the terrain and everything changes it. The player's main tool is
  shaping it.
- **One grid of numbers.** The world is a grid of tile ids (1 grass, 2 dirt, 3 water, ...). Generation, the
  simulation and the player only change numbers; the look follows from them by itself.
- **Readable cause and effect.** You can always see why a village grew or a species left (attraction overlays,
  short event messages).
- **Small first, then grow.** One species and one loop working before the next.

## The world

- A tile map: width x height tile ids (`uint8`, up to 256 types), plus per-tile data the simulation needs later
  (fertility, owner, age). Tiles are plain state, not entities.
- **Generation:** layered value noise picks a base type per tile from height and moisture (water, sand, dirt,
  grass, forest, rock), with a seed so a world can be made again.
- **Variants:** a tile's look variant (flowers on grass, wet sand by water) comes from its neighbours' types and a
  hash of its position, so it is stable and needs no storage.

## Terrain blending (the shader)

Each art pixel looks at the tiles around it and picks the type that wins there:

1. Upload the tile ids as a small texture (one texel per tile, R8, nearest).
2. Per art pixel (pixel-snapped, e.g. 16 art pixels per tile), read the 3 x 3 tiles around it.
3. Give each tile a weight by its distance to the pixel (a smooth falloff; a 2 x 2 bilinear weight is cheaper but
   gives diamond-shaped corners), summed per type.
4. Add clustered noise (per 2 x 2 art pixels) so edges look hand-drawn; take the type with the highest weight and
   sample only its texture.

Corners round off and half-corners appear by themselves, a lone tile becomes a blob, and there are no transition
tiles to draw. Tested in `scripts/prototypes/terrain_blend.py` (four panels: raw, bilinear, smooth, smooth + noise).

Known edge case: **diagonal one-tile lines** (a stream, a road) pinch into beads, because the two types tie exactly
between the diagonal tiles. Fix with a per-type priority (water and roads win ties, so they stay connected), or let
generation and tools add a tile at each diagonal step.

Hydro's own `terrain.frag` already picks the strongest layer per art pixel with edge noise, so this matches how the
engine draws terrain; the difference is a tile-id texture instead of a 4-channel weight map, so many more types.

## Species

Each species is data: what attracts it (tile types nearby, water, space, other species), what it avoids, how it
grows, and how it changes the land.

| Species | Drawn to | Changes the land |
| --- | --- | --- |
| Humans | Grass and dirt near water | Fields (plowed tiles), roads, houses, a town; fell forest |
| Elves | Old forest | Groves, tree homes; let forest spread |
| Animals (deer, wolves, ...) | Food, water, cover | Graze, wear paths |

The simulation runs at a fixed tick: attraction scores per area, settling, growth or decline, then terrain changes.
Every step is a pure rule (state, data and a `Random&` in, a result out), unit-tested.

## Player

Brushes to raise, lower, paint and plant; later, powers that help or harm (rain, drought, fire, blessing a
village). No direct control over the species: you change their world, not them.

## First milestone

1. Generate a tile map from a seed and draw it with the blend shader.
2. Paint tiles with a brush.
3. Humans: appear where land is attractive, found a village, plow fields around it (grass becomes plowed field).
4. An overlay showing why a spot is attractive.

## Open questions

- Top-down or isometric (the reference picture is isometric)?
- Map size and whether it scrolls or is one screen.
- Settlers as entities with sprites, or only drawn as part of the settlement tiles?
- Win or lose conditions, or a pure sandbox?
