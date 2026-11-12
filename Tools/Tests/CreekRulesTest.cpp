// Tools/Tests/CreekRulesTest.cpp
// Dry creeks for a new world (CreekRules) and their sealed beds opened by digging the head (WaterRules).

#include "hypch.h"

#include "Rules/CreekRules.h"
#include "Rules/WaterRules.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace Hydro
{
	static int s_CreekFailures = 0;

	static void CreekCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_CreekFailures++;
			std::printf("FAIL  CreekRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define CREEK_CHECK(condition) CreekCheck((condition), #condition, __LINE__, checks)

	static TerrainTypeDefinition MakeCreekType(int id, TerrainRole role)
	{
		TerrainTypeDefinition type;
		type.TileId = id;
		type.Role = role;
		return type;
	}

	int TestCreekRules(int& checks)
	{
		std::printf("Game rules: CreekRules\n");

		// Settings from the types: dirt (1) and grass (2) carvable; water 3, riverbed 5; rock 6 not.
		const std::vector<TerrainTypeDefinition> types = { MakeCreekType(1, TerrainRole::Land), MakeCreekType(2, TerrainRole::Land),
			MakeCreekType(3, TerrainRole::Water), MakeCreekType(5, TerrainRole::Riverbed), MakeCreekType(6, TerrainRole::Rock) };
		CreekRules::Settings settings = CreekRules::FromTypes(types, CreekRules::Settings{});
		CREEK_CHECK(settings.Carvable[1] == 1 && settings.Carvable[2] == 1 && settings.Carvable[3] == 0 && settings.Carvable[6] == 0);
		CREEK_CHECK(settings.WaterTileId == 3 && settings.BedTileId == 5);

		// A 100 x 100 dirt map with a river down x = 5; strong groundwater (0.9) on the right half, weak (0.4) on the left.
		TileMap map(100, 100, 1);
		std::vector<uint8_t> groundwater(100 * 100, 102);
		for (int y = 0; y < 100; y++)
		{
			map.Set(5, y, 3);
			for (int x = 50; x < 100; x++)
			{
				groundwater[(size_t)y * 100 + (size_t)x] = 230;
			}
		}
		const std::vector<uint8_t> land(100 * 100, 1);

		// Fits: strong groundwater, away from water; not on weak groundwater, near the river, or on rock.
		CREEK_CHECK(CreekRules::Fits(map, land, nullptr, groundwater, settings, 70, 50));
		CREEK_CHECK(CreekRules::Fits(map, land, nullptr, groundwater, settings, 30, 50) == false);
		map.Set(70, 50, 6);
		CREEK_CHECK(CreekRules::Fits(map, land, nullptr, groundwater, settings, 70, 50) == false);
		map.Set(70, 50, 1);
		HeightMap hills(100, 100);
		hills.Set(70, 50, 2);
		CREEK_CHECK(CreekRules::Fits(map, land, &hills, groundwater, settings, 70, 50) == false); // raised land

		// Carve: about PerTenThousand per 10,000 land tiles; each a head left as land and a riverbed of MinLength to MaxLength
		// tiles on the strong side, heads Spacing apart, the bed never beside itself.
		settings.PerTenThousand = 3.0f;
		Random random(12);
		const std::vector<CreekRules::Creek> creeks = CreekRules::Carve(map, land, nullptr, groundwater, settings, random);
		CREEK_CHECK(creeks.empty() == false && (int)creeks.size() <= 3);
		bool shaped = true;
		bool spaced = true;
		for (size_t c = 0; c < creeks.size(); c++)
		{
			const CreekRules::Creek& creek = creeks[c];
			shaped = shaped && creek.SpringX >= 50 && map.Get(creek.SpringX, creek.SpringY) == 1;
			shaped = shaped && (int)creek.Bed.size() >= settings.MinLength && (int)creek.Bed.size() <= settings.MaxLength;
			int lastX = creek.SpringX;
			int lastY = creek.SpringY;
			for (size_t i = 0; i < creek.Bed.size(); i++)
			{
				const int x = (int)(creek.Bed[i] % 100);
				const int y = (int)(creek.Bed[i] / 100);
				shaped = shaped && map.Get(x, y) == 5 && std::abs(x - lastX) + std::abs(y - lastY) == 1; // one step at a time
				for (size_t j = 0; j + 2 < i; j++)
				{
					const int px = (int)(creek.Bed[j] % 100);
					const int py = (int)(creek.Bed[j] / 100);
					shaped = shaped && (std::abs(px - x) > 1 || std::abs(py - y) > 1);
				}
				lastX = x;
				lastY = y;
			}
			for (size_t d = 0; d < c; d++)
			{
				const int dx = creeks[d].SpringX - creek.SpringX;
				const int dy = creeks[d].SpringY - creek.SpringY;
				spaced = spaced && dx * dx + dy * dy >= settings.Spacing * settings.Spacing;
			}
		}
		CREEK_CHECK(shaped && spaced);

		// The same seed carves the same creeks; no groundwater, none.
		TileMap again(100, 100, 1);
		for (int y = 0; y < 100; y++)
		{
			again.Set(5, y, 3);
		}
		Random same(12);
		const std::vector<CreekRules::Creek> twice = CreekRules::Carve(again, land, nullptr, groundwater, settings, same);
		CREEK_CHECK(twice.size() == creeks.size() && twice[0].Bed == creeks[0].Bed && again.GetTiles() == map.GetTiles());
		TileMap dry(40, 40, 1);
		CREEK_CHECK(CreekRules::Carve(dry, {}, nullptr, std::vector<uint8_t>(1600, 50), settings, same).empty());

		// The beds are sealed: over strong groundwater they stay dry until the head is dug; then the spring fills the creek
		// (and counts as found).
		WaterRules::Settings water;
		map.TakeChanges();
		Hydrology hydrology = WaterRules::Start(map, land, water, nullptr, &groundwater);
		hydrology.Sealed.assign(map.GetTiles().size(), 0);
		for (const CreekRules::Creek& creek : creeks)
		{
			for (uint32_t bed : creek.Bed)
			{
				hydrology.Sealed[bed] = 1;
			}
		}
		for (uint32_t i = 0; i < (uint32_t)map.GetTiles().size(); i++)
		{
			hydrology.Activate(i);
		}
		int sprung = 0;
		for (int i = 0; i < 30; i++)
		{
			sprung += WaterRules::Step(map, hydrology, land, water).Sprung;
		}
		const CreekRules::Creek& first = creeks[0];
		CREEK_CHECK(sprung == 0 && map.Get((int)(first.Bed[0] % 100), (int)(first.Bed[0] / 100)) == 5);
		map.Set(first.SpringX, first.SpringY, 5); // the player digs the head
		for (int i = 0; i < 30; i++)
		{
			sprung += WaterRules::Step(map, hydrology, land, water).Sprung;
		}
		CREEK_CHECK(sprung == 1 && map.Get(first.SpringX, first.SpringY) == 3);
		CREEK_CHECK(map.Get((int)(first.Bed[0] % 100), (int)(first.Bed[0] / 100)) == 3); // water runs into the bed

		// A sealed bed filled in and dug again is an ordinary bed: over strong groundwater it springs.
		const uint32_t last = first.Bed.back();
		const int lx = (int)(last % 100);
		const int ly = (int)(last / 100);
		map.Set(lx, ly, 1);
		WaterRules::Step(map, hydrology, land, water);
		hydrology.Activate(last);
		WaterRules::Step(map, hydrology, land, water);
		CREEK_CHECK(hydrology.Sealed[last] == 0);
		return s_CreekFailures;
	}
}
