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
2. Per art pixel (pixel-snapped, e.g. 16 art pixels per tile), read the 5 x 5 tiles around it.
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

## Progression: an ecosystem that unlocks itself

(Added Nov 9, 2026.) A run starts on an empty world of one base type (dirt) with only a few brushes (dirt and grass).
What you paint lets life appear, and life unlocks the next tools. The goal is balance: keep the cycle going and let
all kinds of things happen on the world.

1. Grass painted on dirt slowly fills with life: small grazers appear once there is enough grass.
2. Once grazers pass a threshold, predators appear; trees start to spread.
3. Each step unlocks new terrain types and brushes (water, sand, snow, ...), and later the species of the table above.
4. Too many grazers strip the grass, too many predators starve: the player keeps it in balance by shaping the land.

Unlocks are run state (`HearthlandsSession::UnlockedTiles`); which types start unlocked and what unlocks the others is
data (`TerrainTypeDefinition::StartsUnlocked`, `UnlockTileId`, `UnlockCount`).

### Water and rivers (Nov 9, 2026)

A run starts with two tools: **Dig** (an empty riverbed) and **Dirt**. There is no water brush: water flows into dug
channels from the water they touch, one tile per tick. Each water tile has a head, the strongest neighbouring water's head
less a loss; a narrow channel carries water about 24 tiles inland, a wide one or a junction of rivers loses less and goes
further. Cutting a river with dirt drains it behind the cut. The sea is salt: only fresh water (rivers, lakes on the land)
wets the land around it, fading over 6 tiles, and grass sprouts and spreads there by the terrain rules (data,
`TerrainRules.hytable`) and dries away from water. 400 grass tiles unlock the Grass brush. Later: mountains and springs
as more water sources; water that only reaches so far inland gives the player a reason to shape the land.

## First milestone

1. Generate a tile map from a seed and draw it with the blend shader. Done; a world can also start as one type.
2. Paint tiles with a brush. Done (keys 1-9 pick an unlocked type, left mouse paints).
3. Grass life: grazers appear on enough grass and eat it; grass regrows (the first loop of the progression).
4. An overlay showing why a spot is attractive.

## Open questions

- Top-down or isometric (the reference picture is isometric)?
- Map size and whether it scrolls or is one screen.
- Settlers as entities with sprites, or only drawn as part of the settlement tiles?
- Win or lose conditions, or a pure sandbox?
