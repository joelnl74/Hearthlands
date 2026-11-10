#pragma once

#include "Hydro/Asset/Asset.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

#include <cstdint>
#include <string>

// The things standing on tiles (bushes, trees, reeds, rocks) as data: one row per kind and growth stage in
// Assets/Data/Objects.hytable. The object layer stores ObjectId per tile.
namespace Hydro
{
	// How big an object is, smallest first: MinSpacing only counts objects of the same kind or bigger, so saplings can
	// come up between bushes but not next to other trees.
	HY_ENUM()
	enum class ObjectKind : uint8_t
	{
		Ground, // reeds, flowers
		Shrub,  // bushes
		Tree,
		Rock
	};

	HY_DATA(Table, DisplayField(Name))
	struct ObjectDefinition
	{
		HY_PROPERTY()
		std::string Name;
		HY_PROPERTY(Range(1, 255), Tooltip("The number the object layer stores (0 is none); unique, never reused"))
		int ObjectId = 1;
		HY_PROPERTY()
		ObjectKind Kind = ObjectKind::Shrub;
		HY_PROPERTY(Asset(Texture2D), Tooltip("Drawn standing on its tile, bottom centre on the ground"))
		AssetHandle Sprite = 0;
		HY_PROPERTY(Range(4, 128), Tooltip("Sprite pixels per tile: the size it is drawn at"))
		float PixelsPerTile = 24.0f;
		HY_PROPERTY()
		bool CastsShadow = true;
		HY_PROPERTY(Range(-1, 255), Tooltip("The tile it lives on; -1 any land (not water or riverbed). It dies elsewhere"))
		int GrowsOnTileId = 2;
		HY_PROPERTY(Range(0, 1), Tooltip("The tile's moisture it lives in, at least ..."))
		float MinMoisture = 0.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("... and at most"))
		float MaxMoisture = 1.0f;
		HY_PROPERTY(Range(-1, 255), Tooltip("Needs MinNeighbours of the 8 neighbouring tiles to be this id; -1 none"))
		int NeighbourTileId = -1;
		HY_PROPERTY(Range(1, 8))
		int MinNeighbours = 1;
		HY_PROPERTY(Range(0, 1), Tooltip("Chance to appear each time an empty tile that suits it is looked at"))
		float SproutChance = 0.0f;
		HY_PROPERTY(Range(0, 4), Tooltip("Tiles around it free of objects of its kind or bigger for it to appear"))
		int MinSpacing = 0;
		HY_PROPERTY(Range(-1, 255), Tooltip("Spreads from this object within MinSpacing + 1 tiles; -1 none"))
		int SpreadFromObjectId = -1;
		HY_PROPERTY(Range(0, 1), Tooltip("Extra chance to appear next to SpreadFromObjectId"))
		float SpreadChance = 0.0f;
		HY_PROPERTY(Range(-1, 255), Tooltip("Becomes this object after GrowDays; -1 stays"))
		int GrowsIntoObjectId = -1;
		HY_PROPERTY(Range(0, 100), Tooltip("World days (on average) before it grows into the next stage"))
		float GrowDays = 0.0f;
		HY_PROPERTY(Range(0, 1), Tooltip("Placed on this share of the tiles that suit it when the world is made"))
		float StartDensity = 0.0f;
	};
}
