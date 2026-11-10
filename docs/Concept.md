# Hearthlands: concept

Initial concept (Nov 9, 2026), with the design direction of Nov 10 below. A starting point to grow from, not a spec:
the direction can still pivot.

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

(The first idea; the design direction below replaces the peoples part with "Peoples and ages".) Each species is data: what attracts it (tile types nearby, water, space, other species), what it avoids, how it
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

## Design direction (Nov 10, 2026)

### Look

- **Isometric** (2:1, orthographic). The terrain stays the tile-id blend shader; everything standing on it (plants,
  rocks, buildings, animals, people) is an upright sprite on a tile. Round things are drawn front-on, buildings at the
  diagonal angle. Reference: the ChatGPT key art and asset sheet (style only; the cleaned sheet is placeholder art).
- **Fake elevation in the shader**, no real geometry: each type has a layer height (water and riverbed -1, dirt and
  sand 0, grass +1, rock +2). Where a higher type lies behind, the art pixel is part of its side face (a cliff colour
  per type), with a highlight on the lip and a shadow at the foot.
- **Edges and detail**: rough, tufted edges per type (grass tufty, sand smooth), foam where water meets land, small
  decorations picked by a hash per tile (tufts, flowers, pebbles), chunkier repainted ground tiles, object shadows.
- **Scale**: 2 screen pixels per art pixel at the default zoom; each sprite's size is data, so a rabbit stays smaller
  than a hut.

### Water

- **Hidden groundwater**: a per-tile value (0 to 1) from noise. Digging onto it makes a spring, a water source whose
  strength follows the groundwater, so the inland can come alive too. Clues on the surface (reeds, darker dirt, a
  faint sign in the moisture view) keep it from being blind digging; a talent shows it outright.
- **Seasons are part of the game** from the start: sources weaken in summer (droughts, rivers shrink, grass dries),
  rains and fuller rivers in spring, growth slows in winter. Later: elevation per tile (water runs downhill),
  mountains as sources.

### Life

- **Succession**: dirt -> grass -> shrubs -> forest while moist; back to dirt when overgrazed, burnt or dry.
- **Fauna as populations per region** (a predator-prey model per area), with only a sample of animals drawn.
- **Disturbances** that test the balance: drought, flood, wildfire through dry grass, disease.

### Peoples and ages (evolution)

- **Humans, elves and dwarves all start prehistoric**: a camp of lean-tos and round huts, foraging and hunting. Each
  people has **its own ladder of ages** (for example elves: camp -> grove -> tree city; dwarves: camp -> stone hall ->
  mountain hold) that it climbs when conditions hold (food surplus, population, materials) and talents allow; each age
  brings buildings and new ways of changing the land.
- **Affinities**: each people favours some activities. Elves: foraging and forestry (groves, tree homes, forest
  spreads). Humans: farming and building (fields, roads). Dwarves: mining and stone (quarries, hills, minerals).
  Activities: foraging, hunting, fishing, farming, herding, forestry, mining, building.
- **Talents act through affinities**: a talent raises an activity, and each people gets it times its affinity
  ("Seed gathering: +50% foraging" -> elves +75%, dwarves +25%). Same talent, different impact per people.
- **Peoples come to land that suits them**: elves to old forest by fresh water, humans to fertile grass by water,
  dwarves to rock, hills and minerals. The player never controls them directly (pillar): they shape the land and
  choose talents.
- **Regions are shared and contested**: peoples can live side by side (elves in the forest next to human fields) and
  also compete for the same land (humans fell the forest the elves need); the player tips the balance by shaping it.

### Talents

- **Points come from milestones for now** (Nov 10): one point per first-time event, such as the Grass brush unlocked,
  a hidden water source found, the first forest grown, the first grazers, the first settlers, a people reaching a new
  age. The milestones are data, so new ones are a row. Later ideas: points from variety and stability (species and
  types present, populations steady over days), so painting one huge field earns nothing.
- **Branches**: Water (wider dig, sense groundwater, wells, dams), Land (sand, stone, raise and lower), Life
  (introduce species, nurseries), Insight (overlays: fertility, populations, minerals; forecasts), Peoples (ages,
  per-people traits).
- **The world decides what is possible, talents decide which tools**: no wolf talent before there are deer.
- **Energy**: brushes cost energy (digging more than dirt) that refills with the world's health, so tools are choices.

### Structure

No fail state (cozy); optional island goals (a self-sustaining forest with wolves, a dwarf town in the hills) next to
a free sandbox; seeded islands; saving and loading.

### Systems

| System | Reads | Changes |
| --- | --- | --- |
| Water (`WaterRules`) | tiles, sources (later groundwater, height, season) | water tiles, heads, moisture |
| Growth (`GrowthRules`, `TerrainRules`) | tiles, moisture, neighbours | tiles (succession) |
| Plants and objects | tiles, moisture | the object layer (bushes, trees, rocks) |
| Fauna | food (grass, bushes), water, cover, predators | populations per region; grazing changes tiles |
| Peoples | attraction (land, food, water, materials), age, talents x affinity | settlements, buildings, land use |
| Talents and energy | the world's variety and stability | unlocks, multipliers, energy |
| Disturbances | season, dryness, events | drought, fire, flood |

### Progression (first numbers, to tune)

| Stage | The player gets | When |
| --- | --- | --- |
| Barren island | Dirt, Dig | start |
| Riverlands | Grass brush | 400 grass tiles |
| First life | bushes, grazers appear | enough grass and bushes |
| Predators and forest | wolves, trees spread | enough grazers |
| First people | a camp of the people the land suits | food, fresh water, shelter |
| Ages | buildings and land use per people | talents and conditions |

### Build order

1. The isometric orthographic camera. Done (Nov 10).
2. The object layer: data (sprite, size, where it grows), state (objects per tile), a view (sprites, culling);
   placeholders from the cleaned sheet. Done (Nov 10).
3. Succession: bushes first, saplings between them, trees through stages; pines on the drier edge. Done as object
   data (Nov 10); later bushes giving way to forest, and grass under old forest.
4. The art pass: raised layers, rough edges, foam, decorations, repainted tiles, animated water (needs a time value
   in Hydro's frame data).
5. Hidden groundwater and springs.
6. Fauna populations.
7. Talents and energy.
8. One people (camp and first age), then the others.

## First milestone

1. Generate a tile map from a seed and draw it with the blend shader. Done; a world can also start as one type.
2. Paint tiles with a brush. Done (keys 1-9 pick an unlocked type, left mouse paints).
3. Grass life: grazers appear on enough grass and eat it; grass regrows (the first loop of the progression).
4. An overlay showing why a spot is attractive.

## Open questions

Answered Nov 10: isometric; 512 x 512, scrolling camera; animals and people as sprites (populations drawn by
sample); no fail state, optional island goals plus a sandbox.

Answered Nov 10 (later): a ladder of ages per people; regions shared and contested; seasons from the start; talent
points from milestones (first-time events) for now.

- How much energy a brush stroke costs, and how fast it refills.
- How long a season lasts (in days), and how hard a drought is in the first year.
