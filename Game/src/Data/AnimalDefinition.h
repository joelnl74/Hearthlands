#pragma once

#include "Hydro/Asset/Asset.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

// The wild animals as data: one row per species in Assets/Data/Animals.hytable. WildlifeRules keeps a population per
// region of the map from where it lives; AnimalsView shows some of it near the camera (sprites, or particles).
namespace Hydro
{
	// How a species is shown and moves.
	HY_ENUM()
	enum class AnimalKind : uint8_t
	{
		Walker, // a sprite sheet: rows idle, walk, run, each for SE, SW, NE, NW (scripts/art/make_animal_sheets.py)
		Bird,   // a sprite sheet: rows fly left, away, toward, right, then the same on the ground; lands and takes off
		Fish,   // particles: shadows in the water
		Insect, // particles: wandering specks over the land (butterflies; glowing ones are fireflies)
		Flock   // particles: a flock of specks crossing the sky
	};

	// The water a species needs (HabitatRules gives every water tile its habitats). Fish live in water tiles of it; other
	// species need MinHabitat tiles of it in their region. Stream, River, Lake: water the player made, by how broad it is.
	HY_ENUM()
	enum class WaterHabitat : uint8_t
	{
		None,       // any (fish: any water)
		Sea,        // the open sea round the island
		Stream,     // narrow running water: 1 or 2 tiles across
		River,      // a big river: about 3 to 5 tiles across
		Lake,       // broad water: about 6 tiles across or more
		Flowing,    // Stream or River
		Mouth,      // where a river the player made meets the sea (a few tiles up it)
		SeaRun,     // Stream or River joined to the sea: fish swim up from it
		ColdStream, // a Stream fed by meltwater or up in the hills
		Fresh       // Stream, River or Lake: any water the player made
	};

	HY_DATA(Table, DisplayField(Name))
	struct AnimalDefinition
	{
		HY_PROPERTY()
		std::string Name;
		HY_PROPERTY(Range(1, 255), Tooltip("Unique, never reused: milestones and the run refer to it"))
		int AnimalId = 1;
		HY_PROPERTY()
		AnimalKind Kind = AnimalKind::Walker;

		HY_PROPERTY(Asset(Texture2D), Tooltip("Walker, Bird: the sheet, a grid of cells (Sprites/Animals); Fish: its particle cells"))
		AssetHandle Sprite = 0;
		HY_PROPERTY(Range(4, 128), Tooltip("Sprite pixels per tile: the size it is drawn at"))
		float PixelsPerTile = 32.0f;
		HY_PROPERTY(Range(1, 64), Tooltip("Frames of the idle rows (Bird: of the ground rows)"))
		int IdleFrames = 1;
		HY_PROPERTY(Range(1, 64), Tooltip("Frames of the walk rows (Bird: of the ground rows, hopping)"))
		int WalkFrames = 1;
		HY_PROPERTY(Range(1, 64), Tooltip("Frames of the run rows (Bird: of the fly rows)"))
		int RunFrames = 1;
		HY_PROPERTY(Range(0.5, 30), Tooltip("Frames a second, idle"))
		float IdleRate = 6.0f;
		HY_PROPERTY(Range(0.5, 30), Tooltip("Frames a second, walking"))
		float WalkRate = 8.0f;
		HY_PROPERTY(Range(0.5, 30), Tooltip("Frames a second, running or flying"))
		float RunRate = 12.0f;
		HY_PROPERTY(Range(0.05, 10), Tooltip("Tiles a second, walking"))
		float WalkSpeed = 0.6f;
		HY_PROPERTY(Range(0.1, 20), Tooltip("Tiles a second, running from the brush (Bird: flying)"))
		float RunSpeed = 2.5f;
		HY_PROPERTY(Range(0, 16), Tooltip("Tiles from the painting brush at which it runs (Bird: takes off)"))
		float FleeDistance = 4.0f;
		HY_PROPERTY(Color, Tooltip("Fish, Insect, Flock: the particles' colour (alpha: how plain)"))
		glm::vec4 Color{ 0.1f, 0.12f, 0.14f, 0.5f };
		HY_PROPERTY(Range(0.01, 2), Tooltip("Fish, Insect, Flock: a particle's size, in tiles"))
		float ParticleSize = 0.2f;
		HY_PROPERTY(Range(0, 8), Tooltip("Insect: glow (fireflies); 0 none"))
		float Glow = 0.0f;
		HY_PROPERTY(Range(0, 255), Tooltip("Fish with a Sprite sheet: its cell (row x columns); the next cells: leaping, ripple"))
		int SpriteIndex = 0;
		HY_PROPERTY(Range(0, 30), Tooltip("Fish: leaps out of the water a minute, per group shown; 0 none"))
		float LeapsPerMinute = 0.0f;

		HY_PROPERTY(Tooltip("The tile ids it lives on; empty: any land (Fish: water)"))
		std::vector<int> TileIds;
		HY_PROPERTY(Range(0, 1), Tooltip("The tile's moisture it lives in, at least ..."))
		float MinMoisture = 0.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("... and at most"))
		float MaxMoisture = 1.0f;
		HY_PROPERTY(Range(0, 255), Tooltip("The land's height (levels) it lives at, at least ..."))
		int MinLevel = 0;
		HY_PROPERTY(Range(0, 255), Tooltip("... and at most"))
		int MaxLevel = 255;
		HY_PROPERTY(Range(0, 1), Tooltip("Share of its region's land under shrubs and trees it needs, at least ..."))
		float MinCover = 0.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("... and at most (open land)"))
		float MaxCover = 1.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("Share of its region that must be water (water birds); 0 none"))
		float MinWater = 0.0f;
		HY_PROPERTY(Tooltip("The water it needs: fish live in it, others need MinHabitat tiles of it in their region"))
		WaterHabitat Habitat = WaterHabitat::None;
		HY_PROPERTY(Range(0, 256), Tooltip("Not fish: tiles of Habitat its region needs (0 counts as 1)"))
		int MinHabitat = 0;
		HY_PROPERTY(Tooltip("A discovery: not on the map until the first ones settle, then announced"))
		bool Hidden = false;
		HY_PROPERTY(Tooltip("Hidden: the notice's line when it is discovered"))
		std::string DiscoveryText;

		HY_PROPERTY(Range(0, 1), Tooltip("Animals the land holds per tile that suits it, at most"))
		float Density = 0.02f;
		HY_PROPERTY(Range(0, 5), Tooltip("How fast it grows towards what the land holds, per day (times the season's Life)"))
		float BirthsPerDay = 0.3f;
		HY_PROPERTY(Range(0, 1), Tooltip("Share of a region's animals that move on to a neighbouring one each day"))
		float SpreadPerDay = 0.05f;
		HY_PROPERTY(Range(0, 1), Tooltip("Chance a day that a region suiting it, with none, gets a few from outside"))
		float ArrivePerDay = 0.05f;
		HY_PROPERTY(Range(-1, 255), Tooltip("Hunts this AnimalId: lives only where it is; -1 none"))
		int PreyId = -1;
		HY_PROPERTY(Range(0.5, 100), Tooltip("Prey animals each hunter needs"))
		float PreyPerAnimal = 6.0f;
		HY_PROPERTY(Range(0, 5), Tooltip("Prey each hunter takes a day"))
		float EatPerDay = 0.1f;
		HY_PROPERTY(Tooltip("Leaves in the cold: the land holds the season's Migrants share of it"))
		bool Migrates = false;
		HY_PROPERTY(Tooltip("Sleeps in the cold: the season's Burrowed share is out of sight"))
		bool Hibernates = false;
		HY_PROPERTY(Range(1, 20), Tooltip("Seen together: a group of at least ..."))
		int GroupMin = 1;
		HY_PROPERTY(Range(1, 20), Tooltip("... and at most"))
		int GroupMax = 1;
		HY_PROPERTY(Range(0, 4), Tooltip("Share of a region's animals shown near the camera (particles: how lively)"))
		float ShowShare = 1.0f;
		HY_PROPERTY(Range(0, 40), Tooltip("Shown per region, at most"))
		int MaxShown = 6;
		HY_PROPERTY(Range(0, 1), Tooltip("Share of what the land holds at the start of a generated world"))
		float StartShare = 0.5f;
	};
}
