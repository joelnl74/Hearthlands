// Tools/Tests/SoilRulesTest.cpp
// The soil (SoilRules), its hand in growth (GrowthRules, ObjectRules) and the rich-soil milestone; run from TestGameRules.

#include "hypch.h"

#include "Rules/GrowthRules.h"
#include "Rules/ObjectRules.h"
#include "Rules/SoilRules.h"
#include "Rules/TalentRules.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace Hydro
{
	static int s_SoilFailures = 0;

	static void SoilCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_SoilFailures++;
			std::printf("FAIL  SoilRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define SOIL_CHECK(condition) SoilCheck((condition), #condition, __LINE__, checks)

	static TerrainTypeDefinition MakeSoilType(int id, float fertility, float sand, bool living)
	{
		TerrainTypeDefinition type;
		type.TileId = id;
		type.Fertility = fertility;
		type.Sand = sand;
		type.Living = living;
		return type;
	}

	static bool Near(float a, float b, float within)
	{
		return std::fabs(a - b) <= within;
	}

	int TestSoilRules(int& checks)
	{
		std::printf("Game rules: SoilRules\n");

		// Settings from the types: sand 0, dirt 1, grass 2 (living), water 3.
		const std::vector<TerrainTypeDefinition> types = { MakeSoilType(0, 0.1f, 0.9f, false), MakeSoilType(1, 0.3f, 0.2f, false),
			MakeSoilType(2, 0.45f, 0.2f, true), MakeSoilType(3, 0.0f, 0.3f, false) };
		SoilRules::Settings settings = SoilRules::FromTypes(types, SoilRules::Settings{});
		SOIL_CHECK(settings.Fertility[0] == 0.1f && settings.Sand[0] == 0.9f && settings.Living[2] == 1 && settings.Living[1] == 0);
		SOIL_CHECK(settings.Fertility[200] == 0.3f); // types not in the table keep the default

		// Texture: loam (BestSand) best, heavy and sandy soil less; quality is richness times texture, at most 1.
		SOIL_CHECK(Near(SoilRules::Texture(0.4f, settings), 1.2f, 1e-5f));
		SOIL_CHECK(SoilRules::Texture(0.2f, settings) < 1.2f && SoilRules::Texture(0.9f, settings) < SoilRules::Texture(0.2f, settings));
		SOIL_CHECK(SoilRules::Texture(5.0f, settings) == 0.4f);
		SOIL_CHECK(Near(SoilRules::Quality(0.5f, 0.4f, settings), 0.6f, 1e-5f) && SoilRules::Quality(1.0f, 0.4f, settings) == 1.0f);

		// Factor: as it is on plain dirt (Neutral), + more on rich soil, - more on poor; never below 0.
		SOIL_CHECK(Near(SoilRules::Factor(0.8f, 0.7f), 1.8f, 1e-5f) && Near(SoilRules::Factor(0.8f, 0.0f), 0.4f, 1e-5f));
		SOIL_CHECK(Near(SoilRules::Factor(0.8f, SoilRules::Neutral), 1.0f, 1e-5f) && Near(SoilRules::Factor(-0.6f, 0.0f), 1.45f, 1e-5f));
		SOIL_CHECK(SoilRules::Factor(-1.0f, 1.0f) == 0.0f);

		// Start: each tile its type's soil.
		TileMap map(8, 4, 1);
		map.Set(0, 0, 0);
		map.Set(1, 0, 2);
		map.Set(7, 3, 3);
		SoilState soil = SoilRules::Start(map, settings);
		SOIL_CHECK(soil.Fertility.size() == 32 && soil.Fertility[0] == SoilRules::ToByte(0.1f) && soil.Sand[0] == SoilRules::ToByte(0.9f));
		SOIL_CHECK(soil.Fertility[1] == SoilRules::ToByte(0.45f) && soil.Fertility[2] == SoilRules::ToByte(0.3f) && soil.Revision == 1);

		// Paint: dirt over sand mixes half and half: loam, better than either.
		const float sandQuality = SoilRules::QualityAt(soil, 0, settings);
		SoilRules::Paint(soil, 0, 1, settings);
		SOIL_CHECK(Near((float)soil.Sand[0] / 255.0f, 0.55f, 0.01f) && Near((float)soil.Fertility[0] / 255.0f, 0.2f, 0.01f));
		SOIL_CHECK(SoilRules::Texture(0.55f, settings) > SoilRules::Texture(0.2f, settings));
		SOIL_CHECK(SoilRules::QualityAt(soil, 0, settings) > sandQuality && soil.Revision == 2);
		SoilRules::Paint(soil, 999, 1, settings); // off the map: nothing
		SOIL_CHECK(soil.Revision == 2);
		SOIL_CHECK(SoilRules::QualityAt(SoilState{}, 0, settings) == SoilRules::Neutral);
		SOIL_CHECK(SoilRules::Lookup{}.Quality(3) == SoilRules::Neutral);

		// Target: the type's, humus from shrubs and trees, silt by fresh water, the talents' bonus where life is.
		SOIL_CHECK(Near(SoilRules::Target(settings, 1, 0, 0.0f), 0.3f, 1e-5f));
		SOIL_CHECK(Near(SoilRules::Target(settings, 1, 2, 0.0f), 0.5f, 1e-5f));
		SOIL_CHECK(Near(SoilRules::Target(settings, 1, 1, 0.0f), 0.4f, 1e-5f));
		SOIL_CHECK(Near(SoilRules::Target(settings, 1, 0, 1.0f), 0.55f, 1e-5f));
		settings.Bonus = 0.1f;
		SOIL_CHECK(Near(SoilRules::Target(settings, 2, 0, 0.0f), 0.55f, 1e-5f));
		SOIL_CHECK(Near(SoilRules::Target(settings, 1, 0, 0.0f), 0.3f, 1e-5f));
		settings.Bonus = 0.0f;
		SOIL_CHECK(SoilRules::Target(settings, 2, 2, 1.0f) == 0.9f);

		// Step: richness drifts to the target, rising faster than it falls; water keeps its own.
		Hydrology water;
		water.Carried.assign(32, 0);
		ObjectLayer objects;
		objects.Reset(32);
		ObjectDefinition tree;
		tree.ObjectId = 5;
		tree.Kind = ObjectKind::Tree;
		ObjectDefinition shrub;
		shrub.ObjectId = 6;
		shrub.Kind = ObjectKind::Shrub;
		const std::vector<uint8_t> cover = SoilRules::CoverOf({ tree, shrub });
		SOIL_CHECK(cover[5] == 2 && cover[6] == 1 && cover[0] == 0);
		objects.Set(2, 5); // a tree on dirt at (2, 0)
		water.Carried[3] = 255; // fresh water beside (3, 0)
		soil = SoilRules::Start(map, settings);
		Random random(3);
		for (int i = 0; i < 4000; i++)
		{
			SoilRules::Step(soil, map, water, objects, cover, {}, 8, settings, random);
		}
		SOIL_CHECK(Near((float)soil.Fertility[2] / 255.0f, 0.5f, 0.01f) && Near((float)soil.Fertility[3] / 255.0f, 0.55f, 0.01f));
		SOIL_CHECK(Near((float)soil.Fertility[4] / 255.0f, 0.3f, 0.01f) && soil.Fertility[31] == SoilRules::ToByte(0.0f));
		objects.Set(2, 0); // the tree gone: back to the dirt's, slowly
		soil.Fertility[5] = SoilRules::ToByte(0.9f);
		const uint64_t revision = soil.Revision;
		int changed = 0;
		for (int i = 0; i < 64; i++)
		{
			changed += SoilRules::Step(soil, map, water, objects, cover, {}, 1, settings, random);
		}
		SOIL_CHECK(changed > 0 && soil.Revision > revision && soil.Fertility[5] >= SoilRules::ToByte(0.3f));
		std::vector<uint8_t> land(32, 1);
		land[5] = 0; // off the land: never looked at
		soil.Fertility[5] = SoilRules::ToByte(0.9f);
		for (int i = 0; i < 2000; i++)
		{
			SoilRules::Step(soil, map, water, objects, cover, land, 8, settings, random);
		}
		SOIL_CHECK(soil.Fertility[5] == SoilRules::ToByte(0.9f));

		// Growth: SoilEffect moves a rule's chance: far more grass on rich soil than on poor, from the same looks.
		Hydrology wet;
		wet.Moisture.assign(2000, 200);
		TerrainRuleDefinition spread;
		spread.FromTileId = 1;
		spread.ToTileId = 2;
		spread.NeighbourTileId = -1;
		spread.Chance = 0.2f;
		spread.SoilEffect = 1.0f;
		TileMap poorField(2000, 1, 1);
		TileMap richField(2000, 1, 1);
		SoilState poor = SoilRules::Start(poorField, settings);
		poor.Fertility.assign(2000, 0);
		SoilState rich = SoilRules::Start(richField, settings);
		rich.Fertility.assign(2000, 255);
		rich.Sand.assign(2000, SoilRules::ToByte(0.4f));
		Random dice(9);
		const SoilRules::Lookup poorSoil{ &poor, &settings };
		const SoilRules::Lookup richSoil{ &rich, &settings };
		const int poorGrass = GrowthRules::Step(poorField, wet, {}, { spread }, 1000, dice, poorSoil);
		const int richGrass = GrowthRules::Step(richField, wet, {}, { spread }, 1000, dice, richSoil);
		SOIL_CHECK(poorGrass > 0 && poorGrass * 4 < richGrass);

		// Objects: the same for sprouting.
		ObjectDefinition bush;
		bush.ObjectId = 1;
		bush.Kind = ObjectKind::Shrub;
		bush.GrowsOnTileId = 2;
		bush.SproutChance = 0.2f;
		bush.SoilEffect = 1.0f;
		std::vector<int> byId(256, -1);
		byId[1] = 0;
		TileMap meadow(2000, 1, 2);
		ObjectLayer poorBushes;
		poorBushes.Reset(2000);
		ObjectLayer richBushes;
		richBushes.Reset(2000);
		const int poorSprouts = ObjectRules::Step(poorBushes, meadow, wet, {}, { bush }, byId, 1000, 1.0f, dice, nullptr, poorSoil)
			.Sprouted;
		const int richSprouts = ObjectRules::Step(richBushes, meadow, wet, {}, { bush }, byId, 1000, 1.0f, dice, nullptr, richSoil)
			.Sprouted;
		SOIL_CHECK(poorSprouts > 0 && poorSprouts * 4 < richSprouts);

		// The milestone: land tiles at or above TargetId percent.
		std::vector<uint8_t> qualities;
		soil = SoilRules::Start(map, settings);
		soil.Fertility[1] = SoilRules::ToByte(0.8f); // grass at 0.8: rich
		const uint32_t richTiles = SoilRules::Qualities(soil, settings, qualities, 0.6f);
		SOIL_CHECK(qualities.size() == 32 && richTiles == 1);
		TalentRules::Counts counts;
		std::vector<uint8_t> island(32, 1);
		island[1] = 0;
		TalentRules::AddSoil(counts, qualities, {});
		MilestoneDefinition earth;
		earth.Kind = MilestoneKind::RichSoil;
		earth.TargetId = 60;
		SOIL_CHECK(TalentRules::Progress(earth, counts) == 1);
		TalentRules::Counts sea;
		TalentRules::AddSoil(sea, qualities, island);
		SOIL_CHECK(TalentRules::Progress(earth, sea) == 0);
		earth.TargetId = 0;
		SOIL_CHECK(TalentRules::Progress(earth, sea) == 31);

		return s_SoilFailures;
	}
}
