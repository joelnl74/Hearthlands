#pragma once

#include "Rules/TerrainGenRules.h"

#include <cstdint>
#include <vector>

namespace Hydro
{
	// The hidden water under the land: a value per tile (0 to 255) from noise, in patches. A riverbed dug where it reaches
	// SpringFrom becomes a spring (a source, stronger the higher it is); it keeps the ground above a little damp.
	class GroundwaterRules
	{
	public:
		struct Settings
		{
			float Scale = 40.0f;        // tiles across a patch of the noise
			float Contrast = 1.5f;      // spreads the noise (mostly near 0.5): higher, bigger and sharper patches
			float SpringFrom = 0.78f;   // groundwater (0 to 1) from which a dug riverbed springs (about 1 tile in 11)
			float MinSpringHead = 6.0f; // a spring's head at SpringFrom (tiles of narrow channel it fills) ...
			float SpringHead = 20.0f;   // ... and at full groundwater
			float Moisture = 0.12f;     // moisture (0 to 1) full groundwater gives the land above it, from SpringFrom up
		};

		// Groundwater per tile, row by row like TileMap's tiles, from the seed (its own noise, apart from the terrain's).
		static std::vector<uint8_t> Generate(int width, int height, uint32_t seed, const Settings& settings)
		{
			std::vector<uint8_t> groundwater((size_t)(width > 0 ? width : 0) * (size_t)(height > 0 ? height : 0), 0);
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					const float noise = TerrainGenRules::Noise(x, y, settings.Scale, 3, seed * 2654435761u + 0x47574Bu);
					const float spread = (noise - 0.5f) * settings.Contrast + 0.5f;
					const float value = spread < 0.0f ? 0.0f : (spread > 1.0f ? 1.0f : spread);
					groundwater[(size_t)y * (size_t)width + (size_t)x] = (uint8_t)(value * 255.0f + 0.5f);
				}
			}
			return groundwater;
		}

		// How far groundwater is above `from` (0 to 1 groundwater): 0 at or below it, 1 at full.
		static float Above(uint8_t groundwater, float from)
		{
			const float value = (float)groundwater / 255.0f;
			if (value < from || from >= 1.0f)
			{
				return 0.0f;
			}
			return (value - from) / (1.0f - from);
		}

		static bool Springs(uint8_t groundwater, const Settings& settings)
		{
			return (float)groundwater / 255.0f >= settings.SpringFrom;
		}

		// The head of a spring dug here: MinSpringHead at SpringFrom up to SpringHead at full groundwater.
		static float SpringHead(uint8_t groundwater, const Settings& settings)
		{
			return settings.MinSpringHead + (settings.SpringHead - settings.MinSpringHead) * Above(groundwater, settings.SpringFrom);
		}

		// The moisture (0 to 255) groundwater gives the land above it: none below SpringFrom, Moisture at full.
		static uint8_t Wetness(uint8_t groundwater, const Settings& settings)
		{
			return (uint8_t)(Above(groundwater, settings.SpringFrom) * settings.Moisture * 255.0f + 0.5f);
		}

		// How strongly the surface shows the groundwater (0 to 63, darker damp ground): from clueFrom up, times clarity
		// (0 barely, 1 plainly). Talents raise clarity (clearer clues) and lower clueFrom (weaker spots show too).
		static uint8_t Clue(uint8_t groundwater, float clarity, float clueFrom)
		{
			const float shown = clarity < 0.0f ? 0.0f : (clarity > 1.0f ? 1.0f : clarity);
			return (uint8_t)(Above(groundwater, clueFrom) * shown * 63.0f + 0.5f);
		}
	};
}
