#pragma once

#include "Hydro/Asset/Asset.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

// The peoples as data: one row per people in Assets/Data/Peoples.hytable. SettlingRules reads where they settle,
// SettlementRules how they live and grow (Levels), FeudRules whom they can't stand; SettlersView how they look.
namespace Hydro
{
	// A settlement's level (camp, hamlet, village): what it takes to reach it and what it gives. Level 0 is where a
	// settlement starts; its needs are not looked at.
	HY_DATA()
	struct PeopleLevel
	{
		HY_PROPERTY(Tooltip("Camp, Hamlet, Village"))
		std::string Name;
		HY_PROPERTY(Range(0, 200), Tooltip("To reach it: people, at least"))
		int People = 0;
		HY_PROPERTY(Range(0, 50), Tooltip("... shelters (of any level), at least"))
		int Shelters = 0;
		HY_PROPERTY(Range(0, 30), Tooltip("... food in store, in days of eating for all, at least"))
		float FoodDays = 0.0f;
		HY_PROPERTY(Range(0, 100), Tooltip("... of the people's NeedObjectIds within Radius (old trees), at least"))
		int NeedObjects = 0;
		HY_PROPERTY(Range(0, 2000), Tooltip("... of the people's NeedTileIds within Radius (mountain rock), at least"))
		int NeedTiles = 0;
		HY_PROPERTY(Range(-1, 255), Tooltip("The shelter they build at this level (a tent, a hut); -1 none"))
		int ShelterObjectId = -1;
		HY_PROPERTY(Range(1, 20), Tooltip("People that shelter holds (the centre holds the first level's too)"))
		int PeoplePerShelter = 3;
		HY_PROPERTY(Range(0, 50), Tooltip("Wood it takes"))
		float WoodPerShelter = 6.0f;
		HY_PROPERTY(Range(1, 200), Tooltip("People the settlement holds at this level, at most"))
		int MaxPeople = 15;
		HY_PROPERTY(Range(1, 30), Tooltip("Days of eating they keep in store at this level, at most"))
		float StoreDays = 4.0f;
		HY_PROPERTY(Tooltip("One short line for the settlement panel: what the level gives"))
		std::string Description;
	};

	HY_DATA(Table, DisplayField(Name))
	struct PeopleDefinition
	{
		HY_PROPERTY()
		std::string Name;
		HY_PROPERTY(Range(1, 255), Tooltip("Unique, never reused"))
		int PeopleId = 1;
		HY_PROPERTY(Tooltip("Names their settlements get, in turn (Rename picks the next)"))
		std::vector<std::string> SettlementNames;
		HY_PROPERTY(Tooltip("Names their villagers get, in turn"))
		std::vector<std::string> VillagerNames;

		HY_PROPERTY(Asset(Texture2D), Tooltip("The settler sheet: rows idle, walk, work, each for SE, SW, NE, NW"))
		AssetHandle Sprite = 0;
		HY_PROPERTY(Range(4, 128), Tooltip("Sprite pixels per tile: the size they are drawn at"))
		float PixelsPerTile = 32.0f;
		HY_PROPERTY(Range(1, 64))
		int IdleFrames = 2;
		HY_PROPERTY(Range(1, 64))
		int WalkFrames = 4;
		HY_PROPERTY(Range(1, 64))
		int WorkFrames = 2;
		HY_PROPERTY(Range(0.05, 10), Tooltip("Tiles a second, walking"))
		float WalkSpeed = 0.8f;

		HY_PROPERTY(Range(-1, 255), Tooltip("What they put up at a settlement's centre (a fire, a lantern, a mine); -1 the world's fire"))
		int CentreObjectId = -1;
		HY_PROPERTY(Tooltip("What the centre is called in news: fire, lantern, mine"))
		std::string CentreName = "fire";
		HY_PROPERTY(Color, Tooltip("The centre's light: sparks and glow"))
		glm::vec3 GlowColor{ 1.0f, 0.72f, 0.25f };
		HY_PROPERTY(Tooltip("Smoke rises from the centre (a fire)"))
		bool Smoke = true;
		HY_PROPERTY(Range(0, 15), Tooltip("The centre goes on land at least this many levels high (a mine in the hills)"))
		int CentreMinLevel = 0;

		HY_PROPERTY(Range(1, 32), Tooltip("Tiles around the centre they use: water, plants, game, wood"))
		int Radius = 10;
		HY_PROPERTY(Range(0, 1), Tooltip("To settle: fresh water near, at least (1: 6 tiles or more)"))
		float NeedWater = 0.3f;
		HY_PROPERTY(Range(0, 1), Tooltip("... food: berries and game for a group, at least"))
		float NeedFood = 0.4f;
		HY_PROPERTY(Range(0, 1), Tooltip("... wood: trees for shelters, at least"))
		float NeedWood = 0.2f;
		HY_PROPERTY(Tooltip("... and objects that must stand within Radius (ObjectIds: the old trees elves settle by) ..."))
		std::vector<int> NeedObjectIds;
		HY_PROPERTY(Range(0, 100), Tooltip("... at least this many of them"))
		int NeedObjects = 0;
		HY_PROPERTY(Tooltip("What those objects are called on the settlement panel: old trees"))
		std::string NeedObjectsName;
		HY_PROPERTY(Tooltip("... and tiles that must lie within Radius (TileIds: the rock and snow of mountains for dwarves) ..."))
		std::vector<int> NeedTileIds;
		HY_PROPERTY(Range(0, 1000), Tooltip("... at least this many of them"))
		int NeedTiles = 0;
		HY_PROPERTY(Tooltip("What those tiles are called on the settlement panel: mountain rock"))
		std::string NeedTilesName;
		HY_PROPERTY(Range(0, 5), Tooltip("Of the places that meet their needs, the one they like most: water ..."))
		float LikesWater = 1.0f;
		HY_PROPERTY(Range(0, 5), Tooltip("... forest"))
		float LikesForest = 1.0f;
		HY_PROPERTY(Range(0, 5), Tooltip("... meadows"))
		float LikesMeadow = 1.0f;

		HY_PROPERTY(Range(0, 1), Tooltip("Their first group: chance a day it comes, once a talent lets them settle"))
		float ArrivePerDay = 0.5f;
		HY_PROPERTY(Range(0, 255), Tooltip("A branch of the people with this PeopleId: no groups come, their settlements become it"))
		int EvolvesFromId = 0;
		HY_PROPERTY(Range(0, 1), Tooltip("... chance a day, once a talent allows it and the land there meets this people's needs"))
		float EvolvePerDay = 0.1f;
		HY_PROPERTY(Range(0, 1), Tooltip("Each settlement they have makes a new group (at a new best place) this much less likely"))
		float SettleDecay = 0.5f;
		HY_PROPERTY(Range(4, 64), Tooltip("A new settlement keeps at least this many tiles from every other"))
		int SettleSpacing = 16;
		HY_PROPERTY(Range(1, 20), Tooltip("A group that comes: at least ..."))
		int GroupMin = 3;
		HY_PROPERTY(Range(1, 20), Tooltip("... and at most"))
		int GroupMax = 5;
		HY_PROPERTY(Range(0, 10), Tooltip("Branching out: a full settlement at least this level (0 the first) ..."))
		int BranchFromLevel = 1;
		HY_PROPERTY(Range(0, 1), Tooltip("... sends a group to found a new one nearby: chance a day"))
		float BranchPerDay = 0.05f;
		HY_PROPERTY(Range(8, 128), Tooltip("... at most this many tiles away"))
		int BranchReach = 40;

		HY_PROPERTY(Range(0, 10), Tooltip("Food each person eats a day"))
		float FoodPerPerson = 1.0f;
		HY_PROPERTY(Range(0, 10), Tooltip("Plant food (berries) a gatherer gathers a day, as far as the plants give it"))
		float GatherPerPerson = 2.0f;
		HY_PROPERTY(Range(0, 2), Tooltip("Animals a hunter takes a day, as far as the region has them"))
		float HuntPerPerson = 0.25f;
		HY_PROPERTY(Range(0, 50), Tooltip("Food an animal hunted gives"))
		float MeatPerAnimal = 6.0f;
		HY_PROPERTY(Tooltip("The AnimalIds they hunt and fish"))
		std::vector<int> HuntAnimalIds;
		HY_PROPERTY(Range(0, 20), Tooltip("Trees a woodcutter fells a day, while they need wood"))
		float FellPerDay = 1.0f;
		HY_PROPERTY(Tooltip("Saplings (ObjectIds) their foresters plant where they can grow; empty: they plant none"))
		std::vector<int> PlantObjectIds;
		HY_PROPERTY(Range(0, 20), Tooltip("Saplings a forester plants a day"))
		float PlantPerDay = 0.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("Share of the people who tend the forest as foresters (with saplings to plant)"))
		float ForesterShare = 0.0f;
		HY_PROPERTY(Range(-1, 255), Tooltip("Farming: the farmland (TileId) the player paints that their farmers sow; -1 they don't farm"))
		int FarmTileId = -1;
		HY_PROPERTY(Range(0, 10), Tooltip("... from this level of a settlement on (1: a hamlet)"))
		int FarmFromLevel = 1;
		HY_PROPERTY(Tooltip("... the crop's stages as objects (ObjectIds): sown, growing, ripe"))
		std::vector<int> CropObjectIds;
		HY_PROPERTY(Range(1, 60), Tooltip("... days from sowing to ripe on good soil (twice as rich: half) in a growing season"))
		float CropDays = 8.0f;
		HY_PROPERTY(Range(0, 50), Tooltip("... food a ripe field gives on plain soil (rich soil more, poor less)"))
		float CropFood = 6.0f;
		HY_PROPERTY(Range(0, 50), Tooltip("... fields a farmer sows a day (in a sowing season)"))
		float SowPerDay = 4.0f;
		HY_PROPERTY(Range(0, 50), Tooltip("... and harvests"))
		float HarvestPerDay = 6.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("... soil richness a harvest takes from its field (the soil builds up again in time)"))
		float SoilWear = 0.04f;
		HY_PROPERTY(Range(0, 1), Tooltip("Growth a day (share of the people) while food is plenty and there is room"))
		float BirthsPerDay = 0.08f;
		HY_PROPERTY(Range(0, 5), Tooltip("People who leave a day while there is no food"))
		float LeavePerDay = 0.5f;
		HY_PROPERTY(Tooltip("Their levels, the first where a settlement starts (camp, hamlet, village)"))
		std::vector<PeopleLevel> Levels;

		HY_PROPERTY(Tooltip("Peoples (PeopleIds) they can't stand: living near them builds tension, then quarrels, raids, fights"))
		std::vector<int> Dislikes;
		HY_PROPERTY(Range(0, 96), Tooltip("Tiles within which a disliked people's settlement builds tension (faster the closer)"))
		int DislikeRadius = 32;
		HY_PROPERTY(Range(0, 1), Tooltip("Tension (0 to 1) a day with a disliked settlement right beside them"))
		float TensionPerDay = 0.12f;
		HY_PROPERTY(Range(0, 1), Tooltip("Share of the people who stand guard while tension is high"))
		float GuardShare = 0.3f;
	};
}
