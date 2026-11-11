#version 450

#extension GL_GOOGLE_include_directive : require

// Hearthlands terrain: blends a grid of tile ids per art pixel (docs/Concept.md, "Terrain blending";
// scripts/prototypes/terrain_blend.py is the reference). A material shader (Hydro MaterialData::Shader) on a Plane mesh
// sized in tiles (1 tile = 1 m): fragUV is the position in tiles from the plane's centre, v pointing down (-Y).
//
// Material fields used:
//   Albedo   = the tile atlas, sRGB (scripts/art/make_terrain_atlas.py): Tiling.x texels a tile, one row per type id with
//              a few variants side by side (each tile picks one), and rows WetRow + id for the wet look (moist land,
//              shallow water next to land). An atlas shorter than a tile is a palette: one texel per id, flat colours.
//   Emissive = the tile map, made in code (AssetManager::AddMemoryAsset), RGBA (linear), 1 texel per tile, rows as
//              TileMap stores them (row 0 = the top = the plane's +Y edge): R = tile id, G = priority (ties: higher wins) in
//              the low 4 bits and the land's level (HeightMap) in the high 4, B = the role in the low 2 bits (2 water,
//              1 empty riverbed, 0 land) and the groundwater clue (0 to 63) in the high 6, A = moisture. Only tiles at the
//              level of the fragment's own tile blend: cliffs keep their edges (ReliefView). It has a ring of the outside
//              tile (water) around the map; lookups clamp to it, so a plane larger than the texture shows it beyond the map.
//   Tiling.x = art pixels per tile (32), Tiling.y = edge noise (e.g. 0.18)
//   Tint     = rgb multiplies the colour (white = as drawn); alpha below 0.5 shows the moisture overlay (the map's alpha)
// Up to MaxTypes ids; other ids draw as id 0.

#include "lighting.glsl"
#include "surface_derivatives.glsl"
#include "mesh_material.glsl"

const int MaxTypes = 16;
const float Sigma = 0.75; // the prototype's smooth falloff, in tiles (wide, so edges and diagonals come out round)
const int Reach = 2;      // tiles read on each side: 5 x 5
const int WetRow = 16;    // the atlas rows of the wet looks start here (make_terrain_atlas.py)
const float WetFrom = 0.62;    // blended moisture where land starts to look wet (clustered noise moves it by WetJitter)
const float WetJitter = 0.16;
const float ShallowFrom = 0.04; // share of land around a water pixel where it turns shallow
const float FoamFrom = 0.36;    // ... where foam starts, thickening towards the shore (none past 0.5: thin streams)
const float WaterSteps = 6.0;   // animation frames a second: water moves in pixel-art steps, not smoothly
const vec2 Drift = vec2(1.5, 0.75); // art pixels a second the water's texture drifts

float Hash(vec2 cell, float seed)
{
	return fract(sin(dot(cell, vec2(12.9898, 78.233)) + seed * 37.7) * 43758.5453);
}

void main()
{
	ivec2 mapSize = textureSize(emissiveTexture, 0);
	float artPixels = max(material.tiling.x, 1.0);
	float noiseAmount = material.tiling.y;

	vec2 tile = vec2(fragUV.x, -fragUV.y) + vec2(mapSize) * 0.5; // tiles from the map's bottom-left (-X, -Y) corner
	SurfaceDerivatives derivatives = GetSurfaceDerivatives(fragWorldPos, tile);
	vec2 artPixel = floor(tile * artPixels);        // the art pixel this fragment is in
	vec2 p = (artPixel + 0.5) / artPixels;          // its centre, in tiles
	vec2 base = floor(p);

	float weight[MaxTypes];
	float priority[MaxTypes];
	bool waterType[MaxTypes];
	for (int i = 0; i < MaxTypes; i++)
	{
		weight[i] = 0.0;
		priority[i] = 0.0;
		waterType[i] = false;
	}

	ivec2 baseCell = clamp(ivec2(base), ivec2(0), mapSize - 1);
	int baseLevel = int(texelFetch(emissiveTexture, ivec2(baseCell.x, mapSize.y - 1 - baseCell.y), 0).g * 255.0 + 0.5) >> 4;

	float total = 0.0;
	float land = 0.0;     // the weight of tiles that are neither water nor riverbed
	float moisture = 0.0; // moisture, weighted like the types: wet ground blends as smoothly as the edges
	float clueSum = 0.0;  // groundwater clues, weighted the same
	for (int oy = -Reach; oy <= Reach; oy++)
	{
		for (int ox = -Reach; ox <= Reach; ox++)
		{
			vec2 neighbour = base + vec2(ox, oy);
			ivec2 cell = clamp(ivec2(neighbour), ivec2(0), mapSize - 1);
			ivec2 texel = ivec2(cell.x, mapSize.y - 1 - cell.y); // TileMap row 0 is the top
			vec4 value = texelFetch(emissiveTexture, texel, 0);
			int packed = int(value.g * 255.0 + 0.5); // priority in the low 4 bits, level in the high 4
			if ((packed >> 4) != baseLevel)
			{
				continue; // across a cliff: another level's tiles don't blend into this one
			}
			int id = int(value.r * 255.0 + 0.5);
			id = id < MaxTypes ? id : 0;
			vec2 d = p - (neighbour + 0.5);
			float k = exp(-dot(d, d) / (2.0 * Sigma * Sigma));
			weight[id] += k;
			priority[id] = float(packed & 15);
			total += k;
			int roleAndClue = int(value.b * 255.0 + 0.5); // role in the low 2 bits, clue in the high 6
			int role = roleAndClue & 3;
			land += role == 0 ? k : 0.0;
			moisture += value.a * k;
			clueSum += float(roleAndClue >> 2) / 63.0 * k;
			waterType[id] = role == 2;
		}
	}

	// Clustered noise (2 x 2 art pixels) so edges look hand drawn; priority only breaks exact ties.
	vec2 cluster = floor(artPixel / 2.0);
	float clusterNoise = (Hash(cluster, 0.0) - 0.5) * noiseAmount;
	int winner = 0;
	float best = -1e9;
	for (int i = 0; i < MaxTypes; i++)
	{
		if (weight[i] <= 0.0)
		{
			continue;
		}
		float score = weight[i] / total + clusterNoise + (Hash(cluster, float(i + 1)) - 0.5) * noiseAmount * 0.5 + priority[i] * 1e-4;
		if (score > best)
		{
			best = score;
			winner = i;
		}
	}

	// Water moves with the engine's clock (FrameSeconds), stepped to WaterSteps frames a second.
	float seconds = floor(FrameSeconds() * WaterSteps) / WaterSteps;

	// The winner's art: a variant in its row (picked per tile), repeating every tile (image rows go down, the world's Y up).
	ivec2 here = clamp(ivec2(floor(p)), ivec2(0), mapSize - 1);
	vec4 hereValue = texelFetch(emissiveTexture, ivec2(here.x, mapSize.y - 1 - here.y), 0);
	float landShare = land / max(total, 1e-5);
	bool water = waterType[winner];
	ivec2 atlasSize = textureSize(albedoTexture, 0);
	int tileTexels = int(artPixels);
	vec3 albedo;
	if (atlasSize.y < tileTexels)
	{
		ivec2 paletteTexel = ivec2(min(winner, atlasSize.x - 1), 0); // a palette: one colour per id
		albedo = texelFetch(albedoTexture, paletteTexel, 0).rgb;
	}
	else
	{
		int columns = max(atlasSize.x / tileTexels, 1);
		int column = int(Hash(base, 3.0) * float(columns)) % columns;
		float wetness = moisture / max(total, 1e-5);
		float shallowFrom = ShallowFrom * (0.3 + 1.4 * Hash(cluster, 7.0)); // dithered: no hard line between shallow and deep
		bool wet = water ? landShare > shallowFrom : wetness > WetFrom + (Hash(cluster, 4.0) - 0.5) * WetJitter;
		int row = winner + (wet ? WetRow : 0);
		row = (row + 1) * tileTexels <= atlasSize.y ? row : winner;
		ivec2 inTile = min(ivec2(fract(p) * float(tileTexels)), ivec2(tileTexels - 1));
		if (water)
		{
			inTile = (inTile + ivec2(floor(Drift * seconds))) % tileTexels; // the surface drifts
		}
		ivec2 atlasTexel = ivec2(column * tileTexels + inTile.x, row * tileTexels + tileTexels - 1 - inTile.y);
		albedo = texelFetch(albedoTexture, atlasTexel, 0).rgb;
	}

	// Ripples: light streaks running across the water in waves, a few pixels lit at a time.
	if (water)
	{
		float wave = sin(dot(artPixel, vec2(0.21, 0.37)) - seconds * 2.2);
		float across = sin(dot(artPixel, vec2(-0.13, 0.29)) - seconds * 1.3);
		float glint = Hash(artPixel, 10.0 + mod(floor(seconds * 2.0), 64.0));
		albedo = wave > 0.9 && across > 0.2 && glint > 0.45 ? mix(albedo, vec3(0.27, 0.46, 0.79), 0.7) : albedo;
	}

	// Shore foam: on water pixels close to land, pale specks thickening into a white line at the edge; it breathes in and
	// out along the shore, and its specks twinkle.
	if (water && landShare > FoamFrom && landShare < 0.5)
	{
		float swell = sin(seconds * 1.4 + dot(p, vec2(0.7, 0.45))) * 0.18;
		float edge = (landShare - FoamFrom) / (0.5 - FoamFrom) + swell + (Hash(artPixel, 6.0 + mod(floor(seconds * 3.0), 64.0)) - 0.5) * 0.6;
		albedo = edge > 0.75 ? vec3(0.81, 0.86, 0.91) : (edge > 0.35 ? mix(albedo, vec3(0.27, 0.46, 0.79), 0.8) : albedo);
	}
	// Groundwater clues: over high groundwater the ground looks damp, cool wet specks in clusters and a rare glint, as plain
	// as the clue (GroundwaterRules::Clue: faint at first, clearer with talents).
	float clue = clueSum / max(total, 1e-5);
	if (water == false && clue > 0.0)
	{
		float specks = clue * (0.25 + 0.5 * Hash(cluster, 11.0));
		float pick = Hash(artPixel, 12.0);
		vec3 damp = mix(albedo, vec3(0.06, 0.10, 0.14), 0.6);
		albedo = pick < clue * 0.04 ? vec3(0.26, 0.43, 0.61) : (pick < specks ? damp : mix(albedo, albedo * vec3(0.85, 0.9, 1.0), clue * 0.5));
	}
	albedo *= material.tint.rgb * fragColor.rgb;

	// The moisture overlay (tint alpha below 0.5): the tile's moisture (the map's alpha), dry orange to wet blue, over its art.
	if (material.tint.a < 0.5)
	{
		float wet = hereValue.a;
		albedo = mix(albedo, mix(vec3(0.85, 0.55, 0.25), vec3(0.15, 0.45, 1.0), wet), 0.6);
		albedo = water == false && Hash(artPixel, 13.0) < clue * 1.5 ? vec3(0.2, 0.55, 1.0) : albedo; // groundwater dots
	}

	if (IsForwardLit() == false)
	{
		outColor = vec4(albedo, 1.0);
		outEntityID = entityID;
		return;
	}

	// Lit like the Standard material, light and shadow stepping with the art pixels.
	vec3 normal = normalize(fragNormal);
	vec3 position = fragWorldPos;
	if (SnapsToTexels())
	{
		position += TexelCentreOffset(derivatives, tile, vec2(artPixels));
	}
	float sun = SunShadowSurface(position, normal);
	vec3 lit = albedo * ComputeLighting(position, normal, 0.5, sun);
	outColor = vec4(ApplyFog(lit, position), 1.0);
	outEntityID = entityID;
}
