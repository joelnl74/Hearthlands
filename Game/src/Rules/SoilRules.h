#pragma once

#include "Data/ObjectDefinition.h"
#include "Data/TerrainTypeDefinition.h"
#include "State/Hydrology.h"
#include "State/ObjectLayer.h"
#include "State/Random.h"
#include "State/SoilState.h"
#include "State/TileMap.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// The soil: each tile's richness drifts towards its type's (Fertility) plus humus from the shrubs and trees on it and
	// silt from fresh water beside it; painting a type mixes its soil (and sand) in. Quality: richness times texture. Pure.
	class SoilRules
	{
	public:
		static constexpr float Neutral = 0.3f; // the quality that leaves chances as they are: plain dirt's
		static constexpr float Span = 0.4f;    // quality above Neutral at which a chance is 1 + SoilEffect times
		struct Settings
		{
			std::vector<float> Fertility = std::vector<float>(256, 0.3f); // per tile id (TerrainTypeDefinition::Fertility)
			std::vector<float> Sand = std::vector<float>(256, 0.2f);      // per tile id (TerrainTypeDefinition::Sand)
			std::vector<uint8_t> Living = std::vector<uint8_t>(256, 0);  // per tile id: 1 for a living cover (grass)
			float ShrubHumus = 0.1f;     // richness a shrub on the tile adds
			float TreeHumus = 0.2f;      // and a tree
			float Silt = 0.25f;          // richness at full moisture from fresh water (Carried), less further out
			float Bonus = 0.0f;          // added where life is (shrubs, trees, grass): the talents' SoilRichness
			float RisePerVisit = 0.04f;  // the most richness rises each time a tile is looked at
			float FallPerVisit = 0.02f;  // and falls: soil wears out slower than it builds up
			float PaintMix = 0.5f;       // share of a painted type's soil in the tile's
			float BestSand = 0.4f;       // the sand share plants like best (loam)
		};

		// The soil's quality where rules need it (growth, objects); without soil every tile is Neutral.
		struct Lookup
		{
			const SoilState* Soil = nullptr;
			const Settings* With = nullptr;

			float Quality(size_t index) const
			{
				return Soil != nullptr && With != nullptr ? QualityAt(*Soil, index, *With) : Neutral;
			}
		};

		static Settings FromTypes(const std::vector<TerrainTypeDefinition>& types, Settings settings)
		{
			for (const TerrainTypeDefinition& type : types)
			{
				if (type.TileId >= 0 && type.TileId < 256)
				{
					settings.Fertility[(size_t)type.TileId] = Clamp01(type.Fertility);
					settings.Sand[(size_t)type.TileId] = Clamp01(type.Sand);
					settings.Living[(size_t)type.TileId] = type.Living ? (uint8_t)1 : (uint8_t)0;
				}
			}
			return settings;
		}

		// Per object id: 1 for shrubs, 2 for trees, 0 otherwise (what humus they give).
		static std::vector<uint8_t> CoverOf(const std::vector<ObjectDefinition>& objects)
		{
			std::vector<uint8_t> cover(256, 0);
			for (const ObjectDefinition& object : objects)
			{
				if (object.ObjectId > 0 && object.ObjectId < 256)
				{
					cover[(size_t)object.ObjectId] = object.Kind == ObjectKind::Tree ? 2 : (object.Kind == ObjectKind::Shrub ? 1 : 0);
				}
			}
			return cover;
		}

		// A new map's soil: each tile its type's.
		static SoilState Start(const TileMap& map, const Settings& settings)
		{
			SoilState soil;
			const std::vector<uint8_t>& tiles = map.GetTiles();
			soil.Fertility.resize(tiles.size());
			soil.Sand.resize(tiles.size());
			for (size_t i = 0; i < tiles.size(); i++)
			{
				soil.Fertility[i] = ToByte(settings.Fertility[tiles[i]]);
				soil.Sand[i] = ToByte(settings.Sand[tiles[i]]);
			}
			soil.Revision++;
			return soil;
		}

		// How plants like a sand share: best at BestSand (loam), less towards heavy (no sand) and sandy soil.
		static float Texture(float sand, const Settings& settings)
		{
			return std::max(0.4f, 1.2f - std::fabs(sand - settings.BestSand));
		}

		// How good the soil is for plants, 0 to 1: richness times texture.
		static float Quality(float fertility, float sand, const Settings& settings)
		{
			return Clamp01(fertility * Texture(sand, settings));
		}

		static float QualityAt(const SoilState& soil, size_t index, const Settings& settings)
		{
			if (index >= soil.Fertility.size() || index >= soil.Sand.size())
			{
				return Neutral; // no soil: as if plain dirt
			}
			return Quality((float)soil.Fertility[index] / 255.0f, (float)soil.Sand[index] / 255.0f, settings);
		}

		// What a chance becomes on soil of this quality: as it is on Neutral (plain dirt), times 1 + effect Span above it,
		// less on poorer soil (sand). Never below 0.
		static float Factor(float effect, float quality)
		{
			return std::max(0.0f, 1.0f + effect * (quality - Neutral) / Span);
		}

		// The richness a tile drifts to: its type's, humus from its shrub or tree (and the talents' bonus where life is),
		// silt from fresh water beside it. cover: 0 none, 1 shrub, 2 tree.
		static float Target(const Settings& settings, uint8_t tile, uint8_t cover, float carried)
		{
			float target = settings.Fertility[tile];
			target += cover == 2 ? settings.TreeHumus : (cover == 1 ? settings.ShrubHumus : 0.0f);
			target += cover != 0 || settings.Living[tile] != 0 ? settings.Bonus : 0.0f;
			target += settings.Silt * Clamp01(carried);
			return Clamp01(target);
		}

		// The brush turned the tile into `tile`: its soil mixes in (PaintMix), so dirt over sand makes loam.
		static void Paint(SoilState& soil, size_t index, uint8_t tile, const Settings& settings)
		{
			if (index >= soil.Fertility.size() || index >= soil.Sand.size())
			{
				return;
			}
			const float mix = Clamp01(settings.PaintMix);
			const float fertility = (float)soil.Fertility[index] / 255.0f;
			const float sand = (float)soil.Sand[index] / 255.0f;
			soil.Fertility[index] = ToByte(fertility + (settings.Fertility[tile] - fertility) * mix);
			soil.Sand[index] = ToByte(sand + (settings.Sand[tile] - sand) * mix);
			soil.Revision++;
		}

		// Looks at `samples` random land tiles: each one's richness moves towards its Target, by at most RisePerVisit up or
		// FallPerVisit down. Water tiles keep theirs. Returns how many changed.
		static int Step(SoilState& soil, const TileMap& map, const Hydrology& water, const ObjectLayer& objects,
			const std::vector<uint8_t>& cover, const std::vector<uint8_t>& land, int samples, const Settings& settings, Random& random)
		{
			const uint32_t count = (uint32_t)map.GetTiles().size();
			if (soil.Fertility.size() != count)
			{
				return 0;
			}
			int changed = 0;
			for (int i = 0; i < samples && count > 0; i++)
			{
				const uint32_t index = random.Below(count);
				const uint8_t tile = map.GetTiles()[index];
				if ((land.empty() == false && land[index] == 0) || tile == water.WaterTileId)
				{
					continue;
				}
				const uint8_t object = objects.Get(index);
				const uint8_t covered = object < cover.size() ? cover[object] : (uint8_t)0;
				const float carried = index < water.Carried.size() ? (float)water.Carried[index] / 255.0f : 0.0f;
				const float target = Target(settings, tile, covered, carried);
				const float now = (float)soil.Fertility[index] / 255.0f;
				const float next = target > now ? std::min(target, now + settings.RisePerVisit)
					: std::max(target, now - settings.FallPerVisit);
				const uint8_t stored = ToByte(next);
				if (stored != soil.Fertility[index])
				{
					soil.Fertility[index] = stored;
					changed++;
				}
			}
			soil.Revision += changed > 0 ? 1u : 0u;
			return changed;
		}

		// Per tile the quality as a byte (the soil view), and how many land tiles reach `rich` (a milestone).
		static uint32_t Qualities(const SoilState& soil, const Settings& settings, std::vector<uint8_t>& out, float rich = 2.0f)
		{
			out.resize(soil.Fertility.size());
			uint32_t good = 0;
			for (size_t i = 0; i < out.size(); i++)
			{
				const float quality = QualityAt(soil, i, settings);
				out[i] = ToByte(quality);
				good += quality >= rich ? 1u : 0u;
			}
			return good;
		}

		static uint8_t ToByte(float value)
		{
			return (uint8_t)(Clamp01(value) * 255.0f + 0.5f);
		}

		static float Clamp01(float value)
		{
			return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
		}
	};
}
