// Tools/Tests/WaterLifeRulesTest.cpp
// The water's habitats (HabitatRules), fish and water birds that need them, discoveries (WildlifeRules) and their milestones.

#include "hypch.h"

#include "Rules/HabitatRules.h"
#include "Rules/TalentRules.h"
#include "Rules/WildlifeRules.h"

#include <cstdio>
#include <string>
#include <vector>

namespace Hydro
{
	static int s_WaterLifeFailures = 0;

	static void WaterLifeCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_WaterLifeFailures++;
			std::printf("FAIL  WaterLifeRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define WATERLIFE_CHECK(condition) WaterLifeCheck((condition), #condition, __LINE__, checks)

	static AnimalDefinition MakeWaterAnimal(int id, AnimalKind kind, WaterHabitat habitat, float density)
	{
		AnimalDefinition animal;
		animal.Name = "Animal " + std::to_string(id);
		animal.AnimalId = id;
		animal.Kind = kind;
		animal.Habitat = habitat;
		animal.Density = density;
		animal.SpreadPerDay = 0.0f;
		animal.ArrivePerDay = 0.0f;
		animal.StartShare = 0.0f; // none at the start: found later
		return animal;
	}

	// A 64 x 32 dirt map: the sea on its left 8 columns (outside the coast); a stream 2 wide on rows 5 and 6 from the sea
	// to x = 40; a river 4 wide on rows 12 to 15 from x = 20 to 50 (not joined to the sea); a 10 x 10 lake at x = 50, y = 20;
	// the world's own lake 6 x 6 at x = 26, y = 22; a cold stream 1 wide on row 1 from x = 44 to 60 with meltwater.
	struct WaterLifeWorld
	{
		static constexpr int Width = 64;
		static constexpr int Height = 32;
		TileMap Map{ Width, Height, 1 };
		HeightMap Heights{ Width, Height };
		Hydrology Water;
		std::vector<uint8_t> Land;

		void Fill(int x0, int y0, int x1, int y1)
		{
			for (int y = y0; y <= y1; y++)
			{
				for (int x = x0; x <= x1; x++)
				{
					Map.Set(x, y, 3);
				}
			}
		}

		size_t At(int x, int y) const
		{
			return (size_t)y * (size_t)Width + (size_t)x;
		}

		WaterLifeWorld()
		{
			Land.assign((size_t)(Width * Height), 1);
			Fill(0, 0, 7, Height - 1);
			for (int y = 0; y < Height; y++)
			{
				for (int x = 0; x < 8; x++)
				{
					Land[At(x, y)] = 0;
				}
			}
			Fill(26, 22, 31, 27);
			Water.Native = HabitatRules::NativeOf(Map, Water.WaterTileId); // the sea and the world's lake
			Fill(8, 5, 40, 6);
			Fill(20, 12, 50, 15);
			Fill(50, 20, 59, 29);
			Fill(44, 1, 60, 1);
			Water.Melt.assign((size_t)(Width * Height), 0);
			for (int x = 44; x <= 60; x++)
			{
				Water.Melt[At(x, 1)] = 255;
			}
			Water.Moisture.assign((size_t)(Width * Height), 160);
		}
	};

	int TestWaterLifeRules(int& checks)
	{
		std::printf("Game rules: WaterLife\n");
		WaterLifeWorld world;
		const std::vector<uint8_t>& native = world.Water.Native;
		WATERLIFE_CHECK(native[world.At(3, 3)] == 1 && native[world.At(28, 24)] == 1 && native[world.At(20, 5)] == 0);

		// Habitats: the sea; the player's water by breadth; joined to the sea and its mouth; cold; the world's lake none.
		const std::vector<uint8_t> bits = HabitatRules::Classify(world.Map, world.Water, world.Land, world.Heights);
		WATERLIFE_CHECK(bits[world.At(3, 3)] == HabitatRules::SeaBit);
		WATERLIFE_CHECK(bits[world.At(30, 3)] == 0 && bits[world.At(28, 24)] == 0);
		const uint8_t stream = bits[world.At(20, 5)];
		WATERLIFE_CHECK((stream & HabitatRules::StreamBit) != 0 && (stream & HabitatRules::SeaLinkBit) != 0);
		WATERLIFE_CHECK((stream & HabitatRules::MouthBit) == 0 && (bits[world.At(10, 6)] & HabitatRules::MouthBit) != 0);
		const uint8_t river = bits[world.At(35, 13)];
		WATERLIFE_CHECK((river & HabitatRules::RiverBit) != 0 && (river & HabitatRules::SeaLinkBit) == 0);
		WATERLIFE_CHECK((bits[world.At(55, 25)] & HabitatRules::LakeBit) != 0);
		WATERLIFE_CHECK((bits[world.At(50, 20)] & HabitatRules::LakeBit) == 0); // a lake's corner is narrower
		const uint8_t cold = bits[world.At(52, 1)];
		WATERLIFE_CHECK((cold & HabitatRules::StreamBit) != 0 && (cold & HabitatRules::ColdBit) != 0);

		// Matches: what each WaterHabitat means of the bits.
		WATERLIFE_CHECK(HabitatRules::Matches(WaterHabitat::SeaRun, stream) && HabitatRules::Matches(WaterHabitat::SeaRun, river) == false);
		WATERLIFE_CHECK(HabitatRules::Matches(WaterHabitat::ColdStream, cold) && HabitatRules::Matches(WaterHabitat::ColdStream, stream) == false);
		WATERLIFE_CHECK(HabitatRules::Matches(WaterHabitat::Flowing, river) && HabitatRules::Matches(WaterHabitat::Flowing, stream));
		WATERLIFE_CHECK(HabitatRules::Matches(WaterHabitat::Fresh, bits[world.At(55, 25)])
			&& HabitatRules::Matches(WaterHabitat::Fresh, HabitatRules::SeaBit) == false);
		WATERLIFE_CHECK(HabitatRules::Matches(WaterHabitat::None, 0) && HabitatRules::Matches(WaterHabitat::Sea, stream) == false);

		// Count: per WaterHabitat; joining the river to the stream joins it to the sea.
		std::vector<uint32_t> counts = HabitatRules::Count(bits);
		WATERLIFE_CHECK(counts[(size_t)WaterHabitat::Sea] == 8u * 32u && counts[(size_t)WaterHabitat::ColdStream] == 17u);
		WATERLIFE_CHECK(counts[(size_t)WaterHabitat::Lake] > 0u && counts[(size_t)WaterHabitat::SeaRun] < 80u);
		const uint32_t runBefore = counts[(size_t)WaterHabitat::SeaRun];
		world.Fill(20, 7, 21, 11);
		world.Water.Habitat = HabitatRules::Classify(world.Map, world.Water, world.Land, world.Heights);
		counts = HabitatRules::Count(world.Water.Habitat);
		WATERLIFE_CHECK(counts[(size_t)WaterHabitat::SeaRun] > runBefore + 100u);
		WATERLIFE_CHECK((world.Water.Habitat[world.At(35, 13)] & HabitatRules::SeaLinkBit) != 0);
		Hydrology dry;
		WATERLIFE_CHECK(HabitatRules::Classify(TileMap(4, 4, 1), dry, {}, HeightMap(4, 4))[5] == 0);

		// Fish live only in water of their habitat; plain fish in any water.
		const AnimalDefinition salmon = MakeWaterAnimal(33, AnimalKind::Fish, WaterHabitat::SeaRun, 0.1f);
		AnimalDefinition fish = MakeWaterAnimal(20, AnimalKind::Fish, WaterHabitat::None, 0.1f);
		fish.StartShare = 0.5f; // the world's own
		WATERLIFE_CHECK(WildlifeRules::Suits(salmon, 3, 0, 0, world.Water, stream));
		WATERLIFE_CHECK(WildlifeRules::Suits(salmon, 3, 0, 0, world.Water, HabitatRules::RiverBit) == false);
		WATERLIFE_CHECK(WildlifeRules::Suits(salmon, 3, 0, 0, world.Water) == false && WildlifeRules::Suits(fish, 3, 0, 0, world.Water));
		WATERLIFE_CHECK(WildlifeRules::Suits(salmon, 1, 0, 0, world.Water, stream) == false);

		// Holds: fish by the region's tiles of their habitat; a water bird needs MinHabitat tiles of it in its region.
		ObjectLayer objects;
		objects.Reset((size_t)(WaterLifeWorld::Width * WaterLifeWorld::Height));
		const std::vector<uint8_t> cover(256, 0);
		AnimalDefinition dipper = MakeWaterAnimal(45, AnimalKind::Bird, WaterHabitat::ColdStream, 0.01f);
		dipper.MinHabitat = 10;
		AnimalDefinition picky = dipper;
		picky.MinHabitat = 40;
		const AnimalDefinition trout = MakeWaterAnimal(31, AnimalKind::Fish, WaterHabitat::ColdStream, 0.5f);
		const WildlifeRules::Survey top = WildlifeRules::Look(world.Map, world.Water, objects, cover, 48, 0, 63, 15);
		const float troutHeld = WildlifeRules::Holds(trout, world.Map, world.Water, world.Heights, top, 48, 0, 63, 15);
		WATERLIFE_CHECK(troutHeld > 6.4f && troutHeld < 6.6f); // 13 cold stream tiles there
		WATERLIFE_CHECK(WildlifeRules::Holds(dipper, world.Map, world.Water, world.Heights, top, 48, 0, 63, 15) > 0.0f);
		WATERLIFE_CHECK(WildlifeRules::Holds(picky, world.Map, world.Water, world.Heights, top, 48, 0, 63, 15) == 0.0f);
		const WildlifeRules::Survey low = WildlifeRules::Look(world.Map, world.Water, objects, cover, 48, 16, 63, 31);
		WATERLIFE_CHECK(WildlifeRules::Holds(dipper, world.Map, world.Water, world.Heights, low, 48, 16, 63, 31) == 0.0f);

		// Discover: the world's own species are known from the start; others once one lives, announced once.
		const std::vector<AnimalDefinition> animals = { fish, salmon, trout };
		WildlifeState state = WildlifeRules::Start(WaterLifeWorld::Width, WaterLifeWorld::Height, animals.size());
		WildlifeRules::Fill(state, animals, world.Map, world.Water, world.Heights, objects, cover, WildlifeRules::Factors{});
		WATERLIFE_CHECK(state.IsDiscovered(0) && state.IsDiscovered(1) == false && state.Found.empty());
		WATERLIFE_CHECK(WildlifeRules::Discover(state, animals).empty());
		state.Population[1][0] = 0.5f;
		WATERLIFE_CHECK(WildlifeRules::Discover(state, animals).empty());
		state.Population[1][1] = 0.6f;
		const std::vector<int> found = WildlifeRules::Discover(state, animals);
		WATERLIFE_CHECK(found.size() == 1 && found[0] == 1 && state.IsDiscovered(1) && state.Found.size() == 1);
		state.Population[1][0] = 0.0f;
		state.Population[1][1] = 0.0f;
		WATERLIFE_CHECK(WildlifeRules::Discover(state, animals).empty() && state.IsDiscovered(1));

		// Milestones: water tiles of a habitat, and species living of one kind.
		TalentRules::Counts tally;
		TalentRules::AddHabitats(tally, counts);
		std::vector<uint32_t> totals(256, 0);
		totals[20] = 12;
		totals[33] = 3;
		TalentRules::AddAnimals(tally, totals, animals);
		MilestoneDefinition toSea;
		toSea.Kind = MilestoneKind::Habitat;
		toSea.TargetId = (int)WaterHabitat::SeaRun;
		WATERLIFE_CHECK(TalentRules::Progress(toSea, tally) == counts[(size_t)WaterHabitat::SeaRun]);
		MilestoneDefinition fishKinds;
		fishKinds.Kind = MilestoneKind::Species;
		fishKinds.TargetId = (int)AnimalKind::Fish;
		WATERLIFE_CHECK(TalentRules::Progress(fishKinds, tally) == 2u && tally.Species == 2);
		fishKinds.TargetId = (int)AnimalKind::Bird;
		WATERLIFE_CHECK(TalentRules::Progress(fishKinds, tally) == 0u);
		fishKinds.TargetId = -1;
		WATERLIFE_CHECK(TalentRules::Progress(fishKinds, tally) == 2u);

		return s_WaterLifeFailures;
	}
}
