// Tools/Tests/GameRulesTest.cpp
// The game's rules (Game/src/Rules) on fixed numbers: no scene, no data files, every random draw given. Built into
// Hydro's DataRoundTripTest by scripts/test_data.sh (bash scripts/check.sh --tests). One section per rules file.

#include "hypch.h"

#include "Rules/BrushRules.h"
#include "Rules/CameraRules.h"
#include "Rules/TerrainGenRules.h"
#include "State/TileMap.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace Hydro
{
	static int s_RuleFailures = 0;

	static void RuleCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_RuleFailures++;
			std::printf("FAIL  GameRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define RULE_CHECK(condition) RuleCheck((condition), #condition, __LINE__, checks)

	static TerrainTypeDefinition MakeType(int tileId, int priority, float minHeight, float maxHeight)
	{
		TerrainTypeDefinition type;
		type.TileId = tileId;
		type.Priority = priority;
		type.MinHeight = minHeight;
		type.MaxHeight = maxHeight;
		return type;
	}

	// The four types of Assets/Data/TerrainTypes.hytable, by height only.
	static std::vector<TerrainTypeDefinition> TestTypes()
	{
		return { MakeType(0, 1, 0.35f, 0.45f), MakeType(1, 2, 0.45f, 0.55f), MakeType(2, 3, 0.55f, 1.0f), MakeType(3, 10, 0.0f, 0.35f) };
	}

	static void TestTileMap(int& checks)
	{
		TileMap map(4, 3, 2);
		RULE_CHECK(map.GetWidth() == 4 && map.GetHeight() == 3 && map.GetTiles().size() == 12);
		RULE_CHECK(map.Get(3, 2) == 2);
		RULE_CHECK(map.InBounds(3, 2) && map.InBounds(4, 0) == false && map.InBounds(0, -1) == false);

		const uint64_t start = map.GetRevision();
		RULE_CHECK(map.Set(1, 2, 3) && map.Get(1, 2) == 3 && map.GetTiles()[9] == 3);
		RULE_CHECK(map.GetRevision() == start + 1);
		RULE_CHECK(map.Set(1, 2, 3) == false && map.GetRevision() == start + 1); // same id: no change
		RULE_CHECK(map.Set(9, 9, 1) == false && map.GetRevision() == start + 1); // out of bounds

		TileMap other(2, 2, 1);
		other.Set(0, 0, 0);
		other.Set(1, 0, 0);
		other.Set(0, 1, 0);
		other.Set(1, 1, 0);
		other.Set(0, 0, 1); // revision 5, past map's
		const uint64_t before = map.GetRevision() > other.GetRevision() ? map.GetRevision() : other.GetRevision();
		map.Replace(other);
		RULE_CHECK(map.GetWidth() == 2 && map.GetHeight() == 2 && map.Get(0, 0) == 1 && map.Get(1, 1) == 0);
		RULE_CHECK(map.GetRevision() > before);

		TileMap empty(-3, 5);
		RULE_CHECK(empty.GetWidth() == 0 && empty.GetTiles().empty() && empty.InBounds(0, 0) == false);
	}

	static void TestTerrainGenRules(int& checks)
	{
		const std::vector<TerrainTypeDefinition> types = TestTypes();

		// Picking a type: by range, then priority on overlap, then the nearest range.
		RULE_CHECK(TerrainGenRules::PickType(0.1f, 0.5f, types) == 3);
		RULE_CHECK(TerrainGenRules::PickType(0.4f, 0.5f, types) == 0);
		RULE_CHECK(TerrainGenRules::PickType(0.5f, 0.5f, types) == 1);
		RULE_CHECK(TerrainGenRules::PickType(0.9f, 0.5f, types) == 2);
		RULE_CHECK(TerrainGenRules::PickType(0.35f, 0.5f, types) == 3); // water and sand both hold 0.35: water's priority wins
		std::vector<TerrainTypeDefinition> gap = { MakeType(5, 0, 0.0f, 0.2f), MakeType(6, 0, 0.8f, 1.0f) };
		RULE_CHECK(TerrainGenRules::PickType(0.3f, 0.5f, gap) == 5);
		RULE_CHECK(TerrainGenRules::PickType(0.7f, 0.5f, gap) == 6);
		gap[0].Generated = false;
		RULE_CHECK(TerrainGenRules::PickType(0.1f, 0.5f, gap) == 6); // painted-only types are never generated
		RULE_CHECK(TerrainGenRules::PickType(0.5f, 0.5f, {}) == 0);

		// Noise stays in 0 to 1 and depends on the seed.
		bool inRange = true;
		bool seedMatters = false;
		for (int y = -20; y < 20; y++)
		{
			for (int x = -20; x < 20; x++)
			{
				const float a = TerrainGenRules::Noise(x, y, 8.0f, 3, 7u);
				inRange = inRange && a >= 0.0f && a <= 1.0f;
				seedMatters = seedMatters || a != TerrainGenRules::Noise(x, y, 8.0f, 3, 8u);
			}
		}
		RULE_CHECK(inRange);
		RULE_CHECK(seedMatters);

		// The same settings give the same map; another seed another one; every tile is one of the types.
		TerrainGenRules::Settings settings;
		settings.Seed = 42;
		settings.Width = 48;
		settings.Height = 32;
		const TileMap first = TerrainGenRules::Generate(settings, types);
		const TileMap again = TerrainGenRules::Generate(settings, types);
		RULE_CHECK(first.GetWidth() == 48 && first.GetHeight() == 32);
		RULE_CHECK(first.GetTiles() == again.GetTiles());
		settings.Seed = 43;
		RULE_CHECK(TerrainGenRules::Generate(settings, types).GetTiles() != first.GetTiles());

		bool valid = true;
		int counts[4] = { 0, 0, 0, 0 };
		for (uint8_t tileId : first.GetTiles())
		{
			valid = valid && tileId <= 3;
			if (tileId <= 3)
			{
				counts[tileId]++;
			}
		}
		RULE_CHECK(valid);

		// The coast: corners and edges turn to sea, the middle stays, the same seed gives the same coast, 0 changes nothing.
		const TileMap land(40, 30, 1);
		const TileMap coast = TerrainGenRules::ShapeCoast(land, 4.0f, 9u, 3);
		RULE_CHECK(coast.Get(0, 0) == 3 && coast.Get(39, 29) == 3 && coast.Get(0, 15) == 3 && coast.Get(20, 0) == 3);
		RULE_CHECK(coast.Get(20, 15) == 1 && coast.Get(12, 15) == 1);
		RULE_CHECK(coast.GetTiles() == TerrainGenRules::ShapeCoast(land, 4.0f, 9u, 3).GetTiles());
		RULE_CHECK(TerrainGenRules::ShapeCoast(land, 0.0f, 9u, 3).GetTiles() == land.GetTiles());

		// The beach: land next to the sea turns to sand, the middle stays, the sea stays sea; 0 changes nothing.
		const TileMap beach = TerrainGenRules::AddBeach(coast, 3.0f, 9u, 3, 0);
		bool shoreIsBeach = true;
		bool seaStays = true;
		for (int y = 1; y < 29; y++)
		{
			for (int x = 1; x < 39; x++)
			{
				const bool nextToSea = coast.Get(x - 1, y) == 3 || coast.Get(x + 1, y) == 3
					|| coast.Get(x, y - 1) == 3 || coast.Get(x, y + 1) == 3;
				shoreIsBeach = shoreIsBeach && (coast.Get(x, y) == 3 || nextToSea == false || beach.Get(x, y) == 0);
				seaStays = seaStays && (coast.Get(x, y) != 3 || beach.Get(x, y) == 3);
			}
		}
		RULE_CHECK(shoreIsBeach && seaStays && beach.Get(20, 15) == 1);
		RULE_CHECK(TerrainGenRules::AddBeach(coast, 0.0f, 9u, 3, 0).GetTiles() == coast.GetTiles());

		// Land: the sea joined to the rim is not land, an inland lake is.
		TileMap lake = coast;
		lake.Set(20, 15, 3);
		const std::vector<uint8_t> landMask = TerrainGenRules::LandOf(lake, 3);
		RULE_CHECK(landMask[0] == 0 && landMask[(size_t)15 * 40 + 20] == 1 && landMask[(size_t)15 * 40 + 12] == 1);
		std::printf("  TerrainGen seed 42, 48 x 32: sand %d, dirt %d, grass %d, water %d\n", counts[0], counts[1], counts[2], counts[3]);
	}

	static void TestBrushRules(int& checks)
	{
		// The ray from above meets the ground; parallel or pointing away it doesn't.
		glm::vec3 hit(0.0f);
		RULE_CHECK(BrushRules::RayToGround(glm::vec3(1.0f, 2.0f, 10.0f), glm::vec3(0.5f, 0.0f, -1.0f), 0.0f, hit));
		RULE_CHECK(std::abs(hit.x - 6.0f) < 1e-4f && std::abs(hit.y - 2.0f) < 1e-4f && std::abs(hit.z) < 1e-4f);
		RULE_CHECK(BrushRules::RayToGround(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(1.0f, 0.0f, 0.0f), 0.0f, hit) == false);
		RULE_CHECK(BrushRules::RayToGround(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f, 0.0f, 1.0f), 0.0f, hit) == false);

		// Map points from the map's centre (4 x 2 map: x -2..2, y -1..1): x right, y down from the top-left corner.
		RULE_CHECK(BrushRules::MapPoint(glm::vec2(-2.0f, 1.0f), 4, 2) == glm::vec2(0.0f, 0.0f));
		RULE_CHECK(BrushRules::MapPoint(glm::vec2(1.5f, -0.5f), 4, 2) == glm::vec2(3.5f, 1.5f));
		RULE_CHECK(BrushRules::OnMap(glm::vec2(3.9f, 1.9f), 4, 2) && BrushRules::OnMap(glm::vec2(4.0f, 1.0f), 4, 2) == false);
		RULE_CHECK(BrushRules::OnMap(glm::vec2(1.0f, -0.1f), 4, 2) == false);

		// Strokes: both ends, at most the spacing apart.
		const std::vector<glm::vec2> stroke = BrushRules::Stroke(glm::vec2(0.0f, 0.0f), glm::vec2(3.0f, 4.0f), 0.5f);
		RULE_CHECK(stroke.size() == 11 && stroke.front() == glm::vec2(0.0f) && glm::length(stroke.back() - glm::vec2(3.0f, 4.0f)) < 1e-5f);
		RULE_CHECK(BrushRules::Stroke(glm::vec2(2.0f), glm::vec2(2.0f), 0.5f).size() == 1);

		// Stamps are round: radius 1.5 on a tile centre is 3 x 3; radius 2.5 is 5 x 5 without its 4 corners; a small radius
		// still paints the tile under the point; clipped at the map's edge.
		RULE_CHECK(BrushRules::Stamp(glm::vec2(5.5f, 5.5f), 1.5f, 10, 10).size() == 9);
		RULE_CHECK(BrushRules::Stamp(glm::vec2(5.5f, 5.5f), 2.5f, 10, 10).size() == 21);
		RULE_CHECK(BrushRules::Stamp(glm::vec2(5.2f, 5.9f), 0.1f, 10, 10).size() == 1);
		RULE_CHECK(BrushRules::Stamp(glm::vec2(0.5f, 0.5f), 1.5f, 10, 10).size() == 4);
		RULE_CHECK(BrushRules::Stamp(glm::vec2(-9.0f, 0.5f), 1.5f, 10, 10).empty());
		std::vector<uint8_t> seaRow(100, 1);
		for (int x = 0; x < 10; x++)
		{
			seaRow[(size_t)(50 + x)] = 0; // row 5 is sea
		}
		RULE_CHECK(BrushRules::Stamp(glm::vec2(5.5f, 5.5f), 1.5f, 10, 10, seaRow).size() == 6);
		RULE_CHECK(BrushRules::Stamp(glm::vec2(5.5f, 5.5f), 0.1f, 10, 10, seaRow).empty());

		// Starting brushes: the StartsUnlocked types, ascending, each once.
		std::vector<TerrainTypeDefinition> types = TestTypes();
		RULE_CHECK(BrushRules::StartingBrushes(types).empty());
		types[2].StartsUnlocked = true; // grass 2
		types[1].StartsUnlocked = true; // dirt 1
		types.push_back(types[1]);      // a duplicate id
		const std::vector<uint8_t> brushes = BrushRules::StartingBrushes(types);
		RULE_CHECK(brushes.size() == 2 && brushes[0] == 1 && brushes[1] == 2);
	}

	static void TestCameraRules(int& checks)
	{
		// The camera sits back along -Y and up, at the distance, looking at the target.
		const glm::vec3 position = CameraRules::Position(glm::vec2(3.0f, 4.0f), 10.0f, glm::radians(35.0f));
		RULE_CHECK(std::abs(glm::length(position - glm::vec3(3.0f, 4.0f, 0.0f)) - 10.0f) < 1e-4f);
		RULE_CHECK(position.x == 3.0f && position.y < 4.0f && position.z > 0.0f);
		RULE_CHECK(CameraRules::Position(glm::vec2(0.0f), 5.0f, 0.0f) == glm::vec3(0.0f, 0.0f, 5.0f));

		RULE_CHECK(CameraRules::ClampTarget(glm::vec2(300.0f, -10.0f), 512, 512) == glm::vec2(256.0f, -10.0f));
		RULE_CHECK(CameraRules::ClampTarget(glm::vec2(-5.0f, -40.0f), 64, 32) == glm::vec2(-5.0f, -16.0f));

		RULE_CHECK(CameraRules::Zoom(40.0f, 1.0f, 8.0f, 160.0f) < 40.0f && CameraRules::Zoom(40.0f, -1.0f, 8.0f, 160.0f) > 40.0f);
		RULE_CHECK(CameraRules::Zoom(10.0f, 50.0f, 8.0f, 160.0f) == 8.0f && CameraRules::Zoom(100.0f, -50.0f, 8.0f, 160.0f) == 160.0f);
		RULE_CHECK(std::abs(CameraRules::MetresPerPixel(10.0f, 90.0f, 100.0f) - 0.2f) < 1e-4f);
	}

	int TestGameRules(int& checks)
	{
		std::printf("Game rules: TileMap, TerrainGenRules, BrushRules, CameraRules\n");
		TestTileMap(checks);
		TestTerrainGenRules(checks);
		TestBrushRules(checks);
		TestCameraRules(checks);

		return s_RuleFailures;
	}
}
