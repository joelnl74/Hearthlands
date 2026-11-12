// Tools/Tests/ForestRulesTest.cpp
// Forests away from rivers (ForestRules: roots, shade, saplings) in GrowthRules and ObjectRules; run from TestGameRules.

#include "hypch.h"

#include "Rules/ForestRules.h"
#include "Rules/GrowthRules.h"
#include "Rules/ObjectRules.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace Hydro
{
	static int s_ForestFailures = 0;

	static void ForestCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_ForestFailures++;
			std::printf("FAIL  ForestRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define FOREST_CHECK(condition) ForestCheck((condition), #condition, __LINE__, checks)

	static bool ForestNear(float a, float b, float within)
	{
		return std::fabs(a - b) <= within;
	}

	static ObjectDefinition MakeForestTree(int id, float roots, float canopy, float minMoisture)
	{
		ObjectDefinition tree;
		tree.ObjectId = id;
		tree.Kind = ObjectKind::Tree;
		tree.GrowsOnTileId = 2;
		tree.Roots = roots;
		tree.Canopy = canopy;
		tree.MinMoisture = minMoisture;
		tree.MaxMoisture = 1.0f;
		return tree;
	}

	// On a dirt map (16 x 16) a 3 x 3 grass patch at (6..8, 6..8); dry, all groundwater at `groundwater`.
	static void DryForestMap(TileMap& map, Hydrology& water, float groundwater)
	{
		for (int y = 6; y <= 8; y++)
		{
			for (int x = 6; x <= 8; x++)
			{
				map.Set(x, y, 2);
			}
		}
		water.Moisture.assign(256, 0);
		water.Groundwater.assign(256, (uint8_t)(groundwater * 255.0f + 0.5f));
	}

	int TestForestRules(int& checks)
	{
		std::printf("Game rules: ForestRules\n");

		// Rooted: the ground's moisture, or Roots x groundwater if more.
		TileMap map(16, 16, 1);
		Hydrology water;
		DryForestMap(map, water, 0.8f);
		water.Moisture[0] = 26; // 0.1
		const ObjectDefinition pine = MakeForestTree(1, 0.5f, 1.0f, 0.15f);
		ObjectDefinition bush = MakeForestTree(2, 0.0f, 0.0f, 0.25f);
		bush.Kind = ObjectKind::Shrub;
		FOREST_CHECK(ForestNear(ForestRules::Rooted(pine, water, 0), 0.4f, 0.01f));
		FOREST_CHECK(ForestNear(ForestRules::Rooted(bush, water, 0), 0.1f, 0.01f));
		FOREST_CHECK(ForestRules::Rooted(pine, Hydrology{}, 0) == 0.0f); // no water state: dry

		// ShadeFactor: shade lovers more under trees, pioneers more in the open, never below 0.
		FOREST_CHECK(ForestNear(ForestRules::ShadeFactor(0.6f, 0.0f), 0.4f, 1e-5f));
		FOREST_CHECK(ForestNear(ForestRules::ShadeFactor(0.6f, 1.0f), 1.6f, 1e-5f));
		FOREST_CHECK(ForestNear(ForestRules::ShadeFactor(-0.5f, 1.0f), 0.5f, 1e-5f) && ForestRules::ShadeFactor(-1.0f, 1.0f) == 0.0f);
		FOREST_CHECK(ForestNear(ForestRules::ShadeFactor(0.6f, 0.5f), 1.0f, 1e-5f));

		// An empty canopy shades nothing.
		ForestRules::Canopy none;
		FOREST_CHECK(none.IsEmpty() && none.Cover(map, 5, 5) == 0.0f && none.Shade(map, water, 5, 5) == 0.0f);

		// Three pines on the patch's left column (FullCover 4): the tile beside them (5, 7) is 3/4 covered; it keeps
		// 0.9 x 0.75 x their moisture (0.4 from the groundwater) = 0.27; so does (4, 7), Reach 2 off; three off nothing.
		const std::vector<ObjectDefinition> objects = { pine, bush };
		const std::vector<int> byId = ObjectRules::IndexById(objects);
		ObjectLayer layer;
		layer.Reset(256);
		for (int y = 6; y <= 8; y++)
		{
			layer.Set((size_t)y * 16 + 6, 1);
		}
		layer.Set(7 * 16 + 4, 2); // a bush: no canopy
		ForestRules::Canopy canopy;
		canopy.Forest.FullCover = 4.0f;
		canopy.Layer = &layer;
		canopy.Objects = &objects;
		canopy.ById = &byId;
		FOREST_CHECK(canopy.IsEmpty() == false);
		FOREST_CHECK(ForestNear(canopy.Cover(map, 5, 7), 0.75f, 1e-5f) && ForestNear(canopy.Cover(map, 5, 5), 0.5f, 1e-5f));
		FOREST_CHECK(ForestNear(canopy.Shade(map, water, 5, 7), 0.27f, 0.01f) && ForestNear(canopy.Shade(map, water, 4, 7), 0.27f, 0.01f));
		FOREST_CHECK(canopy.Shade(map, water, 3, 7) == 0.0f && canopy.Cover(map, 6, 3) == 0.0f);
		canopy.Forest.Reach = 1; // only the 3 x 3 tiles
		FOREST_CHECK(canopy.Shade(map, water, 4, 7) == 0.0f && ForestNear(canopy.Cover(map, 5, 5), 0.25f, 1e-5f));
		canopy.Forest.FullCover = 2.0f; // a smaller FullCover covers fully
		FOREST_CHECK(ForestNear(canopy.Cover(map, 5, 7), 1.0f, 1e-5f) && ForestNear(canopy.Shade(map, water, 5, 7), 0.36f, 0.01f));
		canopy.Forest = ForestRules::Settings{};
		canopy.Forest.FullCover = 4.0f;

		// Fits: a pine sapling on dry grass lives from its roots where the groundwater is high, not where it is low;
		// the canopy's shade lets it live there too.
		const ObjectDefinition sapling = MakeForestTree(3, 0.45f, 0.2f, 0.15f);
		FOREST_CHECK(ObjectRules::Fits(sapling, map, water, 7, 7));
		Hydrology dry = water;
		dry.Groundwater.assign(256, 51); // 0.2: 0.09 from the roots
		FOREST_CHECK(ObjectRules::Fits(sapling, map, dry, 7, 7) == false && ObjectRules::Fits(sapling, map, dry, 7, 7, nullptr, 0.2f));
		FOREST_CHECK(ObjectRules::Fits(bush, map, water, 7, 7) == false && ObjectRules::Fits(bush, map, water, 7, 7, nullptr, 0.3f));

		// Grass: spreads onto the dry dirt beside the forest in its shade, and doesn't dry under it.
		TerrainRuleDefinition spreads;
		spreads.FromTileId = 1;
		spreads.ToTileId = 2;
		spreads.MinMoisture = 0.2f;
		spreads.NeighbourTileId = 2;
		spreads.Chance = 1.0f;
		TerrainRuleDefinition dries;
		dries.FromTileId = 2;
		dries.ToTileId = 1;
		dries.MinMoisture = 0.0f;
		dries.MaxMoisture = 0.05f;
		dries.Chance = 1.0f;
		FOREST_CHECK(GrowthRules::Matches(spreads, map, water, 5, 7) == false);
		FOREST_CHECK(GrowthRules::Matches(spreads, map, water, 5, 7, canopy.Shade(map, water, 5, 7)));
		FOREST_CHECK(GrowthRules::Matches(dries, map, water, 7, 7) && GrowthRules::Matches(dries, map, water, 7, 7, 0.2f) == false);

		// The types: grass (2) living ground whose roots reach 0.1 of the groundwater, dirt (1) bare.
		TerrainTypeDefinition dirtType;
		dirtType.TileId = 1;
		TerrainTypeDefinition grassType;
		grassType.TileId = 2;
		grassType.Living = true;
		grassType.Roots = 0.1f;
		canopy.Forest = ForestRules::FromTypes({ dirtType, grassType }, canopy.Forest);
		FOREST_CHECK(canopy.Forest.Living[2] == 1 && canopy.Forest.Living[1] == 0 && canopy.Forest.Roots[2] == 0.1f);
		FOREST_CHECK(canopy.Forest.FullCover == 4.0f); // the rest kept

		// Ground: grass gets its roots' share of the groundwater and the shade; bare dirt neither (without ShadeBare).
		FOREST_CHECK(ForestNear(canopy.Ground(map, water, 8, 8), 0.27f, 0.01f)); // grass in the shade
		FOREST_CHECK(canopy.Ground(map, water, 5, 7) == 0.0f);                   // dirt beside the trees
		map.Set(12, 12, 2);
		FOREST_CHECK(ForestNear(canopy.Ground(map, water, 12, 12), 0.08f, 0.005f)); // grass away from trees: roots
		map.Set(12, 12, 1);
		canopy.Forest.ShadeBare = true;
		FOREST_CHECK(ForestNear(canopy.Ground(map, water, 5, 7), 0.27f, 0.01f));
		canopy.Forest.ShadeBare = false;

		// Step with the canopy: the forest's grass holds; grass the player paints over groundwater holds by its roots,
		// over poor groundwater it dries; nothing creeps onto the bare dirt.
		{
			TileMap grown(16, 16, 1);
			Hydrology wet;
			DryForestMap(grown, wet, 0.8f);
			grown.Set(12, 7, 2); // painted grass, no trees near: 0.08 from its roots
			grown.Set(13, 13, 2);
			wet.Groundwater[13 * 16 + 13] = 77; // 0.3: 0.03, it dries
			Random random(5);
			GrowthRules::Step(grown, wet, {}, { spreads, dries }, 6000, random, {}, canopy);
			FOREST_CHECK(grown.Get(6, 7) == 2 && grown.Get(8, 7) == 2); // under and near the trees
			FOREST_CHECK(grown.Get(12, 7) == 2 && grown.Get(13, 13) == 1);
			FOREST_CHECK(grown.Get(5, 7) == 1 && grown.Get(14, 14) == 1);

			// With Forest edge (ShadeBare) the forest's grass creeps out up to Reach from its trees.
			canopy.Forest.ShadeBare = true;
			GrowthRules::Step(grown, wet, {}, { spreads, dries }, 6000, random, {}, canopy);
			FOREST_CHECK(grown.Get(5, 7) == 2 && grown.Get(4, 7) == 2 && grown.Get(3, 7) == 1);
			canopy.Forest.ShadeBare = false;
		}

		// The same forest over poor groundwater, even with ShadeBare: its trees hold little, the grass can't spread.
		{
			TileMap grown(16, 16, 1);
			Hydrology poor;
			DryForestMap(grown, poor, 0.3f); // the pines' 0.15 kept at 0.9 x 0.75: 0.1
			Random random(5);
			canopy.Forest.ShadeBare = true;
			GrowthRules::Step(grown, poor, {}, { spreads }, 6000, random, {}, canopy);
			FOREST_CHECK(grown.Get(5, 7) == 1);
			canopy.Forest.ShadeBare = false;
		}

		// ObjectRules::Step: saplings sprout on the grass the groundwater feeds, the shade-loving more under trees.
		{
			TileMap field(16, 16, 2);
			Hydrology deep;
			deep.Moisture.assign(256, 0);
			deep.Groundwater.assign(256, 204); // 0.8: a sapling's roots get 0.36
			ObjectDefinition oak = MakeForestTree(4, 0.45f, 0.2f, 0.3f);
			oak.SproutChance = 0.05f;
			oak.ShadeEffect = 1.0f; // none in the open, twice under a closed canopy
			oak.Canopy = 0.0f;      // its saplings shade nothing, so the open stays open
			const std::vector<ObjectDefinition> rows = { pine, oak };
			const std::vector<int> ids = ObjectRules::IndexById(rows);
			ObjectLayer woods;
			woods.Reset(256);
			for (int y = 0; y < 16; y += 2)
			{
				for (int x = 0; x < 6; x += 2)
				{
					woods.Set((size_t)y * 16 + (size_t)x, 1); // pines on the left: cover there
				}
			}
			Random random(9);
			ObjectRules::Step(woods, field, deep, {}, rows, ids, 20000, 1.0f, random);
			int shaded = 0;
			int open = 0;
			for (uint32_t i = 0; i < 256; i++)
			{
				const int x = (int)(i % 16);
				shaded += woods.Get(i) == 4 && x < 6 ? 1 : 0;
				open += woods.Get(i) == 4 && x >= 9 ? 1 : 0;
			}
			FOREST_CHECK(shaded > 0 && open == 0);

			// Over dry ground and poor groundwater nothing comes up.
			Hydrology poor = deep;
			poor.Groundwater.assign(256, 51);
			ObjectLayer bare;
			bare.Reset(256);
			oak.ShadeEffect = 0.0f;
			const std::vector<ObjectDefinition> plain = { oak };
			ObjectRules::Step(bare, field, poor, {}, plain, ObjectRules::IndexById(plain), 20000, 1.0f, random);
			FOREST_CHECK(bare.GetCount() == 0);
		}

		return s_ForestFailures;
	}
}
