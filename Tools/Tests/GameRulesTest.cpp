// Tools/Tests/GameRulesTest.cpp
// The game's rules (Game/src/Rules) on fixed numbers: no scene, no data files, every random draw given. Built into
// Hydro's DataRoundTripTest by scripts/test_data.sh (bash scripts/check.sh --tests). One section per rules file.

#include "hypch.h"

#include "Rules/BrushRules.h"
#include "Rules/CameraRules.h"
#include "Rules/GroundwaterRules.h"
#include "Rules/GrowthRules.h"
#include "Rules/HeightRules.h"
#include "Rules/MountainRules.h"
#include "Rules/ObjectRules.h"
#include "Rules/TerrainGenRules.h"
#include "Rules/WaterRules.h"
#include "State/HeightMap.h"
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

	// 13 x 11 dirt; with `wet`, column 0 is water; a 5 x 5 grass block over x 3..7, y 3..7 with a dirt hole at (5, 5).
	static TileMap ShelterMap(bool wet, bool grass)
	{
		TileMap map(13, 11, 1);
		for (int y = 0; y < 11 && wet; y++)
		{
			map.Set(0, y, 3);
		}
		for (int y = 3; y <= 7 && grass; y++)
		{
			for (int x = 3; x <= 7; x++)
			{
				map.Set(x, y, x == 5 && y == 5 ? 1 : 2);
			}
		}
		map.TakeChanges();
		return map;
	}

	static void TestShelter(int& checks, const WaterRules::Settings& plain)
	{
		// Grass (2) shelters with 0.15; a dirt hole walled in by wet grass holds their mean moisture plus that.
		std::vector<TerrainTypeDefinition> types(1);
		types[0].TileId = 2;
		types[0].Shelter = 0.15f;
		const WaterRules::Settings settings = WaterRules::FromTypes(types, plain);
		RULE_CHECK(settings.Shelter[2] == 38 && settings.Shelter[1] == 0 && plain.Shelter[2] == 0);
		const std::vector<uint8_t> land(13 * 11, 1);
		const size_t hole = (size_t)5 * 13 + 5;
		TileMap map = ShelterMap(true, true);
		const Hydrology water = WaterRules::Start(map, land, settings);
		RULE_CHECK(water.Carried[hole] == 255 - 5 * 42 && water.Moisture[hole] == (3 * 87 + 2 * 45 + 3 * 3) / 8 + 38);
		RULE_CHECK(water.Moisture[hole] >= 51 && water.Carried[hole] < 51); // wet enough for grass to spread in (0.2)
		RULE_CHECK(water.Moisture[(size_t)4 * 13 + 4] > water.Carried[(size_t)4 * 13 + 4]); // the grass inside too
		RULE_CHECK(water.Moisture[(size_t)5 * 13 + 8] == water.Carried[(size_t)5 * 13 + 8]); // the edge: 3 of 8, not walled in
		RULE_CHECK(WaterRules::Start(map, land, plain).Moisture[hole] == water.Carried[hole]); // nothing shelters by default

		// Dry grass walls in nothing: without water the hole stays dry.
		TileMap dryMap = ShelterMap(false, true);
		RULE_CHECK(WaterRules::Start(dryMap, land, settings).Moisture[hole] == 0);

		// Grass painted in later shelters the same after a few ticks; with the water gone everything dries (no loop holds it).
		TileMap painted = ShelterMap(true, false);
		Hydrology paintedWater = WaterRules::Start(painted, land, settings);
		for (int y = 3; y <= 7; y++)
		{
			for (int x = 3; x <= 7; x++)
			{
				painted.Set(x, y, x == 5 && y == 5 ? 1 : 2);
			}
		}
		for (int i = 0; i < 5; i++)
		{
			WaterRules::Step(painted, paintedWater, land, settings);
		}
		RULE_CHECK(paintedWater.Moisture == water.Moisture && paintedWater.Carried == water.Carried);
		for (int y = 0; y < 11; y++)
		{
			painted.Set(0, y, 1);
		}
		int wettest = 0;
		for (int i = 0; i < 40; i++)
		{
			WaterRules::Step(painted, paintedWater, land, settings);
		}
		for (uint8_t value : paintedWater.Moisture)
		{
			wettest = value > wettest ? value : wettest;
		}
		RULE_CHECK(wettest == 0);

		// With grass spreading (0.2, a grass neighbour) the hole fills; the dry edge east of the block never moves.
		TerrainRuleDefinition spread;
		spread.FromTileId = 1;
		spread.ToTileId = 2;
		spread.MinMoisture = 0.2f;
		spread.NeighbourTileId = 2;
		spread.Chance = 1.0f;
		Hydrology growing = WaterRules::Start(map, land, settings);
		Random random(5);
		for (int i = 0; i < 100; i++)
		{
			GrowthRules::Step(map, growing, land, { spread }, 200, random);
			WaterRules::Step(map, growing, land, settings);
		}
		int east = 0;
		for (int y = 0; y < 11; y++)
		{
			for (int x = 8; x < 13; x++)
			{
				east += map.Get(x, y) == 2 ? 1 : 0;
			}
		}
		RULE_CHECK(map.Get(5, 5) == 2 && east == 0);
		RULE_CHECK(growing.Moisture[hole] >= 51); // filled in, it stays wet enough not to dry back
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

		// A wide river's end is rounded: by the banks it needs more head (FillHead), so the middle reaches furthest.
		RULE_CHECK(WaterRules::FillHead(narrow, 10, 2, settings) == 0.0f);
		TileMap broad = ChannelMap(120, 9, 15);
		std::vector<uint8_t> broadLand(broad.GetTiles().size(), 1);
		for (int y = 0; y < 15; y++)
		{
			broadLand[(size_t)y * 121] = 0;
		}
		Hydrology broadWater = WaterRules::Start(broad, broadLand, settings);
		const int middleReach = Flood(broad, broadWater, broadLand, settings, 200);
		int bankReach = 0;
		while (bankReach + 1 < broad.GetWidth() && broad.Get(bankReach + 1, 3) == 3)
		{
			bankReach++;
		}
		int innerReach = 0;
		while (innerReach + 1 < broad.GetWidth() && broad.Get(innerReach + 1, 4) == 3)
		{
			innerReach++;
		}
		RULE_CHECK(middleReach < 120 && bankReach + 2 < innerReach && innerReach + 2 < middleReach);
		std::printf("  Water: nine wide river ends rounded: bank %d, inner %d, middle %d tiles\n", bankReach, innerReach, middleReach);

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

		// A Melt tile (MountainRules::Meltwater: the raised land below snow) feeds the riverbed next to it with MeltHead; the
		// meltwater is fresh, and the channel drains when the melt stops.
		WaterRules::Settings melt = settings;
		melt.MeltHead = 10.0f;
		TileMap stream = ChannelMap(30, 1, 5);
		for (int y = 0; y < 5; y++)
		{
			stream.Set(0, y, 6); // rock at the channel's head instead of the sea
		}
		stream.TakeChanges();
		const std::vector<uint8_t> streamLand(stream.GetTiles().size(), 1);
		TileMap dry(stream);
		Hydrology dryWater = WaterRules::Start(dry, streamLand, melt);
		RULE_CHECK(Flood(dry, dryWater, streamLand, melt, 50) == 0); // no meltwater: nothing flows
		Hydrology streamWater = WaterRules::Start(stream, streamLand, melt);
		std::vector<uint8_t> meltMask(stream.GetTiles().size(), 0);
		for (int y = 0; y < 5; y++)
		{
			meltMask[(size_t)y * 31] = 1;
		}
		WaterRules::UpdateMelt(stream, streamWater, meltMask);
		const int meltReach = Flood(stream, streamWater, streamLand, melt, 60);
		RULE_CHECK(meltReach >= 8 && meltReach <= 10);
		RULE_CHECK(streamWater.Moisture[(size_t)2 * 31 + 3] == 255 && streamWater.Moisture[(size_t)1 * 31 + 3] > 150);
		WaterRules::UpdateMelt(stream, streamWater, std::vector<uint8_t>(stream.GetTiles().size(), 0));
		for (int i = 0; i < 60; i++)
		{
			WaterRules::Step(stream, streamWater, streamLand, melt);
		}
		RULE_CHECK(stream.Get(1, 2) == 5 && stream.Get(5, 2) == 5);

		// Highland rain: raised land gets RainPerLevel a level as moisture, never passed on; ground level none.
		WaterRules::Settings rainy = settings;
		rainy.RainPerLevel = 0.1f;
		TileMap hills(5, 5, 1);
		HeightMap hillHeights(5, 5);
		hillHeights.Set(2, 2, 3);
		const std::vector<uint8_t> hillLand(25, 1);
		Hydrology hillWater = WaterRules::Start(hills, hillLand, rainy, &hillHeights);
		RULE_CHECK(hillWater.Moisture[12] == 77 && hillWater.Carried[12] == 0 && hillWater.Moisture[11] == 0);
		hillHeights.Set(2, 2, 1);
		hillHeights.Set(0, 0, 10);
		WaterRules::UpdateRain(hills, hillWater, hillHeights, hillHeights.TakeChanges(), rainy);
		WaterRules::Step(hills, hillWater, hillLand, rainy);
		RULE_CHECK(hillWater.Moisture[12] == 26 && hillWater.Moisture[0] == 255 && hillWater.Moisture[1] == 0);

		// Snowmelt wets the land it runs down: Melt times MeltMoisture, never passed on.
		WaterRules::Settings snowmelt = settings;
		snowmelt.MeltMoisture = 0.5f;
		TileMap slope(5, 5, 1);
		Hydrology slopeWater = WaterRules::Start(slope, hillLand, snowmelt);
		std::vector<uint8_t> slopeMelt(25, 0);
		slopeMelt[12] = 255;
		slopeMelt[13] = 1;
		WaterRules::UpdateMelt(slope, slopeWater, slopeMelt);
		WaterRules::Step(slope, slopeWater, hillLand, snowmelt);
		RULE_CHECK(slopeWater.Moisture[12] == 128 && slopeWater.Moisture[13] == 1 && slopeWater.Moisture[11] == 0);
		RULE_CHECK(slopeWater.Carried[12] == 0);
		std::printf("  Meltwater: narrow channel %d tiles at melt head 10\n", meltReach);
		TestShelter(checks, settings);
	}

	static int CountTile(const TileMap& map, uint8_t tileId)
	{
		return (int)map.GetCount(tileId);
	}

	// A cone of land: peak levels at (cx, cy), one less per tile away (rounded), 0 beyond.
	static HeightMap ConeHeights(int width, int height, int cx, int cy, int peak)
	{
		HeightMap heights(width, height);
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				const float distance = std::sqrt((float)((x - cx) * (x - cx) + (y - cy) * (y - cy)));
				const int level = peak - (int)std::lround(distance);
				heights.Set(x, y, (uint8_t)(level > 0 ? level : 0));
			}
		}
		heights.TakeChanges();
		return heights;
	}

	// The same heights, cut down to at most `top`.
	static HeightMap CutHeights(const HeightMap& heights, int top)
	{
		HeightMap cut(heights);
		for (int y = 0; y < cut.GetHeight(); y++)
		{
			for (int x = 0; x < cut.GetWidth(); x++)
			{
				cut.Set(x, y, (uint8_t)(cut.Get(x, y) > top ? top : cut.Get(x, y)));
			}
		}
		return cut;
	}

	static void TestHeightRules(int& checks)
	{
		// HeightMap: levels per tile, 0 off the map, a revision and a change log like TileMap's.
		HeightMap heights(4, 3);
		RULE_CHECK(heights.GetLevels().size() == 12 && heights.Get(3, 2) == 0 && heights.Get(-1, 0) == 0 && heights.Get(4, 0) == 0);
		const uint64_t revision = heights.GetRevision();
		RULE_CHECK(heights.Set(1, 2, 5) && heights.Get(1, 2) == 5 && heights.GetRevision() == revision + 1);
		RULE_CHECK(heights.Set(1, 2, 5) == false && heights.Set(9, 9, 1) == false && heights.GetRevision() == revision + 1);
		std::vector<uint32_t> changes = heights.TakeChanges();
		RULE_CHECK(changes.size() == 1 && changes[0] == 9 && heights.TakeChanges().empty());
		HeightMap bigger(6, 6, 2);
		heights.Replace(bigger);
		RULE_CHECK(heights.GetWidth() == 6 && heights.Get(5, 5) == 2 && heights.GetRevision() > revision + 1);

		// Bump: 1 at the rim, one more every 2 tiles in; a small brush always 1.
		RULE_CHECK(HeightRules::Bump(0.0f, 8.0f) == 5 && HeightRules::Bump(3.0f, 8.0f) == 3 && HeightRules::Bump(7.9f, 8.0f) == 1);
		RULE_CHECK(HeightRules::Bump(9.0f, 8.0f) == 1 && HeightRules::Bump(0.0f, 0.5f) == 1 && HeightRules::Bump(0.0f, 1.5f) == 1);

		// Move: a stroke moves a tile at most its bump from its level at the press, within 0 and MaxLevel.
		HeightRules::Settings settings; // max level 15, max step 2, water 3, riverbed 5
		HeightMap field(5, 5);
		HeightRules::Stroke stroke;
		stroke.Begin(25);
		RULE_CHECK(HeightRules::Move(field, stroke, 2, 2, 2, settings) && field.Get(2, 2) == 2);
		RULE_CHECK(HeightRules::Move(field, stroke, 2, 2, 2, settings) == false && field.Get(2, 2) == 2); // held: no more
		stroke.Begin(25);
		RULE_CHECK(HeightRules::Move(field, stroke, 2, 2, 2, settings) && field.Get(2, 2) == 4); // a new press: 2 more
		stroke.Begin(25);
		RULE_CHECK(HeightRules::Move(field, stroke, 2, 2, -3, settings) && field.Get(2, 2) == 1);
		RULE_CHECK(HeightRules::Move(field, stroke, 2, 2, -3, settings) == false && field.Get(2, 2) == 1);
		stroke.Begin(25);
		RULE_CHECK(HeightRules::Move(field, stroke, 2, 2, -5, settings) && field.Get(2, 2) == 0);
		stroke.Begin(25);
		RULE_CHECK(HeightRules::Move(field, stroke, 1, 1, 40, settings) && field.Get(1, 1) == 15);

		// Slump raising: a spike lifts the land around it to within MaxStep; the sea and water hold their ground (cliffs).
		TileMap ground(9, 9, 1);
		ground.Set(5, 4, 3);
		std::vector<uint8_t> land(81, 1);
		land[(size_t)4 * 9 + 3] = 0;
		HeightMap spike(9, 9);
		spike.Set(4, 4, 8);
		std::vector<uint32_t> moved = HeightRules::Slump(spike, ground, land, { 40u }, true, settings);
		bool gentle = true;
		for (int y = 0; y < 9; y++)
		{
			for (int x = 0; x < 9; x++)
			{
				const bool fixed = (x == 5 && y == 4) || (x == 3 && y == 4);
				for (int oy = -1; oy <= 1 && fixed == false; oy++)
				{
					for (int ox = -1; ox <= 1; ox++)
					{
						const bool neighbourFixed = (x + ox == 5 && y + oy == 4) || (x + ox == 3 && y + oy == 4);
						if (spike.InBounds(x + ox, y + oy) && neighbourFixed == false)
						{
							gentle = gentle && std::abs((int)spike.Get(x, y) - (int)spike.Get(x + ox, y + oy)) <= 2;
						}
					}
				}
			}
		}
		RULE_CHECK(gentle && spike.Get(4, 3) == 6 && spike.Get(4, 2) == 4 && spike.Get(4, 1) == 2 && spike.Get(4, 0) == 0);
		RULE_CHECK(spike.Get(5, 4) == 0 && spike.Get(3, 4) == 0 && moved.size() > 20);

		// Slump lowering: a pit dug into a plateau cuts the land around it down to within MaxStep.
		HeightMap plateau(9, 9, 6);
		plateau.Set(4, 4, 0);
		HeightRules::Slump(plateau, TileMap(9, 9, 1), {}, { 40u }, false, settings);
		RULE_CHECK(plateau.Get(4, 3) == 2 && plateau.Get(4, 2) == 4 && plateau.Get(4, 1) == 6 && plateau.Get(3, 3) == 2);

		// A level stands levelPixels art pixels high on screen.
		RULE_CHECK(std::abs(HeightRules::LevelHeight(8.0f, 32.0f, 30.0f) - 0.288675f) < 1e-4f);

		// RayToLand: straight down onto a raised tile meets its top, elsewhere the ground; a flat ray meets the wall.
		HeightMap tower(9, 9);
		tower.Set(4, 4, 4);
		glm::vec2 point(0.0f);
		float z = -1.0f;
		RULE_CHECK(HeightRules::RayToLand({ 0.0f, 0.0f, 50.0f }, { 0.0f, 0.0f, -1.0f }, tower, 1.0f, 15, point, z));
		RULE_CHECK(std::abs(point.x - 4.5f) < 1e-3f && std::abs(point.y - 4.5f) < 1e-3f && z == 4.0f);
		RULE_CHECK(HeightRules::RayToLand({ -3.0f, 3.0f, 50.0f }, { 0.0f, 0.0f, -1.0f }, tower, 1.0f, 15, point, z));
		RULE_CHECK(std::abs(point.x - 1.5f) < 1e-3f && std::abs(point.y - 1.5f) < 1e-3f && z == 0.0f);
		RULE_CHECK(HeightRules::RayToLand({ -3.0f, 0.0f, 2.0f }, { 1.0f, 0.0f, -0.01f }, tower, 1.0f, 15, point, z));
		RULE_CHECK(point.x >= 4.0f && point.x < 4.3f && z == 4.0f);
		RULE_CHECK(HeightRules::RayToLand({ 0.0f, 0.0f, 5.0f }, { 0.0f, 0.0f, 1.0f }, tower, 1.0f, 15, point, z) == false);
		RULE_CHECK(HeightRules::RayToLand({ 40.0f, 0.0f, 5.0f }, { 0.0f, 0.0f, -1.0f }, tower, 1.0f, 15, point, z) == false);

		// Generated levels follow the height noise: floor((noise - from) x levelsPerHeight), only where kept.
		TerrainGenRules::Settings gen;
		gen.Seed = 3;
		gen.Width = 16;
		gen.Height = 16;
		const HeightMap generated = TerrainGenRules::Levels(gen, 0.0f, 10.0f, 15);
		bool follows = true;
		for (int i = 0; i < 16; i++)
		{
			const float noise = TerrainGenRules::Noise(i, 15 - i, gen.HeightScale, gen.Octaves, gen.Seed);
			follows = follows && generated.Get(i, 15 - i) == (int)std::floor(noise * 10.0f);
		}
		std::vector<uint8_t> keep(256, 1);
		keep[0] = 0;
		const HeightMap kept = TerrainGenRules::Levels(gen, 0.0f, 10.0f, 15, keep);
		RULE_CHECK(follows && kept.Get(0, 0) == 0 && kept.Get(1, 0) == generated.Get(1, 0) && generated.GetRevision() > 0);
	}

	static void TestGroundwater(int& checks)
	{
		// Generate: patches from the seed, the same every time; a share of the land high enough to spring.
		GroundwaterRules::Settings settings; // springs from 0.78: heads 6 to 20, damp up to 0.12
		const std::vector<uint8_t> first = GroundwaterRules::Generate(256, 256, 1, settings);
		const std::vector<uint8_t> again = GroundwaterRules::Generate(256, 256, 1, settings);
		const std::vector<uint8_t> other = GroundwaterRules::Generate(256, 256, 2, settings);
		int springing = 0;
		for (uint8_t value : first)
		{
			springing += GroundwaterRules::Springs(value, settings) ? 1 : 0;
		}
		const float share = (float)springing / (float)first.size();
		RULE_CHECK(first.size() == 65536 && first == again && first != other && share > 0.04f && share < 0.2f);

		// Above SpringFrom: a spring's head, the damp above, the clue (clarity scales it, clueFrom shows weaker spots).
		RULE_CHECK(GroundwaterRules::Above(255, 0.7f) == 1.0f && GroundwaterRules::Above(178, 0.7f) == 0.0f);
		RULE_CHECK(GroundwaterRules::Springs(199, settings) && GroundwaterRules::Springs(198, settings) == false);
		RULE_CHECK(GroundwaterRules::SpringHead(255, settings) == 20.0f && GroundwaterRules::SpringHead(199, settings) < 6.1f);
		RULE_CHECK(GroundwaterRules::Wetness(255, settings) == 31 && GroundwaterRules::Wetness(150, settings) == 0);
		RULE_CHECK(GroundwaterRules::Clue(255, 0.25f, 0.7f) == 16 && GroundwaterRules::Clue(255, 1.0f, 0.7f) == 63);
		RULE_CHECK(GroundwaterRules::Clue(150, 1.0f, 0.7f) == 0 && GroundwaterRules::Clue(150, 1.0f, 0.5f) == 11);
		RULE_CHECK(GroundwaterRules::Clue(255, 0.0f, 0.7f) == 0);

		// A riverbed dug over high groundwater springs: a source that fills the channel dug from it; over low groundwater
		// a dug channel stays dry. The land above high groundwater keeps a little damp.
		TileMap map(40, 7, 1);
		std::vector<uint8_t> groundwater(map.GetTiles().size(), 60);
		groundwater[(size_t)3 * 40 + 5] = 255;
		groundwater[(size_t)0 * 40 + 38] = 255;
		for (int x = 2; x <= 25; x++)
		{
			map.Set(x, 3, 5);
		}
		for (int x = 30; x <= 34; x++)
		{
			map.Set(x, 6, 5); // dug where the groundwater is low
		}
		map.TakeChanges();
		const std::vector<uint8_t> land(map.GetTiles().size(), 1);
		WaterRules::Settings water;
		Hydrology hydrology = WaterRules::Start(map, land, water, nullptr, &groundwater);
		RULE_CHECK(hydrology.Moisture[38] == 31 && hydrology.Moisture[37] == 0);
		for (uint32_t i = 0; i < (uint32_t)map.GetTiles().size(); i++)
		{
			hydrology.Activate(i);
		}
		int sprung = 0;
		for (int i = 0; i < 40; i++)
		{
			sprung += WaterRules::Step(map, hydrology, land, water).Sprung;
		}
		const size_t spring = (size_t)3 * 40 + 5;
		RULE_CHECK(sprung == 1 && map.Get(5, 3) == 3 && hydrology.Source[spring] == 1 && hydrology.Head[spring] == 20.0f);
		RULE_CHECK(map.Get(2, 3) == 3 && map.Get(24, 3) == 3 && map.Get(25, 3) == 5); // 19 tiles from a head of 20
		RULE_CHECK(map.Get(30, 6) == 5 && map.Get(34, 6) == 5);

		// Filling the spring in (here: back to dirt) ends it; the channel dries.
		map.Set(5, 3, 1);
		for (int i = 0; i < 40; i++)
		{
			WaterRules::Step(map, hydrology, land, water);
		}
		RULE_CHECK(hydrology.Source[spring] == 0 && map.Get(6, 3) == 5 && map.Get(20, 3) == 5);
		std::printf("  Groundwater: %.1f%% of a 256 x 256 map springs where dug\n", share * 100.0f);
	}

	static void TestMountainRules(int& checks)
	{
		MountainRules::Settings settings; // rock 6 from level 4, snow 4 from level 9 (ragged 1.5), bare 1, melt reach 16
		std::vector<TerrainTypeDefinition> roles(2);
		roles[0].TileId = 9;
		roles[0].Role = TerrainRole::Rock;
		roles[0].FromLevel = 3;
		roles[0].BelowTileId = 2;
		roles[1].TileId = 8;
		roles[1].Role = TerrainRole::Snow;
		roles[1].FromLevel = 7;
		MountainRules::Settings fromTypes = MountainRules::FromTypes(roles, settings);
		RULE_CHECK(fromTypes.RockTileId == 9 && fromTypes.RockLevel == 3 && fromTypes.BareTileId == 2);
		RULE_CHECK(fromTypes.SnowTileId == 8 && fromTypes.SnowLevel == 7);
		roles[0].FromLevel = 0;
		roles[0].BelowTileId = -1;
		fromTypes = MountainRules::FromTypes(roles, settings);
		RULE_CHECK(fromTypes.RockLevel == 4 && fromTypes.BareTileId == 1);

		// LevelTile: below RockLevel the land keeps its type, rock from it, snow from the snowline.
		MountainRules::Settings exact = settings;
		exact.Ragged = 0.0f;
		RULE_CHECK(MountainRules::LevelTile(0, 3, 3, exact) == -1 && MountainRules::LevelTile(3, 3, 3, exact) == -1);
		RULE_CHECK(MountainRules::LevelTile(4, 3, 3, exact) == 6 && MountainRules::LevelTile(8, 3, 3, exact) == 6);
		RULE_CHECK(MountainRules::LevelTile(9, 3, 3, exact) == 4 && MountainRules::LevelTile(15, 3, 3, exact) == 4);

		// A cone of peak 12 on grass settles into a snow cap, a ring of rock and grass below; settling again changes nothing.
		TileMap map(25, 25, 2);
		const HeightMap cone = ConeHeights(25, 25, 12, 12, 12);
		MountainRules::SettleReport report = MountainRules::Settle(map, cone, settings);
		const int cap = CountTile(map, 4);
		RULE_CHECK(report.Snowed == cap && report.Rocked == CountTile(map, 6) && report.Melted == 0 && report.Bared == 0);
		RULE_CHECK(cap >= 13 && cap <= 37 && CountTile(map, 6) > cap);
		RULE_CHECK(map.Get(12, 12) == 4 && map.Get(12, 6) == 6 && map.Get(12, 3) == 2 && map.Get(0, 0) == 2);
		report = MountainRules::Settle(map, cone, settings);
		RULE_CHECK(report.Snowed == 0 && report.Melted == 0 && report.Rocked == 0 && report.Bared == 0);

		// Cut down to level 5 the snow melts to rock; to level 2 the rock goes bare (dirt), not back to grass.
		report = MountainRules::Settle(map, CutHeights(cone, 5), settings);
		RULE_CHECK(report.Melted == cap && CountTile(map, 4) == 0 && map.Get(12, 12) == 6);
		report = MountainRules::Settle(map, CutHeights(cone, 2), settings);
		RULE_CHECK(report.Bared > 0 && CountTile(map, 6) == 0 && map.Get(12, 12) == 1 && map.Get(12, 3) == 2);

		// SettleTiles looks only at the listed tiles; without heights for the map nothing settles.
		TileMap partial(25, 25, 2);
		report = MountainRules::SettleTiles(partial, cone, { (uint32_t)(12 * 25 + 12), (uint32_t)(12 * 25 + 6) }, settings);
		RULE_CHECK(report.Snowed == 1 && report.Rocked == 1 && partial.Get(12, 12) == 4 && partial.Get(12, 11) == 2);
		TileMap unmeasured(25, 25, 2);
		RULE_CHECK(MountainRules::Settle(unmeasured, HeightMap(3, 3, 9), settings).Rocked == 0 && CountTile(unmeasured, 6) == 0);

		// The snowline is ragged by tile but fixed: within half of Ragged either way, the same every time.
		bool within = true;
		bool varies = false;
		for (int i = 0; i < 50; i++)
		{
			const float line = MountainRules::SnowLine(i, i * 3, settings);
			within = within && line >= 8.25f && line <= 9.75f && line == MountainRules::SnowLine(i, i * 3, settings);
			varies = varies || std::abs(line - MountainRules::SnowLine(0, 0, settings)) > 0.1f;
		}
		RULE_CHECK(within && varies);

		// Meltwater runs from the snow down the raised land within MeltReach, fading (255 at the snow, 1 at the reach), never
		// onto the ground; no snow, none.
		RULE_CHECK(MountainRules::MeltAt(0, settings) == 255 && MountainRules::MeltAt(8, settings) == 128);
		RULE_CHECK(MountainRules::MeltAt(16, settings) == 1 && MountainRules::MeltAt(40, settings) == 1);
		TileMap snowy(25, 25, 1);
		MountainRules::Settle(snowy, cone, settings);
		std::vector<uint8_t> melt = MountainRules::Meltwater(snowy, cone, settings);
		const uint8_t peak = melt[(size_t)12 * 25 + 12];
		const uint8_t slope = melt[(size_t)12 * 25 + 6];
		const uint8_t foot = melt[(size_t)12 * 25 + 1];
		RULE_CHECK(peak == 255 && slope < peak && foot > 0 && foot < slope && melt[(size_t)12 * 25 + 0] == 0);
		MountainRules::Settings shortReach = settings;
		shortReach.MeltReach = 2;
		melt = MountainRules::Meltwater(snowy, cone, shortReach);
		RULE_CHECK(melt[(size_t)12 * 25 + 12] == 255 && melt[(size_t)12 * 25 + 1] == 0);
		TileMap bare(25, 25, 1);
		const HeightMap hill = ConeHeights(25, 25, 12, 12, 6);
		MountainRules::Settle(bare, hill, settings);
		melt = MountainRules::Meltwater(bare, hill, settings);
		int melting = 0;
		for (uint8_t value : melt)
		{
			melting += value;
		}
		RULE_CHECK(melting == 0 && CountTile(bare, 6) > 0);
		std::printf("  Mountains: a cone of 12 levels gets a cap of %d snow tiles from level 9\n", cap);
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

		// Height: MinLevel and MaxLevel bound where it lives (no heights: level 0); ChancePerLevel makes it likelier higher up.
		HeightMap levels(12, 12);
		levels.Set(3, 3, 2);
		levels.Set(4, 4, 5);
		ObjectDefinition highland = MakeObject(5, 2, 0.0f, -1, 0.0f);
		highland.MinLevel = 1;
		highland.MaxLevel = 3;
		water.Moisture.assign(144, 200);
		RULE_CHECK(ObjectRules::Fits(highland, map, water, 3, 3, &levels));
		RULE_CHECK(ObjectRules::Fits(highland, map, water, 4, 4, &levels) == false); // too high
		RULE_CHECK(ObjectRules::Fits(highland, map, water, 5, 5, &levels) == false); // too low
		RULE_CHECK(ObjectRules::Fits(highland, map, water, 3, 3) == false); // no heights: level 0
		ObjectDefinition climber = MakeObject(6, 2, 0.1f, -1, 0.0f);
		climber.ChancePerLevel = 9.0f;
		RULE_CHECK(ObjectRules::HeightFactor(climber, 0) == 1.0f && ObjectRules::HeightFactor(climber, 1) == 10.0f);
		TileMap meadow(12, 12, 2);
		HeightMap slope(12, 12);
		for (int y = 0; y < 12; y++)
		{
			for (int x = 6; x < 12; x++)
			{
				slope.Set(x, y, 1); // the east half one level up: sprouts there at chance 1
			}
		}
		ObjectLayer climbers;
		climbers.Reset(144);
		const std::vector<ObjectDefinition> climbing = { climber };
		ObjectRules::Step(climbers, meadow, water, land, climbing, ObjectRules::IndexById(climbing), 400, 0.0f, random, &slope);
		int low = 0;
		int high = 0;
		for (int y = 0; y < 12; y++)
		{
			for (int x = 0; x < 12; x++)
			{
				const int grown = climbers.Get((size_t)y * 12 + (size_t)x) != 0 ? 1 : 0;
				low += x < 6 ? grown : 0;
				high += x < 6 ? 0 : grown;
			}
		}
		RULE_CHECK(high > 50 && low < high / 2);

		// Scatter: StartDensity 1 places on every land tile that fits.
		ObjectLayer rocks;
		rocks.Reset(144);
		ObjectDefinition rock = MakeObject(3, -1, 0.0f, -1, 0.0f);
		rock.StartDensity = 1.0f;
		RULE_CHECK(ObjectRules::Scatter(rocks, map, water, { rock }, random) == 142 && rocks.Get(143) == 0);
	}

	int TestTalentRules(int& checks); // TalentRulesTest.cpp
	int TestSeasonRules(int& checks); // SeasonRulesTest.cpp
	int TestWildlifeRules(int& checks); // WildlifeRulesTest.cpp
	int TestSoilRules(int& checks); // SoilRulesTest.cpp
	int TestSettlementRules(int& checks); // SettlementRulesTest.cpp
	int TestSettlingRules(int& checks);   // SettlingRulesTest.cpp
	int TestFarmRules(int& checks);       // FarmRulesTest.cpp
	int TestSaveRules(int& checks);       // SaveRulesTest.cpp
	int TestBuildingRules(int& checks);   // BuildingRulesTest.cpp
	int TestForestRules(int& checks); // ForestRulesTest.cpp
	int TestCreekRules(int& checks); // CreekRulesTest.cpp
	int TestOldWoodsRules(int& checks); // OldWoodsRulesTest.cpp
	int TestWaterLifeRules(int& checks); // WaterLifeRulesTest.cpp

	int TestGameRules(int& checks)
	{
		std::printf("Game rules: TileMap, TerrainGenRules, BrushRules, CameraRules, SimulationClock, WaterRules, HeightRules, "
			"GroundwaterRules, MountainRules, GrowthRules, ObjectRules\n");
		TestTileMap(checks);
		TestTerrainGenRules(checks);
		TestBrushRules(checks);
		TestCameraRules(checks);
		TestSimulationClock(checks);
		TestWaterRules(checks);
		TestHeightRules(checks);
		TestGroundwater(checks);
		TestMountainRules(checks);
		TestGrowthRules(checks);
		TestObjectRules(checks);
		s_RuleFailures += TestTalentRules(checks);
		s_RuleFailures += TestSeasonRules(checks);
		s_RuleFailures += TestWildlifeRules(checks);
		s_RuleFailures += TestSoilRules(checks);
		s_RuleFailures += TestSettlementRules(checks);
		s_RuleFailures += TestSettlingRules(checks);
		s_RuleFailures += TestFarmRules(checks);
		s_RuleFailures += TestSaveRules(checks);
		s_RuleFailures += TestBuildingRules(checks);
		s_RuleFailures += TestForestRules(checks);
		s_RuleFailures += TestCreekRules(checks);
		s_RuleFailures += TestOldWoodsRules(checks);
		s_RuleFailures += TestWaterLifeRules(checks);

		return s_RuleFailures;
	}
}
