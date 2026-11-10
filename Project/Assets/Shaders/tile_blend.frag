#version 450

#extension GL_GOOGLE_include_directive : require

// Hearthlands terrain: blends a grid of tile ids per art pixel (docs/Concept.md, "Terrain blending";
// scripts/prototypes/terrain_blend.py is the reference). A material shader (Hydro MaterialData::Shader) on a Plane mesh
// sized in tiles (1 tile = 1 m): fragUV is the position in tiles from the plane's centre, v pointing down (-Y).
//
// Material fields used:
//   Albedo   = the tile atlas, sRGB: one square column per type id in id order (height x height texels each), repeated
//              every tile. A 1-texel-high atlas (one colour per type, made in code from TerrainTypeDefinition::Color)
//              gives flat colours until the types have art.
//   Emissive = the tile map, made in code (AssetManager::AddMemoryAsset), RGBA (linear), 1 texel per tile, rows as
//              TileMap stores them (row 0 = the top = the plane's +Y edge): R = tile id, G = priority (ties: higher wins), A = moisture.
//              It has a ring of the outside tile (water) around the map; lookups clamp to it, so a plane larger than
//              the texture shows that tile beyond the map. Both stay centred on the plane.
//   Tiling.x = art pixels per tile for the blend (e.g. 16), Tiling.y = edge noise (e.g. 0.18)
//   Tint     = rgb multiplies the colour (white = as drawn); alpha below 0.5 shows the moisture overlay (the map's alpha)
// Up to MaxTypes ids; other ids draw as id 0.

#include "lighting.glsl"
#include "surface_derivatives.glsl"
#include "mesh_material.glsl"

const int MaxTypes = 16;
const float Sigma = 0.75; // the prototype's smooth falloff, in tiles (wide, so edges and diagonals come out round)
const int Reach = 2;      // tiles read on each side: 5 x 5

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
	for (int i = 0; i < MaxTypes; i++)
	{
		weight[i] = 0.0;
		priority[i] = 0.0;
	}

	float total = 0.0;
	for (int oy = -Reach; oy <= Reach; oy++)
	{
		for (int ox = -Reach; ox <= Reach; ox++)
		{
			vec2 neighbour = base + vec2(ox, oy);
			ivec2 cell = clamp(ivec2(neighbour), ivec2(0), mapSize - 1);
			ivec2 texel = ivec2(cell.x, mapSize.y - 1 - cell.y); // TileMap row 0 is the top
			vec4 value = texelFetch(emissiveTexture, texel, 0);
			int id = int(value.r * 255.0 + 0.5);
			id = id < MaxTypes ? id : 0;
			vec2 d = p - (neighbour + 0.5);
			float k = exp(-dot(d, d) / (2.0 * Sigma * Sigma));
			weight[id] += k;
			priority[id] = value.g * 255.0;
			total += k;
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

	// The winner's art: its column of the atlas, repeating every tile (image rows go down, the world's Y goes up).
	int columnSize = max(textureSize(albedoTexture, 0).y, 1);
	ivec2 inTile = min(ivec2(fract(p) * float(columnSize)), ivec2(columnSize - 1));
	ivec2 atlasTexel = ivec2(winner * columnSize + inTile.x, columnSize - 1 - inTile.y);
	vec3 albedo = texelFetch(albedoTexture, atlasTexel, 0).rgb * material.tint.rgb * fragColor.rgb;

	// The moisture overlay (tint alpha below 0.5): the tile's moisture (the map's alpha), dry orange to wet blue, over its art.
	if (material.tint.a < 0.5)
	{
		ivec2 here = clamp(ivec2(floor(p)), ivec2(0), mapSize - 1);
		float wet = texelFetch(emissiveTexture, ivec2(here.x, mapSize.y - 1 - here.y), 0).a;
		albedo = mix(albedo, mix(vec3(0.85, 0.55, 0.25), vec3(0.15, 0.45, 1.0), wet), 0.6);
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
