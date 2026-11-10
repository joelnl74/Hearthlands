// Tools/Tests/GameRulesTest.cpp
// The game's rules (Game/src/Rules) on fixed numbers: no scene, no data files, every random draw given. Built into
// Hydro's DataRoundTripTest by scripts/test_data.sh (bash scripts/check.sh --tests). One section per rules file.

#include "hypch.h"

#include "Rules/BrushRules.h"
#include "Rules/CameraRules.h"
#include "Rules/GrowthRules.h"
#include "Rules/ObjectRules.h"
#include "Rules/TerrainGenRules.h"
#include "Rules/WaterRules.h"
#include "State/Random.h"
#include "State/SimulationClock.h"
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

		// Counts and the change log.
		TileMap counted(3, 2, 1);
		RULE_CHECK(counted.GetCount(1) == 6 && counted.GetCount(2) == 0 && counted.TakeChanges().empty());
		counted.Set(0, 0, 2);
		counted.Set(2, 1, 2);
		counted.Set(2, 1, 2); // no change, not logged
		RULE_CHECK(counted.GetCount(1) == 4 && counted.GetCount(2) == 2);
		const std::vector<uint32_t> changes = counted.TakeChanges();
		RULE_CHECK(changes.size() == 2 && changes[0] == 0 && changes[1] == 5 && counted.TakeChanges().empty());
		TileMap other2(2, 2, 3);
		counted.Replace(other2);
		RULE_CHECK(counted.GetCount(3) == 4 && counted.GetCount(1) == 0 && counted.TakeChanges().empty());

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

		// Brush sizes step through Radii and stay in it; the brush tile falls back to the first unlocked one.
		RULE_CHECK(BrushRules::StepRadius(1.5f, 1) == 2.0f && BrushRules::StepRadius(1.5f, -1) == 1.0f);
		RULE_CHECK(BrushRules::StepRadius(8.0f, 1) == 8.0f && BrushRules::StepRadius(0.5f, -3) == 0.5f);
		RULE_CHECK(BrushRules::StepRadius(2.5f, 1) == 3.0f);
		const std::vector<uint8_t> open = { 1, 2 };
		RULE_CHECK(BrushRules::BrushTile(2, open) == 2 && BrushRules::BrushTile(4, open) == 1 && BrushRules::BrushTile(4, {}) == 0);

		// Digging never drains water; painting the same tile does nothing.
		RULE_CHECK(BrushRules::CanPaint(3, 5, 3, 5) == false && BrushRules::CanPaint(1, 5, 3, 5) && BrushRules::CanPaint(3, 1, 3, 5));
		RULE_CHECK(BrushRules::CanPaint(1, 1, 3, 5) == false);

		// Starting brushes: the StartsUnlocked types, ascending, each once.
		std::vector<TerrainTypeDefinition> types = TestTypes();
		RULE_CHECK(BrushRules::StartingBrushes(types).empty());
		types[2].StartsUnlocked = true; // grass 2
		types[1].StartsUnlocked = true; // dirt 1
		types.push_back(types[1]);      // a duplicate id
		const std::vector<uint8_t> brushes = BrushRules::StartingBrushes(types);
		RULE_CHECK(brushes.size() == 2 && brushes[0] == 1 && brushes[1] == 2);
	}

	static void TestSimulationClock(int& checks)
	{
		SimulationClock clock;
		RULE_CHECK(clock.GetDay() == 1 && clock.Speed == 1);
		clock.Seconds = SimulationClock::SecondsPerDay * 2.5;
		RULE_CHECK(clock.GetDay() == 3);
	}

	// A strip map: sea (3) in column 0, dirt (1) elsewhere; dig a riverbed (5) channel `width` rows tall from the sea.
	static TileMap ChannelMap(int length, int width, int height)
	{
		TileMap map(length + 1, height, 1);
		for (int y = 0; y < height; y++)
		{
			map.Set(0, y, 3);
		}
		const int top = (height - width) / 2;
		for (int y = top; y < top + width; y++)
		{
			for (int x = 1; x <= length; x++)
			{
				map.Set(x, y, 5);
			}
		}
		map.TakeChanges();
		return map;
	}

	// How many tiles along the channel's middle row hold water after `ticks` ticks.
	static int Flood(TileMap& map, Hydrology& water, const std::vector<uint8_t>& land, const WaterRules::Settings& settings, int ticks)
	{
		for (int x = 1; x < map.GetWidth(); x++)
		{
			for (int y = 0; y < map.GetHeight(); y++)
			{
				water.Activate((uint32_t)(y * map.GetWidth() + x));
			}
		}
		for (int i = 0; i < ticks; i++)
		{
			WaterRules::Step(map, water, land, settings);
		}
		int reach = 0;
		while (reach + 1 < map.GetWidth() && map.Get(reach + 1, map.GetHeight() / 2) == 3)
		{
			reach++;
		}
		return reach;
	}

	static void TestWaterRules(int& checks)
	{
		WaterRules::Settings settings; // water 3, riverbed 5, head 24, loss 1, reach 6
		RULE_CHECK(WaterRules::FromTypes({}, settings).WaterTileId == 3);
		std::vector<TerrainTypeDefinition> roles(2);
		roles[0].TileId = 7;
		roles[0].Role = TerrainRole::Water;
		roles[1].TileId = 8;
		roles[1].Role = TerrainRole::Riverbed;
		RULE_CHECK(WaterRules::FromTypes(roles, settings).WaterTileId == 7 && WaterRules::FromTypes(roles, settings).BedTileId == 8);

		// A narrow channel fills about SourceHead tiles; a wide one reaches further. Water moves a tile per tick.
		TileMap narrow = ChannelMap(60, 1, 5);
		std::vector<uint8_t> land(narrow.GetTiles().size(), 1);
		for (int y = 0; y < 5; y++)
		{
			land[(size_t)y * 61] = 0; // column 0 is the sea
		}
		Hydrology narrowWater = WaterRules::Start(narrow, land, settings);
		RULE_CHECK(narrowWater.Source[0] == 1 && narrowWater.Head[0] == 24.0f && narrowWater.Moisture[0] == 0); // salt: dry
		RULE_CHECK(Flood(narrow, narrowWater, land, settings, 3) <= 4);
		const int narrowReach = Flood(narrow, narrowWater, land, settings, 100);
		RULE_CHECK(narrowReach >= 20 && narrowReach <= 24);

		TileMap wide = ChannelMap(60, 3, 7);
		std::vector<uint8_t> wideLand(wide.GetTiles().size(), 1);
		for (int y = 0; y < 7; y++)
		{
			wideLand[(size_t)y * 61] = 0;
		}
		Hydrology wideWater = WaterRules::Start(wide, wideLand, settings);
		const int wideReach = Flood(wide, wideWater, wideLand, settings, 150);
		RULE_CHECK(wideReach > narrowReach + 10);
		std::printf("  Water: narrow channel %d tiles, three wide %d tiles\n", narrowReach, wideReach);

		// River water wets the land around it, fading over MoistureReach; the dirt far away stays dry.
		RULE_CHECK(narrowWater.Moisture[(size_t)2 * 61 + 5] == 255);
		RULE_CHECK(narrowWater.Moisture[(size_t)1 * 61 + 5] > 150 && narrowWater.Moisture[(size_t)1 * 61 + 5] < 255);
		RULE_CHECK(narrowWater.Moisture[(size_t)2 * 61 + 59] == 0);

		// Cutting the river with dirt drains the water behind the cut back to riverbed, and the land dries.
		narrow.Set(2, 2, 1);
		for (int i = 0; i < 200; i++)
		{
			WaterRules::Step(narrow, narrowWater, land, settings);
		}
		RULE_CHECK(narrow.Get(1, 2) == 3 && narrow.Get(3, 2) == 5 && narrow.Get(10, 2) == 5);
		RULE_CHECK(narrowWater.Moisture[(size_t)1 * 61 + 10] == 0);

		// A generated lake on land is a source and wets its shore.
		TileMap lake(9, 9, 1);
		lake.Set(4, 4, 3);
		const Hydrology lakeWater = WaterRules::Start(lake, std::vector<uint8_t>(81, 1), settings);
		RULE_CHECK(lakeWater.Source[40] == 1 && lakeWater.Moisture[40] == 255 && lakeWater.Moisture[41] == 255 - 42);
	}

	static void TestGrowthRules(int& checks)
	{
		// Random: repeatable, in range.
		Random a(7);
		Random b(7);
		bool same = true;
		bool inRange = true;
		for (int i = 0; i < 1000; i++)
		{
			const float value = a.Float();
			same = same && value == b.Float();
			inRange = inRange && value >= 0.0f && value < 1.0f && a.Below(10) < 10;
			b.Below(10);
		}
		RULE_CHECK(same && inRange);

		TerrainRuleDefinition spread;
		spread.FromTileId = 1;
		spread.ToTileId = 2;
		spread.MinMoisture = 0.2f;
		spread.NeighbourTileId = 2;
		spread.Chance = 1.0f;
		TileMap map(5, 5, 1);
		map.Set(2, 2, 2);
		Hydrology water;
		water.Moisture.assign(25, 128);
		RULE_CHECK(GrowthRules::Matches(spread, map, water, 1, 1) && GrowthRules::Matches(spread, map, water, 0, 0) == false);
		water.Moisture[6] = 10;
		RULE_CHECK(GrowthRules::Matches(spread, map, water, 1, 1) == false); // too dry
		RULE_CHECK(GrowthRules::CountNeighbours(map, 1, 1, 2) == 1 && GrowthRules::CountNeighbours(map, 2, 2, 1) == 8);

		// With Chance 1, sampling enough tiles turns every matching neighbour; never the sea or a dry tile.
		water.Moisture.assign(25, 128);
		water.Moisture[6] = 10;
		std::vector<uint8_t> land(25, 1);
		land[8] = 0;
		Random random(3);
		const int grown = GrowthRules::Step(map, water, land, { spread }, 2000, random);
		RULE_CHECK(grown > 0 && map.Get(3, 2) == 2 && map.Get(1, 1) == 1 && map.Get(3, 1) == 1);
		RULE_CHECK(map.Get(0, 0) == 2); // spread on from the grown ring

		// Unlocks: a locked type opens once the map holds enough of its UnlockTileId; open ones aren't repeated.
		std::vector<TerrainTypeDefinition> types(2);
		types[0].TileId = 2;
		types[0].UnlockTileId = 2;
		types[0].UnlockCount = 10;
		types[1].TileId = 4;
		TileMap few(3, 3, 2);
		RULE_CHECK(GrowthRules::NewUnlocks(types, { 1 }, few).empty());
		TileMap many(4, 4, 2);
		RULE_CHECK(GrowthRules::NewUnlocks(types, { 1 }, many).size() == 1 && GrowthRules::NewUnlocks(types, { 1, 2 }, many).empty());

		// Ticks due: whole ticks of world time not yet run, capped.
		RULE_CHECK(GrowthRules::TicksDue(1.0, 0, 4.0f, 8) == 4 && GrowthRules::TicksDue(1.0, 3, 4.0f, 8) == 1);
		RULE_CHECK(GrowthRules::TicksDue(10.0, 0, 4.0f, 8) == 8 && GrowthRules::TicksDue(0.2, 0, 4.0f, 8) == 0);
	}

	static void TestCameraRules(int& checks)
	{
		// Looking straight down (elevation 90) the camera sits above the target; isometric it sits back along -Y+X and up.
		const glm::vec3 above = CameraRules::Position(glm::vec2(3.0f, 4.0f), CameraRules::Rotation(0.0f, 90.0f), 10.0f);
		RULE_CHECK(glm::length(above - glm::vec3(3.0f, 4.0f, 10.0f)) < 1e-4f);
		const glm::quat iso = CameraRules::Rotation(45.0f, 30.0f);
		const glm::vec3 back = CameraRules::Position(glm::vec2(0.0f), iso, 1.0f);
		RULE_CHECK(std::abs(back.z - 0.5f) < 1e-4f && back.x > 0.0f && back.y < 0.0f && std::abs(back.x + back.y) < 1e-4f);
		const glm::vec3 forward = iso * glm::vec3(0.0f, 0.0f, -1.0f); // the camera looks down at 30 degrees
		RULE_CHECK(std::abs(std::asin(-forward.z) - glm::radians(30.0f)) < 1e-4f);

		// Zoom: whole steps within the table; the orthographic size shows that many art pixels a screen pixel.
		RULE_CHECK(CameraRules::StepZoom(3, 1) == 4 && CameraRules::StepZoom(5, 1) == 5 && CameraRules::StepZoom(0, -2) == 0);
		RULE_CHECK(std::abs(CameraRules::OrthographicSize(1080.0f, 2.0f, 24.0f) - 22.5f) < 1e-4f);

		// A drag: screen right moves along the camera's right, screen up along its ground forward, stretched by 1 / sin(30) = 2.
		const glm::vec2 right = CameraRules::GroundDelta(glm::vec2(10.0f, 0.0f), 10.0f, 0.0f, 30.0f);
		RULE_CHECK(glm::length(right - glm::vec2(1.0f, 0.0f)) < 1e-4f);
		const glm::vec2 up = CameraRules::GroundDelta(glm::vec2(0.0f, -10.0f), 10.0f, 0.0f, 30.0f);
		RULE_CHECK(glm::length(up - glm::vec2(0.0f, 2.0f)) < 1e-4f);
		const glm::vec2 turned = CameraRules::GroundDelta(glm::vec2(10.0f, 0.0f), 10.0f, 90.0f, 30.0f);
		RULE_CHECK(glm::length(turned - glm::vec2(0.0f, 1.0f)) < 1e-4f);

		RULE_CHECK(CameraRules::ClampTarget(glm::vec2(300.0f, -10.0f), 512, 512) == glm::vec2(256.0f, -10.0f));
		RULE_CHECK(CameraRules::ClampTarget(glm::vec2(-5.0f, -40.0f), 64, 32) == glm::vec2(-5.0f, -16.0f));
	}

	static ObjectDefinition MakeObject(int id, int tileId, float sprout, int growsInto, float growDays)
	{
		ObjectDefinition object;
		object.ObjectId = id;
		object.GrowsOnTileId = tileId;
		object.SproutChance = sprout;
		object.GrowsIntoObjectId = growsInto;
		object.GrowDays = growDays;
		return object;
	}

	static void TestObjectRules(int& checks)
	{
		// A grass field (2) with a dirt column (1) and a water tile (3); everything moist.
		TileMap map(12, 12, 2);
		for (int y = 0; y < 12; y++)
		{
			map.Set(0, y, 1);
		}
		map.Set(11, 11, 3);
		Hydrology water;
		water.Moisture.assign(144, 200);
		std::vector<uint8_t> land(144, 1);

		std::vector<ObjectDefinition> objects = { MakeObject(1, 2, 1.0f, 2, 1.0f), MakeObject(2, 2, 0.0f, -1, 0.0f),
			MakeObject(3, -1, 0.0f, -1, 0.0f) };
		objects[0].MinMoisture = 0.5f;
		const std::vector<int> byId = ObjectRules::IndexById(objects);
		RULE_CHECK(byId[1] == 0 && byId[3] == 2 && byId[4] == -1);

		// Fits: the tile, the moisture, any land for -1 (not water).
		RULE_CHECK(ObjectRules::Fits(objects[0], map, water, 3, 3) && ObjectRules::Fits(objects[0], map, water, 0, 3) == false);
		RULE_CHECK(ObjectRules::Fits(objects[2], map, water, 0, 3) && ObjectRules::Fits(objects[2], map, water, 11, 11) == false);
		water.Moisture[(size_t)3 * 12 + 3] = 50;
		RULE_CHECK(ObjectRules::Fits(objects[0], map, water, 3, 3) == false);
		water.Moisture[(size_t)3 * 12 + 3] = 200;

		// Sprouting with chance 1 fills the grass but not the dirt; with MinSpacing 1 no two touch.
		ObjectLayer layer;
		layer.Reset(144);
		Random random(5);
		objects[0].MinSpacing = 1;
		ObjectRules::Step(layer, map, water, land, objects, byId, 3000, 0.0f, random);
		bool spaced = true;
		bool onGrass = true;
		for (int y = 0; y < 12; y++)
		{
			for (int x = 0; x < 12; x++)
			{
				const size_t index = (size_t)y * 12 + (size_t)x;
				onGrass = onGrass && (layer.Get(index) == 0 || map.Get(x, y) == 2);
				spaced = spaced && (layer.Get(index) == 0 || ObjectRules::CountObjects(layer, map, x, y, 0, 1) == 0);
			}
		}
		RULE_CHECK(layer.GetCount() > 10 && onGrass && spaced);

		// Growing: GrowDays 1 at 10 looks a day grows 1 in 10 looks; after many looks all have grown.
		objects[0].SproutChance = 0.0f;
		ObjectRules::Step(layer, map, water, land, objects, byId, 20000, 10.0f, random);
		bool grown = true;
		for (uint8_t id : layer.GetIds())
		{
			grown = grown && id != 1;
		}
		RULE_CHECK(grown && layer.GetCount() > 10);

		// Spreading: a lone object 2 spreads to tiles near it (SpreadFromObjectId) and nowhere else.
		ObjectLayer spread;
		spread.Reset(144);
		spread.Set((size_t)6 * 12 + 6, 2);
		ObjectDefinition seedling = MakeObject(4, 2, 0.0f, -1, 0.0f);
		seedling.SpreadFromObjectId = 2;
		seedling.SpreadChance = 1.0f;
		const std::vector<ObjectDefinition> pair = { seedling, MakeObject(2, 2, 0.0f, -1, 0.0f) };
		ObjectRules::Step(spread, map, water, land, pair, ObjectRules::IndexById(pair), 3000, 0.0f, random);
		RULE_CHECK(spread.Get((size_t)6 * 12 + 7) == 4 && spread.Get((size_t)2 * 12 + 2) == 0);

		// Dying: a dug tile loses its object at once (RemoveUnfit); a dried one in Step.
		const size_t tree = (size_t)6 * 12 + 6;
		map.Set(6, 6, 5);
		RULE_CHECK(ObjectRules::RemoveUnfit(spread, map, water, pair, ObjectRules::IndexById(pair), { (uint32_t)tree }) == 1);
		RULE_CHECK(spread.Get(tree) == 0);
		ObjectLayer dry;
		dry.Reset(144);
		dry.Set((size_t)3 * 12 + 3, 1);
		water.Moisture.assign(144, 10);
		ObjectRules::Step(dry, map, water, land, objects, byId, 5000, 0.0f, random);
		RULE_CHECK(dry.GetCount() == 0);

		// Scatter: StartDensity 1 places on every land tile that fits.
		ObjectLayer rocks;
		rocks.Reset(144);
		ObjectDefinition rock = MakeObject(3, -1, 0.0f, -1, 0.0f);
		rock.StartDensity = 1.0f;
		RULE_CHECK(ObjectRules::Scatter(rocks, map, water, { rock }, random) == 142 && rocks.Get(143) == 0);
	}

	int TestGameRules(int& checks)
	{
		std::printf("Game rules: TileMap, TerrainGenRules, BrushRules, CameraRules, SimulationClock, WaterRules, GrowthRules, ObjectRules\n");
		TestTileMap(checks);
		TestTerrainGenRules(checks);
		TestBrushRules(checks);
		TestCameraRules(checks);
		TestSimulationClock(checks);
		TestWaterRules(checks);
		TestGrowthRules(checks);
		TestObjectRules(checks);

		return s_RuleFailures;
	}
}
