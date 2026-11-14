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
- **Seasons (built Nov 11)**: `Seasons.hytable`, a 20-day year (four seasons of 5 days, each turning into the next over
  its last 1.5 days; the clock shows "Spring, day 3"). Per season: the snowline's move (spring -1, summer +2, winter -3
  levels; snow never on land below rock), how far rivers and springs reach (`Flow`: spring 1.25, summer 0.85, winter
  0.8), meltwater (`Melt`: a spring flood 1.6, winter 0.2), how fast the land and plants grow (`Growth`, `Life`: spring
  1.4, autumn 0.6, winter 0.1), and the look: the land's tint, green fading to straw (`Fade`: autumn 0.45), snow lying
  on the ground in drifts, thickest up high (`GroundSnow`: winter 0.55), ice from the shores (`Ice`: winter 0.5), leaf
  colours on leafy objects (`LeafTint` on `Deciduous` objects: orange autumn, bare grey winter; pines stay). Snow and
  ice are a look only: grass under the snow is still grass. `terrain_blend.py` renders `terrain_seasons.png`.
  Next for the look: real seasonal sprites from PixelLab (autumn trees, bare winter trees, snowy pines) as object
  data; snow on cliff walls; falling leaves and snowflakes as particles.

### Life

- **Succession**: dirt -> grass -> shrubs -> forest while moist; back to dirt when overgrazed, burnt or dry.
- **Grass holds moisture** (Nov 10): a tile walled in by grass (5 of 8 neighbours) has at least the grass's mean moisture
  plus the type's `Shelter` (grass 0.15), so holes and bays in a meadow fill in. Only if the grass is wet (dry grass holds
  nothing), and the bonus is never passed on: with 5 of 8 a straight edge never advances, grass stays inside its own
  convex outline, and it all still dries when the water goes.
- **Forests away from rivers (built Nov 11), led by the player**: the hidden groundwater does nothing by itself until
  the player paints grass on it. Grass's roots reach a little of it (`Roots` 0.1), so grass painted where the
  groundwater is good (about 0.5 and up: the clues and the Insight talents show where) holds instead of drying, while
  over poor groundwater it dries as before. On that grass trees come up from their own roots (`Roots`: saplings about
  0.45, grown trees 0.5-0.65 of the groundwater): mostly pine (it needs least), birch and oak only over strong
  groundwater or near water. A forest's canopy (`Canopy`, within 2 tiles) keeps its trees' water in the living ground
  around it (up to 0.9 of it under a closed canopy), so the grass under and beside a forest never dries and gaps fill
  in. Oak saplings come up under trees, birch and pine saplings prefer the open; ferns grow in the shade. Forests don't
  march over bare ground by themselves: the Life talent **Forest edge** (row 6, 2 points) lets the shade keep bare
  ground damp too, so forests creep out on their own (about half a tile a day over medium groundwater, stopping where
  it is poor). Headless run (60 days): a painted 12 x 12 patch over groundwater 0.65 holds and grows about 22 trees,
  one over 0.35 dries; the river's band stays about 10 tiles without the talent and grows to about 30 with it.
- **Groundwater sight (built Nov 11)**: the Insight talent Groundwater sight (row 6, 1 point, after Sense water) adds a
  Groundwater button (G) to the Inspect bar: grey-tan where painted grass dries, teal from 0.5 where it holds and
  forest can grow, deep blue with bright dots from 0.78 where a dug bed springs. The faint clues stay for finding it
  without the talent.
- **Dry creeks (built Nov 11, dry since 22:45)**: a new world gets about 1.2 creeks per 100 x 100 tiles of land
  (`WorldScript::Creeks`; about 21 on the 512 island): a narrow riverbed of 6 to 14 tiles winding over the low ground
  from a head on the strongest groundwater (0.8 and up), at least 10 tiles from other water (`CreekRules`). They start
  **dry** (the user, Nov 11 22:45: flowing creeks greened the land by themselves and paid milestones for free): their
  beds are sealed (`Hydrology::Sealed`) and never spring on their own; digging the head (Dig brush) opens the spring,
  which counts as a spring found and fills the creek as far as its head carries (about 7 tiles; Strong springs further).
  A hint at the start says so. A sealed bed filled in and dug again is an ordinary bed.
- **Old woods (built Nov 11)**: with the Life talent Old woods (row 7, 2 points, after Lush growth) a grown oak or pine
  deep inside closed forest (its canopy and its neighbours' within 2 tiles adding up to 7: an edge tree has about 6)
  becomes an ancient oak or old pine in about 30 days (a year and a half). Felling nearby or gaps hold it back, so old
  woods need the player to grow a forest and leave it alone. Ancient trees give more food (acorns, mushrooms) and wood
  (humans love to fell them). Milestone Old growth (5 ancient trees, 2 points). Art: the grown sprites made larger,
  darker and mossy (`scripts/art/make_temp_oldwoods_art.py`), temporary.
- **Fauna as populations per region** (a predator-prey model per area), with only a sample of animals drawn.
- **Wildlife (built Nov 11)**: `Animals.hytable`, one row per species. The map is cut into 16 x 16 regions; per region
  `WildlifeRules` works out what the land holds of each species (tiles that suit it by type, moisture and height, times
  `Density`; none when the region's cover (shrubs and trees) or water share is out of range) and moves the number there
  towards it: births (`BirthsPerDay` times the season's `Life`), leaving above it, spreading to a neighbour with room,
  newcomers from outside into a region with room and none. Hunters (`PreyId`, the wolf on deer) live only where their
  prey feeds them and eat it. `Migrates` species follow the season's `Migrants` share (birds, insects away in winter),
  `Hibernates` ones the `Burrowed` share out of sight (badgers). 16 regions a tick: the whole map every 16 world seconds.
  `AnimalsView` shows a share of each region's number near the camera: walkers (deer, boar, wolf, badger) and birds
  (sparrow, robin, cardinal, bluejay, eagle, egret, crow) as sprite sheets that idle, wander, fly, land and run from
  the painting brush, fading in and out; fish (shadows in the water), butterflies, fireflies and starling flocks as
  particles. Milestones: first tracks, herds (40 deer), living land (8 species); talents: Wild lands (+50% room),
  Tracker (+50% shown). Art: free packs, credited in `Art/Animals/CREDITS.md`; fox, rabbit and bear to come (PixelLab).
- **Water life (built Nov 12, placeholder art)**: fish and water birds come only to water the **player** made (the user,
  Nov 11 23:08: unlock species by large rivers, rivers joined to the sea, streams). `HabitatRules` gives every water
  tile its habitats a few seconds after the water changed: the sea (outside the coast); of the water the world didn't
  start with (`Hydrology::Native`) a **stream** (at most 20 water tiles in its 9 x 9 square: 1 or 2 across), a **big
  river** (21 to 48: about 3 to 5 across) or a **lake** (49 and up: 6 across or more); **joined to the sea** (through
  any water) and its **mouth** (within 8 water steps of the sea); **cold** (meltwater, or level 4 and up). The world's
  own lakes are none of these, so nothing is free at the start. A species' `Habitat` (`WaterHabitat`): fish live in
  its tiles, birds need `MinHabitat` tiles of it in their region. New, all `Hidden` (not on the map until the first
  settle, then a "New species" notice): minnows (stream), brook trout (cold stream), pike (big river), salmon (river
  or stream joined to the sea: they leap a lot), eels (mouth), carp (lake); kingfisher (stream, bushes, hunts
  minnows), grey heron (big river, hunts fish), osprey (salmon river, hunts salmon), gulls (mouth), mallards (lake),
  dipper (cold stream). Fish show as their shapes drifting in the water, ripple rings and (`LeapsPerMinute`) leaps out
  of it, from `FishShapes.png`; the birds are the pack's sheets recoloured. All temporary
  (`scripts/art/make_temp_water_life_art.py`).
- **Discoveries (Nov 12)**: only the intro milestones show (first meadow, hidden water, first grove, highlands, first
  tracks, first settlers); the rest are `Hidden`: a ? card with a riddle (`Hint`) and the reward, no name or numbers,
  until reached ("Discovery: ..." notice). 19 new ones (37 in all, 12 a page): running water (30 stream tiles), a big
  river (80), to the sea (2 points), still waters, mountain stream; each new fish and bird; teeming waters (5 kinds of
  fish, 2 points), birdsong (10 kinds of birds, 2 points). Tracks: All, Land, Water, Woods, Wildlife, Peoples.
- **Soil (built Nov 11)**: under each tile its richness and its sand share (`SoilState`). Richness drifts towards the
  type's `Fertility` (sand 0.1, dirt 0.3, grass 0.45, riverbed 0.35) plus humus (shrubs +0.1, trees +0.2), silt from
  fresh water beside it (+0.25 at the bank) and the Humus talent where life is (grass, shrubs, trees); it builds up
  faster than it wears out. Painting a new type mixes half of its soil in, so dirt painted over sand makes loam (sand
  share about 0.55); quality = richness times texture (best at 0.4 sand). Plain dirt is neutral: terrain rules and
  objects have a `SoilEffect` (grass spreading and sprouting 0.8, drying -0.6; shrubs and broadleaf trees 0.6, pines
  0.2, dead wood -0.6) that makes them likelier on rich soil and less on poor. F shows the soil (pale poor to green
  rich). Milestone Rich earth (300 tiles at 60%); farming comes later as a talent with the settlers' area marks.
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
- **Elves and their branches** (the user, Nov 12): the first elves are like humans but live by the forest: they settle
  by forest and water (no old woods needed) and their foresters plant saplings. As they come to live by old forest they
  become **wood elves** (built: the talent Call of the woods allows it). Later branches, planning only: **high elves**
  in open land, relying on magic; **dark elves** in corruption zones (corruption is still to be designed). Each branch
  is a people row with `EvolvesFromId`; no group of a branch arrives on its own, settlements of its kin turn into it.
- **Magic (planning, the user Nov 12; not built)**: how magic shapes the world is still to be found, but it should be
  an early discovery. Every people may use it; they differ in how much they rely on it: humans use it but don't need it
  to grow, high elves rely on it for growth. Ideas to discuss: a magic discovery milestone and an Insight or Peoples
  talent; places of power on the map (old woods, springs, peaks) that magic draws on; corruption as magic gone wrong,
  where dark elves live.
- **Ages (planning, the user Nov 11; not built)**: a people climbs to its next age when its settlements hold enough
  people, food and materials for long enough, and the player has learned that age's talent (a Peoples branch). An age
  unlocks what the people does and builds, so the player's land and talents decide what each people becomes. First
  ladders to discuss:
  - **Humans**: camp (foraging, hunting, tents) -> village (farming unlocks: fields on rich soil, the soil system pays
    off; wooden houses; area marks for fields) -> town (tools and weapons from stone and later metal: quarries in the
    hills, smiths; better houses of timber and stone; roads) -> later ages (walls, mills, trade between settlements).
  - **Wood elves**: grove camp (lanterns, leaf huts, gathering) -> grove (tree homes in ancient trees, tending the
    forest: they plant saplings and protect old woods, forestry marks) -> tree city (bridges between ancient trees,
    healing magic over the land around: faster growth, soil richness) -> later (moonwells, spirits).
  - **Dwarves** (later): camp in the hills -> stone hall (mining, quarries) -> mountain hold.
  - Each age raises `MaxPeople`, adds shelter types (tent -> hut -> house) and work (fields, quarries, forestry),
    and can change what they need (elves need more old woods as they grow; humans need farmland). Weapons come with
    hunting and with the first conflicts between peoples over land (later, still no fail state).

### Talents

- **Points come from milestones for now** (Nov 10): one point per first-time event, such as the Grass brush unlocked,
  a hidden water source found, the first forest grown, the first grazers, the first settlers, a people reaching a new
  age. The milestones are data, so new ones are a row. Later ideas: points from variety and stability (species and
  types present, populations steady over days), so painting one huge field earns nothing.
- **Branches**: Water (wider dig, sense groundwater, wells, dams), Land (sand, stone, raise and lower), Life
  (introduce species, nurseries), Insight (overlays: fertility, populations, minerals; forecasts), Peoples (who
  settles and how often, peace between peoples; later ages and per-people traits; built Nov 12, see Settlers).
- **The world decides what is possible, talents decide which tools**: no wolf talent before there are deer.
- **Built (Nov 11)**: `Milestones.hytable` (9 first-time events, a point each: 50 / 400 / 1,500 grass, the first and
  fifth spring, 25 and 300 trees, land at rock level, the first snow) and `Talents.hytable` (15 talents in four
  branches, 21 points in all, so a run chooses): **Water** wetter banks and floodplains (moisture reach +1, +2), strong
  springs (+8 head), deep digging (springs from groundwater 0.1 weaker); **Land** the Sand brush, wide and great
  brushes (sizes up to 12 and 16); **Life** fertile and rich soil (land changes +50% each), seed wind and lush growth
  (plants spread +50% each; "lush growth" was "old woods" until Nov 11, that name is kept for ancient forests), forest
  edge (forests creep onto bare ground); **Insight** sense water, read the land, water sight (groundwater clues +0.25 each), faint
  traces (clues from 0.1 weaker groundwater). Some need the one above them. They are bought on the World Menu's
  Technology page (T); the brush bar's Grass and Raise still come by themselves.
- **Energy**: brushes cost energy (digging more than dirt) that refills with the world's health, so tools are choices.

### World Menu (Nov 11, 2026)

- **Tab opens it** (also the Menu button at the top right, purple while points wait; Esc opens it too): the world dims
  and the clock stops (Settings > Gameplay can leave it running). Tiles: **Technology** (T: the talent tree, a tab per
  branch with its talents as cards along a path, choose one and Learn), **Milestones** (cards with progress, by track,
  Spend points), **Settings** (Gameplay: overlays, pause; Controls; Audio, Graphics, Interface later), **Codex** and **Map Overview** (placeholders),
  **Return to World**; beside them Island Progress (season, points, talents, milestones, species, animals, brushes, a
  tip). Esc goes back to the tiles, then closes; the keys move between the buttons while it is open.
- **Look** (from the user's mock-ups, Nov 11): dark navy panels with gold rims, purple accents (ribbons, main buttons),
  parchment detail panels on the right, a serif for titles (Cormorant Garamond) over Open Sans. The art is drawn in code
  for now (`scripts/art/make_ui_art.py`); tile pictures are tinted placeholders until painted. The mock-ups' top bar of
  tabs and bottom resource bar are left out for now. The HUD (brush bar, clock, notices, settlements) uses the same look;
  top left only the Inspect bar (moisture M, soil F overlays); every key is in Settings > Controls, and a tutorial level
  will teach them later (the user, Nov 11). From the user's HUD mock-up (Nov 11): settlement cards with icon stats,
  brush pictures, toasts at the top right; its top tab bar, bottom stats bar and objective banner are not built yet.

### Settlers (Nov 11, 2026; settling on their own Nov 12)

- **Indirect control** (agreed Nov 11, made stricter Nov 12): the player shapes the land and picks talents; settlers
  choose for themselves where to live and what to do. The player doesn't place settlements or mark work areas: they
  learn talents that let a people settle, select a settlement to see it in depth, rename it and set its focus.
- **Settling on their own** (the user, Nov 12): a Peoples talent lets a people settle. First fire (humans), Elven kin
  (elves), Mountain halls (dwarves, after First fire). Their first group soon comes to the best place on the map for them
  (`SettlingRules::Find`: meets their needs, keeps `SettleSpacing` from others and from fresh ruins, liked most by
  `LikesWater/Forest/Meadow`). After that a new group now and then comes to another best place, half as likely for each
  settlement they have (`SettleDecay`); a later talent replaces the earlier and raises the chance (Wanderers, Elven
  paths, Deep delving), so the player chooses which people to favour and shapes the land for them. A full settlement of
  level 2 or more branches out: it sends a group to the best place within `BranchReach`. No land that suits them: the
  news says so and they look again in a few days. **Wood elves** never come as a group: an elf settlement with 4 ancient
  trees within 32 tiles becomes wood elves once Call of the woods (after Old woods) is learned (`SettlingRules::Evolve`:
  its hearth turns into a lantern, bowers into leaf huts, lodges into tree houses; name and villagers stay); the
  settlement panel shows how near they are.
- **Villagers and jobs**: every person is a villager with a name. Each day the settlement gives each one a job from what
  it needs: gatherers and hunters for food (whichever gives more: dwarves hunt, elves gather), guards while tension is
  high, a woodcutter and a builder while it needs shelters, foresters who plant saplings (elves, wood elves); then the
  player's **focus** (food, wood or building) takes
  the rest, else they rest. The hurt can't work. The gatherers' and hunters' work is what they eat; the land caps it
  (berries within reach, game in the region), so a settlement stops growing where its land runs out.
- **Levels** (each people's own, data in `Peoples.hytable`): humans camp (tents) -> hamlet (timber huts) -> village;
  elves camp (bowers) -> glen (elven lodges) -> village; wood elves grove (leaf huts) -> glade (tree houses, 8 old trees) -> elder hold (14); dwarves delving (stone huts) -> hold
  (stone halls, 60 rock) -> deep hall (90). A level needs people, shelters, days of food in store and the people's own
  needs; it gives a new shelter, more room and a bigger store. Ages (above) build on this later.
- **The settlement panel**: the HUD's Settlers list (level or tension per card; people / room, shelters, food, wood), and
  a click on a card or on a settlement's centre in the world opens its detail: people and level, stores, focus buttons,
  tension, what the next level needs (have / need, green when met) and every villager with their job.
- **Feuds** (the user, Nov 12: hostile, "not immediately murder" but much faster violence the closer they live): no
  limits on where peoples live. A people that dislikes another (`Dislikes`: elves and dwarves) builds tension living
  within `DislikeRadius` (32) of it, much faster the closer (by closeness squared), fading when apart. Quarrels from 25%,
  raids from 50% (food stolen or a shelter burned; guards drive raiders off), fights from 75% (villagers hurt for days),
  deaths only at the very top. Violence vents the attackers' tension and angers the victims, so it comes in waves; new
  groups prefer places away from peoples they dislike, but settle there when the land is much better. The talent
  Peacemakers halves the tension. In the sim, a dwarf hold beside an elf grove had its first quarrel after about ten
  days and its first death about a month later.
- **Look**: centres (fire, lantern, mine) with their light, shelters per level (on reaching a level one older shelter is
  rebuilt as the new level's at once, then the builders rebuild the rest with spare wood), villagers as small sprites doing their
  job (at bushes, at trees, out hunting, at shelters, on guard round the centre). Temporary art drawn in code
  (`scripts/art/make_temp_settler_art.py`: Hut, TreeHouse and StoneHall added Nov 12).
- **Farming** (the user, Nov 12): once a human settlement becomes a hamlet (milestone First hamlet), the Peoples talent
  Farming unlocks the **Farmland** brush. The player paints where fields may go; the villagers decide when to work
  them. **Farmers** sow empty fields in spring and summer and harvest ripe ones (`FarmRules`). Crops grow from sown to
  ripe over about `CropDays`, faster on rich (soil quality) and moist ground and in a growing season; winter frost
  withers what is still growing. A harvest gives `CropFood` by the soil's fertility and wears the soil a little, so the
  soil system matters to the peoples. Wild plant food is now seasonal too (`Harvest`: winter 0.25), so the store has to
  last the winter. A hamlet is no longer capped by berries and game: in the sim, humans with 40 painted fields reached
  village within a season or two, those without stayed hamlets for 120 days; stores still ran out by late winter, so
  more field, better soil and water are the player's levers. Nothing is built on farmland. Art is temporary.
  The brush paints only within reach of a settlement that farms (a hamlet's Radius; the user, Nov 12: anywhere else feels
  wrong); elsewhere nothing is laid and a notice says why.
- **Next**: tune in play; ages (planning above); more ways the peoples meet (trade, peace talents); a tutorial.

### Plan (agreed ideas, Nov 12, 2026; not built yet)

Everything agreed with the user on Nov 12, thought through as one game and put in a build order. Each step is talked
over briefly before it is built.

#### Buildings and unlocks (the user, Nov 12 14:04)

Every people gets new buildings as it unlocks things; until a building stands, what it gives is limited. This one
system carries farming, fishing, hunting, storage and defence, so it comes first.

- **Rule: no building, a low cap.** Each activity works without its building but only a little: the store holds a few
  days of food (today's `StoreDays`), hunters bring back only what they eat that day, no fish at all. A building lifts
  the cap or opens the job. So unlocks feel like growth, never like a wall, and the land still decides the most.
- **The player unlocks, the villagers build.** A Peoples talent (per people) makes a building type known; the settlement
  builds it itself when it needs it (its store is full, its hunters return more than it holds, fish live in water within
  reach) with its builders and materials, and places it by the work: a fishing lodge on the shore, a hunting lodge at the
  edge of the wild, a granary by the centre. Indirect control stays: the player never places a building.
- **Some need a level.** Defences only from level 3, and only if their talent is learned for that people.
- **Materials.** Food and wood today; **stone** comes in with dwarves' masons and humans' level 3 (quarries on rock),
  so stone buildings wait for both. Later tools. Costs are data.
- **Data, not code**: a `BuildingDefinition` table (`Buildings.hytable`): the people, the talent and level it needs,
  wood and stone cost, how many per settlement, where it stands (centre, shore, forest edge, rock), what it gives
  (`StoreBonus`, `HuntKeep`, a job it opens such as Fisher, `Defence`, `Lookout`), its sprite. Rules stay pure
  (`BuildingRules`: what is wanted, where it goes, what it gives); tests in `BuildingRulesTest.cpp`.
- **The panel** lists a settlement's buildings and what is next ("Granary: learn Storehouses", "Palisade: needs a town").

| Unlock (talent) | Humans | Elves | Wood elves | Dwarves |
| --- | --- | --- | --- | --- |
| Food store | Granary | Larder bower | Seed hollow (in an old tree) | Cold cellar |
| Hunting | Hunting lodge (keeps meat: hunters bring back more) | Hunters' blind | (they hunt little) | Hunting lodge |
| Farming | Barn (with the Farmland brush) | Orchard shed (with Orchards) | Orchard shed | Mushroom cellar (with mushroom beds) |
| Fishing | Fishing lodge, later a dock and boats | (later) | (later) | Fishing lodge on cold streams |
| Wood and stone | Woodcutters' hut; quarry at level 3 | Foresters' lodge | Foresters' lodge | Masons' hall, quarry |
| Defence (level 3 + talent) | Palisade, watchtower | Thorn hedge, lookout tree | Warding trees | Stone wall, gatehouse |

How it ties into what exists: stores into seasons (winter needs a granary's worth); hunting lodges into wildlife (more
meat, herds thin faster); fishing lodges into water life (the player's rivers feed people, fish thin); defences into
feuds (walls make raids fail more, towers spot raiders so guards turn out sooner; fights still happen); farming as
built, with the barn lifting how many fields a settlement works.

#### The other agreed ideas

- **Third-level art**: a shelter for every people's third level (humans a longhouse, elves a great hall, wood elves an
  elder tree, dwarves a carved gate); the centres grow a step with the levels (a bigger fire, a mine with a headframe).
  Temporary art drawn in code, as before; the new buildings above get the same.
- **Each people farms its own way** (same `FarmRules`, growth per people as data): elves and wood elves an **Orchard**
  brush (fruit trees their foresters plant: slow, a year to the first fruit, then fruit each autumn without sowing;
  little soil wear; like moisture and old woods near); dwarves **mushroom beds** on rock or dug ground near their mine
  (no soil, need moisture, no frost: steady in winter, less food). Each brush paints only within that people's reach,
  like Farmland.
- **Fishing** (the user, Nov 12): a talent unlocked once a human or dwarf settlement has its first lasting building (hut
  or stone hall); it lets them build a fishing lodge. **Fishers** sit at the shore, later go out in **boats** (a dock,
  a boat sprite on water tiles). The catch comes from the water life the player made (`HabitatRules`) and thins it.
- **The area grows with the level** (the user, Nov 12): a reach per `PeopleLevel` instead of one `Radius`, used by the
  survey, jobs, the farmland reach and where villagers walk. Villagers walk further as it grows and find their way home
  **without pathfinding**: they steer towards a heading, try a turned heading round water and steep height, and give up
  a spot they can't reach.
- **Work parties for large towns** (the user, Nov 12, like Total War): villagers with the same job walk as a group; one
  leader picks where to go, the others follow at an offset and spread round the spot; home the same way. Walking is only
  for show (the simulation counts jobs), so this cuts `SettlersView` work; in big towns a sprite stands for several
  people and far-off settlements update less often.
- **Naming** (the user, Nov 12): name settlements and villagers. Needs a text field in Hydro's UI first (an engine
  change: `UIInputFieldComponent`, focus, typed characters, Backspace, Enter, Escape, keyboard captured while focused);
  ASCII at first, Japanese input (IME) later. Rename stays as "suggest a name".
- **Hidden species with a loop** (the user, Nov 12): every species (later faction) starts hidden: **hint** (? card,
  silhouette, riddle; Insight talents sharpen hints or show where it could live), **discovery** (it appears, the card
  flips, an entry with its needs), **influence** (only then its talents: attract, protect, spread), **upgrade** (hidden
  variants by habitat: red deer in snowy hills, grey wolves in old woods). Always a few visible goals; Dev reveals all.
- **Pacing profiles** (the user, Nov 12): `Pacing.hytable` with Dev and Release rows scaling day and season length,
  milestone targets, talent costs and points, settling, births, level needs and building costs; Dist uses Release, Oxygen
  and Debug use Dev, Settings can switch. Milestone times logged; the headless sim prints a timeline per profile to tune
  against target times. Later a pace choice per world (Relaxed, Standard, Long).
- **Brush energy**: brushes cost energy that refills with the world's health; instant in Dev.
- **Saving and loading with several slots** (the user, Nov 12; makes testing easier): the whole session to a file per
  slot with a name, date and a small picture; quick save and load keys in Dev; a version number so old saves load.
- **More factions** (the user, Nov 12): warlike peoples that raid, peoples that build defences, more indirect ways to
  steer peoples; factions can arise like hidden species (a warlike people where land is scarce beside rich neighbours).

#### Build order

1. **Saving and loading** (slots, quick save in Dev): every later step is tested from saved worlds. **Built Nov 12**:
   World Menu > Saved Worlds (quick save and six slots, "Sure?" before loading or saving over), F5 / F9 quick save and
   load (in every build for now), files in `Saves/` beside the game; a load starts the world's scene again. No picture
   per slot yet; the saves folder moves to the player's own folder before a release.
2. **Buildings and unlocks**: `BuildingDefinition`, `BuildingRules`, the panel list; first the food store and hunting
   lodge for all four peoples (they change the food balance most), the barn for humans' farming.
   **Built Nov 12**: `Buildings.hytable` (granary, hunting lodge, barn; larder bower and hunters' blind for elves; seed
   hollow and woodland blind for wood elves; cold cellar and stone lodge for dwarves), eight 1-point Peoples talents
   (Storehouses, Hunting lodges, Larders, Hunters' blinds, Seed hollows, Woodland blinds, Cold cellars, Stone lodges;
   after each people's settling talent), the barn with Farming (from a hamlet; farmers work 12 fields without it, 36
   with it). Stores add 6 to 8 days of food, lodges half again as much meat. The settlement panel lists them; the
   Peoples tab now has pages. In the sim every people built theirs within its first two months; late-winter stores
   can still run dry (tune in play). Art is temporary.
3. **The area grows with the level**, then **work parties and steering home** (the same walking code).
   **Area and steering built Nov 13**: `PeopleLevel` has `Radius` (the land a settlement of that level uses: survey,
   felling, planting, farmland reach, where villagers walk; 0 keeps the people's `Radius`, which is still where a people
   looks to settle) and `ShelterReach` (where its shelters go, 0 keeps 4). Humans 10 / 13 / 16 tiles, elves 12 / 15 / 18,
   dwarves 12 / 14 / 17; wood elves keep their 32 at every level; shelters within 4 / 5 / 6. Villagers steer round water
   and steps steeper than their people's `Climb` (1; dwarves 2) with `WalkRules` (a heading turned to a kept side, no
   pathfinding), give up a spot after 3 s without getting nearer, and when lost far from home fade and come back by the
   centre. In a headless sim on thin land (2% berry bushes, no game) humans grow from 9 to 13 people at a hamlet, elves
   from 12 at a glen to a full village; on rich land nothing changes (they were full anyway). Work parties still to come.
4. **Fishing** (lodge, shore fishers; boats later) and **Orchards / mushroom beds** through the building system.
   **Fishing built Nov 14** (before work parties, the user agreed): talent Fishing (54, Peoples row 18, after First
   fire and First hamlet) lets a human hamlet build a **fishing lodge** (`BuildingKind::FishingLodge`, `Site: Shore`):
   once 2 or more of their fish live in water within reach, the builders put it on the shore tile whose region has
   the most. Talent Cold-stream fishing (55, row 19, after Mountain halls and the new hidden milestone First hold, 40)
   gives dwarf holds a **fishers' hall**. Without a lodge no one fishes; with one, up to its `Fishers` (3) fish when
   it feeds best (0.5 fish a day each, 5 food a fish), at most a quarter of the fish in reach at a time, so the catch
   thins the water life and follows it as it regrows. Humans fish all six of the player's fish (30-35), dwarves
   minnows, trout and salmon; elves not yet. Fishers sit at the shore. In a headless sim a hamlet on thin land by a
   2-tile stream of minnows grew from 12 people to a village of about 21. Boats and docks later.
5. **Third-level art** for shelters, centres and the new buildings.
6. **Pacing profiles** with the timeline from the sim, then **brush energy** (needs the profiles to tune).
7. **Hidden species loop** for land animals and plants, with influence talents.
8. **Stone, level 3 and defences**, then **more factions** (raiders, wall builders) on top of feuds.
9. **Naming** (Hydro text field first), whenever the engine has a quiet moment: it blocks nothing.

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
6. Fauna populations (built Nov 11: wildlife), soil (built Nov 11: richness, sand, loam; drainage still to come).
   Farming as a talent once there are peoples: built Nov 12 (Farmland brush, farmers, crops).
7. Talents (built Nov 11) and energy. Seasons built Nov 11.
8. One people (camp and first age), then the others. The Humans' camp built Nov 11 (settlers); the first age next.

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
