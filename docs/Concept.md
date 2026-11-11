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
2. Per art pixel (pixel-snapped, 32 art pixels per tile), read the 5 x 5 tiles around it.
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

The art on top (Nov 10, 2026; `TerrainAtlas.png`, 32 art pixels a tile):

- **Variants**: each type has a row of 8 textures; every tile picks one by its hash, and a texture listed twice shows
  up twice as often. Variants share their row's main colour, so tiles of one type meet without a seam.
- **Wet ground**: moisture is blended over the 5 x 5 tiles like the types; above a threshold (with clustered noise)
  a type draws from its wet row: lush grass, damp dirt and sand. Rivers grow green, damp banks.
- **Shore**: water with land among its 5 x 5 tiles draws shallow water, and close to the edge pale foam thickening
  into a white line. Thin streams (more land than water around them) get no foam.
- **Levels**: the tile texture's G holds the land's level (high 4 bits) next to the priority; a pixel blends only the
  tiles at its own tile's level, so raised land keeps square edges where its cliffs are (Height, below).
- Still to come: cliff and lip art for the walls (temporary pieces in `Art/Terrain/Temp`).

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
channels from the water they touch, one tile per tick; by the banks of a wide channel the water needs a little more
head (`BankHead`), so a wide river ends in a rounded tongue instead of a straight line, and the wet ground around it
fades in curves (a diagonal step counts 1.41). Each water tile has a head, the strongest neighbouring water's head
less a loss; a narrow channel carries water about 24 tiles inland, a wide one or a junction of rivers loses less and goes
further. Cutting a river with dirt drains it behind the cut. The sea is salt: only fresh water (rivers, lakes on the land)
wets the land around it, fading over 6 tiles, and grass sprouts and spreads there by the terrain rules (data,
`TerrainRules.hytable`) and dries away from water. 400 grass tiles unlock the Grass brush. Later: springs as another water source (mountains: below);
water that only reaches so far inland gives the player a reason to shape the land.

### Height, mountains and snow (Nov 10, 2026)

The land has a real height: a level per tile (`HeightMap`, 0 = the ground the water lies on, up to 15), next to the tile
ids. 1,500 grass tiles unlock the **Raise** brush (the rock type's brush): each press lifts a dome, one level at the rim and
one more every 2 tiles in (radius 8: 5 in the middle); holding the button doesn't add more, another press does. **Dig**
lowers raised land the same way, and on the ground digs a riverbed as before (below the ground the depth stays the
shader's fake). Slopes slump to at most 2 levels between neighbours: raising lifts the land around, digging cuts it down;
water, riverbeds and the sea hold their ground, so cliffs stand by rivers and the coast. Raising water fills it with dirt.

Rock and snow follow from the height (`MountainRules`, the types' `FromLevel`): rock from level 4, snow from level 9
(ragged by up to 0.75 a level); below that the land keeps its own type (grassy hills), and rock that drops below level 4
goes back to dirt. Painting never covers rock or snow. A mountain needs a few strokes for a cap. **Meltwater** runs down
the raised land within 16 tiles of snow: it wets that land (0.7 moisture right below the snow, fading to nothing at 16
tiles, never passed on), and a riverbed on the ground next to it fills (`MeltHead` 24 tiles), so a mountain is an inland
source and its rivers start at its foot. Hills are wet only from snow above or rivers below (`RainPerLevel`, rain on all
raised land, is 0): grass spreads where that is enough, so slopes become a patchwork of grass and dirt, greenest near the
snow and the river; a dry hill stays dirt.

Drawn as stepped blocks (`ReliefView`): the raised tiles' tops in the ground's own blend material, a level 8 art pixels
up (`LevelHeight` 0.2887 m at 32 pixels a tile from 30 degrees), and cliff walls in the types' `CliffColor`, banded per
level. Objects stand on their tile's top; the brush ray meets the hills. Trees by height (`ObjectDefinition` `MinLevel`,
`MaxLevel`, `ChancePerLevel`), only on grass, never on bare rock: oaks up to level 2, birches to 3, pines sprout and spread
more the higher they are (pine saplings twice as likely a level up), rock pines on the high grass (levels 2 and 3) as the
treeline. A big snowy mountain with a river at its foot: snow, bare rock (scree, crags), a band of pines on patchy grass,
grass and leafy trees by the river. Next: seasons move the snowline; later water
running downhill (real depth below the ground, waterfalls at the steps), and digging down to groundwater makes a spring.

## Design direction (Nov 10, 2026)

### Look

- **Isometric** (2:1, orthographic). The terrain stays the tile-id blend shader; everything standing on it (plants,
  rocks, buildings, animals, people) is an upright sprite on a tile. Round things are drawn front-on, buildings at the
  diagonal angle. Reference: the ChatGPT key art and asset sheet (style only; the cleaned sheet is placeholder art).
- **Real height** (Nov 10, replacing the fake layer heights per type): a level per tile, drawn as stepped blocks with
  cliff walls (Height, mountains and snow, above); below the ground (riverbeds, water) the depth is still faked in the
  shader.
- **Edges and detail**: rough, tufted edges per type (grass tufty, sand smooth), foam where water meets land, small
  decorations picked by a hash per tile (tufts, flowers, pebbles), chunkier repainted ground tiles, object shadows.
- **Scale**: 2 screen pixels per art pixel at the default zoom; each sprite's size is data, so a rabbit stays smaller
  than a hut.

### Water

- **Hidden groundwater** (built Nov 10): a per-tile value (0 to 1) from its own noise, in patches about 40 tiles
  across. A riverbed dug where it reaches 0.78 (about 1 tile in 11) becomes a **spring**: a water source whose head
  follows the groundwater (6 tiles of narrow channel at 0.78, 20 at full), so the inland can come alive too, and a wide
  hole dug over a patch becomes a lake. Springs only come from digging; filling one in (Raise) ends it. High groundwater
  also keeps the ground above a little damp (up to 0.12: painted grass holds, but it doesn't spread by itself).
  **Clues**: the ground over it looks damp, cool wet specks in clusters and a rare glint, plus blue dots in the
  moisture view (M). They start barely visible (`ClueClarity` 0.25) and only over spots that would spring
  (`ClueFrom` 0.78); talents and points will raise the clarity (clearer) and lower `ClueFrom` (find weaker spots).
  `scripts/prototypes/terrain_blend.py` renders `terrain_groundwater.png`: the clues at the start and fully revealed.
- **Seasons are part of the game** from the start: sources weaken in summer (droughts, rivers shrink, grass dries),
  rains and fuller rivers in spring, growth slows in winter; the snowline moves with them. Height per tile is in (Nov
  10); later water runs downhill. Mountains are sources already (meltwater at their foot, Nov 10).

### Life

- **Succession**: dirt -> grass -> shrubs -> forest while moist; back to dirt when overgrazed, burnt or dry.
- **Grass holds moisture** (Nov 10): a tile walled in by grass (5 of 8 neighbours) has at least the grass's mean moisture
  plus the type's `Shelter` (grass 0.15), so holes and bays in a meadow fill in. Only if the grass is wet (dry grass holds
  nothing), and the bonus is never passed on: with 5 of 8 a straight edge never advances, grass stays inside its own
  convex outline, and it all still dries when the water goes.
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
| Highlands | Raise brush (height; rock, snow and meltwater follow) | 1,500 grass tiles |
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
4. The art pass: rough edges, foam, decorations, repainted tiles, animated water; real height with stepped cliffs (Nov
   10; cliff art still to come).
5. Hidden groundwater and springs (built Nov 10; talents to sharpen the clues come with 7).
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
