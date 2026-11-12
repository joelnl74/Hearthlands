#pragma once

#include "Data/TerrainTypeDefinition.h"
#include "Rules/GroundwaterRules.h"
#include "State/HeightMap.h"
#include "State/Hydrology.h"
#include "State/TileMap.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// How water moves and wets the land: a tile's head is its strongest water neighbour's (or meltwater's) less a loss; riverbed
	// fills while that is above 0, or springs over groundwater. Fresh water, snowmelt and groundwater wet the land.
	class WaterRules
	{
	public:
		struct Settings
		{
			uint8_t WaterTileId = 3;
			uint8_t BedTileId = 5;
			float SourceHead = 24.0f; // how far a narrow channel carries water from a source, in tiles
			float MeltHead = 24.0f;   // the same from meltwater: a channel next to a Melt tile fills from it
			float MeltMoisture = 0.0f; // moisture (0 to 1) meltwater gives the land right below the snow (Melt), never passed on
			float RainPerLevel = 0.0f; // moisture (0 to 1) each level of height gives the land, never passed on
			GroundwaterRules::Settings Groundwater; // springs and the damp above high groundwater (Hydrology::Groundwater)
			float Loss = 1.0f;        // head lost per tile in a one-tile-wide channel; wider ones lose less
			float BankHead = 4.0f;    // head a wide channel's tiles by its banks need to hold water: its end comes out rounded
			int MoistureReach = 6;    // tiles over which moisture fades from fresh water to nothing
			int MaxPerTick = 40000;   // active tiles looked at per tick; the rest wait
			int ShelterNeighbours = 5; // sheltering neighbours (of 8) that wall a tile in; 5 or more never pushes a straight edge
			std::array<uint8_t, 256> Shelter{}; // per tile id: moisture (0 to 255) a tile walled in by it keeps; 0 shelters nothing
		};

		struct TickReport
		{
			int Filled = 0;  // riverbed tiles that took water
			int Dried = 0;   // water tiles that dried
			int Sprung = 0;  // riverbed dug down to groundwater that became a spring
			int Looked = 0;  // active tiles looked at
			std::vector<uint32_t> Changes; // the map's changes taken this tick (brush, growth): for other rules to react
		};

		// `settings` with the water and riverbed ids from the types' roles (unchanged when a role has no type) and their shelter.
		static Settings FromTypes(const std::vector<TerrainTypeDefinition>& types, Settings settings)
		{
			for (const TerrainTypeDefinition& type : types)
			{
				if (type.TileId >= 0 && type.TileId <= 255)
				{
					const float shelter = type.Shelter < 0.0f ? 0.0f : (type.Shelter > 1.0f ? 1.0f : type.Shelter);
					settings.Shelter[(size_t)type.TileId] = (uint8_t)(shelter * 255.0f + 0.5f);
				}
				if (type.Role == TerrainRole::Water)
				{
					settings.WaterTileId = (uint8_t)type.TileId;
				}
				else if (type.Role == TerrainRole::Riverbed)
				{
					settings.BedTileId = (uint8_t)type.TileId;
				}
			}
			return settings;
		}

		// Fields for a new map: every water tile is a source at SourceHead; moisture fades from the fresh ones (land[i] != 0, empty
		// = all). heights give rain, groundwater springs where dug and dampens the land (nullptr: flat, none).
		static Hydrology Start(const TileMap& map, const std::vector<uint8_t>& land, const Settings& settings,
			const HeightMap* heights = nullptr, const std::vector<uint8_t>* groundwater = nullptr)
		{
			const size_t count = map.GetTiles().size();
			Hydrology water;
			water.WaterTileId = settings.WaterTileId;
			water.BedTileId = settings.BedTileId;
			water.Head.assign(count, 0.0f);
			water.Source.assign(count, 0);
			water.Carried.assign(count, 0);
			water.Moisture.assign(count, 0);
			water.Queued.assign(count, 0);
			std::vector<int> carried(count, 0);
			for (size_t i = 0; i < count; i++)
			{
				if (map.GetTiles()[i] == settings.WaterTileId)
				{
					water.Head[i] = settings.SourceHead;
					water.Source[i] = 1;
					carried[i] = IsFresh(land, i) ? 255 : 0;
				}
			}
			// Moisture fades with the distance to fresh water (diagonal steps cost 1.41), as TargetMoisture keeps it: one pass
			// down and one up settle it.
			const int width = map.GetWidth();
			const int height = map.GetHeight();
			const int step = 255 / (settings.MoistureReach > 0 ? settings.MoistureReach : 1);
			const int diagonal = DiagonalStep(step);
			for (int pass = 0; pass < 2; pass++)
			{
				const int dir = pass == 0 ? 1 : -1;
				for (int row = 0; row < height; row++)
				{
					const int y = pass == 0 ? row : height - 1 - row;
					for (int column = 0; column < width; column++)
					{
						const int x = pass == 0 ? column : width - 1 - column;
						int& here = carried[(size_t)y * (size_t)width + (size_t)x];
						const int before[4][3] = { { -dir, 0, step }, { -dir, -dir, diagonal }, { 0, -dir, step },
							{ dir, -dir, diagonal } };
						for (const auto& from : before)
						{
							if (map.InBounds(x + from[0], y + from[1]))
							{
								const int value = carried[(size_t)(y + from[1]) * (size_t)width + (size_t)(x + from[0])] - from[2];
								here = value > here ? value : here;
							}
						}
					}
				}
			}
			for (size_t i = 0; i < count; i++)
			{
				water.Carried[i] = (uint8_t)(carried[i] > 0 ? carried[i] : 0);
			}
			if (heights != nullptr && heights->GetLevels().size() == count)
			{
				water.Rain.assign(count, 0);
				for (size_t i = 0; i < count; i++)
				{
					water.Rain[i] = RainAt(heights->GetLevels()[i], settings);
				}
			}
			if (groundwater != nullptr && groundwater->size() == count)
			{
				water.Groundwater = *groundwater;
			}
			for (size_t i = 0; i < count; i++)
			{
				water.Moisture[i] = ShelteredMoisture(map, water, (int)(i % (size_t)width), (int)(i / (size_t)width), settings);
			}
			return water;
		}

		// One tick: the map's changes (brush, growth, last tick's water) wake their tiles and neighbours, then every
		// woken tile is looked at once. Changes it makes wake their neighbours for the next tick.
		static TickReport Step(TileMap& map, Hydrology& water, const std::vector<uint8_t>& land, const Settings& settings)
		{
			TickReport report;
			const int width = map.GetWidth();
			report.Changes = map.TakeChanges();
			for (uint32_t index : report.Changes)
			{
				ActivateAround(map, water, index, 2); // FillHead looks 2 tiles out
			}

			std::vector<uint32_t> batch;
			if ((int)water.Active.size() <= settings.MaxPerTick)
			{
				batch.swap(water.Active);
			}
			else
			{
				batch.assign(water.Active.begin(), water.Active.begin() + settings.MaxPerTick);
				water.Active.erase(water.Active.begin(), water.Active.begin() + settings.MaxPerTick);
			}
			for (uint32_t index : batch)
			{
				water.Queued[index] = 0;
			}

			// Decide every tile from the state at the start of the tick, then apply, so water moves one tile per tick.
			struct Update
			{
				uint32_t Index;
				uint8_t Tile;
				float Head;
				uint8_t Carried;
				uint8_t Moisture;
				uint8_t Source;
			};
			std::vector<Update> updates;
			for (uint32_t index : batch)
			{
				report.Looked++;
				const int x = (int)(index % (uint32_t)width);
				const int y = (int)(index / (uint32_t)width);
				const uint8_t tile = map.GetTiles()[index];
				if (tile != settings.WaterTileId)
				{
					water.Source[index] = 0; // a lake filled in by the player stops being a source
				}
				const bool sealed = index < water.Sealed.size() && water.Sealed[index] != 0;
				if (sealed && tile != settings.BedTileId && tile != settings.WaterTileId)
				{
					water.Sealed[index] = 0; // filled in by the player: dug again it is an ordinary bed
				}

				Update update{ index, tile, water.Head[index], water.Carried[index], water.Moisture[index], water.Source[index] };
				if (tile == settings.WaterTileId && water.Source[index] == 0)
				{
					const float head = FlowHead(map, water, x, y, settings);
					const bool holds = head > FillHead(map, x, y, settings);
					update.Tile = holds ? tile : settings.BedTileId;
					update.Head = holds ? head : 0.0f;
				}
				else if (tile == settings.BedTileId && sealed == false && index < water.Groundwater.size()
					&& GroundwaterRules::Springs(water.Groundwater[index], settings.Groundwater))
				{
					update.Tile = settings.WaterTileId; // dug down to the groundwater: a spring
					update.Head = GroundwaterRules::SpringHead(water.Groundwater[index], settings.Groundwater);
					update.Source = 1;
				}
				else if (tile == settings.BedTileId)
				{
					const float head = FlowHead(map, water, x, y, settings);
					const bool holds = head > FillHead(map, x, y, settings);
					update.Tile = holds ? settings.WaterTileId : tile;
					update.Head = holds ? head : 0.0f;
				}
				else if (tile != settings.WaterTileId)
				{
					update.Head = 0.0f;
				}
				update.Carried = TargetMoisture(map, water, land, x, y, settings);
				update.Moisture = ShelteredMoisture(map, water, x, y, settings, update.Carried);
				const bool changed = update.Tile != tile || std::abs(update.Head - water.Head[index]) > 1e-3f
					|| update.Carried != water.Carried[index] || update.Moisture != water.Moisture[index]
					|| update.Source != water.Source[index];
				if (changed)
				{
					updates.push_back(update);
				}
			}

			for (const Update& update : updates)
			{
				const int x = (int)(update.Index % (uint32_t)width);
				const int y = (int)(update.Index / (uint32_t)width);
				// Only what neighbours read (tile, head, carried moisture) wakes them; the shown moisture is the tile's own.
				const bool wakes = update.Tile != map.GetTiles()[update.Index] || std::abs(update.Head - water.Head[update.Index]) > 1e-3f
					|| update.Carried != water.Carried[update.Index];
				if (update.Tile != map.GetTiles()[update.Index])
				{
					report.Filled += update.Tile == settings.WaterTileId ? 1 : 0;
					report.Sprung += update.Source != 0 && water.Source[update.Index] == 0 ? 1 : 0;
					report.Dried += update.Tile == settings.BedTileId ? 1 : 0;
					map.Set(x, y, update.Tile);
				}
				water.Head[update.Index] = update.Head;
				water.Source[update.Index] = update.Source;
				water.Carried[update.Index] = update.Carried;
				water.Moisture[update.Index] = update.Moisture;
				if (wakes)
				{
					ActivateAround(map, water, update.Index);
				}
			}
			return report;
		}

		// The head water would have here: the strongest neighbouring water's head (a Melt tile: MeltHead) less the loss for the
		// channel's width (water and riverbed neighbours: 2 or fewer lose Loss, 8 lose a quarter of it).
		static float FlowHead(const TileMap& map, const Hydrology& water, int x, int y, const Settings& settings)
		{
			float strongest = 0.0f;
			int channel = 0;
			for (int oy = -1; oy <= 1; oy++)
			{
				for (int ox = -1; ox <= 1; ox++)
				{
					if ((ox == 0 && oy == 0) || map.InBounds(x + ox, y + oy) == false)
					{
						continue;
					}
					const uint8_t tile = map.Get(x + ox, y + oy);
					const size_t next = (size_t)(y + oy) * (size_t)map.GetWidth() + (size_t)(x + ox);
					if (tile == settings.WaterTileId || tile == settings.BedTileId)
					{
						channel++;
					}
					if (tile == settings.WaterTileId)
					{
						strongest = water.Head[next] > strongest ? water.Head[next] : strongest;
					}
					else if (next < water.Melt.size() && water.Melt[next] != 0)
					{
						strongest = settings.MeltHead > strongest ? settings.MeltHead : strongest;
					}
				}
			}
			if (strongest <= 0.0f)
			{
				return 0.0f;
			}
			return strongest - settings.Loss * 2.0f / (float)(channel > 2 ? channel : 2);
		}

		// The head a channel tile needs to hold water: 0 in narrow channels (under 5 channel neighbours) and amid wide ones, up
		// to BankHead by a wide one's banks (by the land within 2 tiles), so a wide river's end is rounded, not cut straight.
		static float FillHead(const TileMap& map, int x, int y, const Settings& settings)
		{
			int channel = 0;
			int land = 0;
			for (int oy = -2; oy <= 2; oy++)
			{
				for (int ox = -2; ox <= 2; ox++)
				{
					if (ox == 0 && oy == 0)
					{
						continue;
					}
					const bool inside = map.InBounds(x + ox, y + oy); // off the map counts as land
					const uint8_t tile = inside ? map.Get(x + ox, y + oy) : (uint8_t)0;
					const bool wet = inside && (tile == settings.WaterTileId || tile == settings.BedTileId);
					const bool ring = ox >= -1 && ox <= 1 && oy >= -1 && oy <= 1;
					channel += wet && ring ? 1 : 0;
					land += wet ? 0 : 1;
				}
			}
			return channel < 5 ? 0.0f : settings.BankHead * (float)land / 24.0f;
		}

		// The moisture carried from water: 255 on fresh water; elsewhere the wettest neighbour's carried moisture less a step of
		// the reach (1.41 steps from a diagonal neighbour, so wet ground ends in curves, not squares).
		static uint8_t TargetMoisture(const TileMap& map, const Hydrology& water, const std::vector<uint8_t>& land, int x, int y,
			const Settings& settings)
		{
			const size_t index = (size_t)y * (size_t)map.GetWidth() + (size_t)x;
			if (map.GetTiles()[index] == settings.WaterTileId && IsFresh(land, index))
			{
				return 255;
			}
			const int step = 255 / (settings.MoistureReach > 0 ? settings.MoistureReach : 1);
			const int diagonal = DiagonalStep(step);
			int wettest = 0;
			for (int oy = -1; oy <= 1; oy++)
			{
				for (int ox = -1; ox <= 1; ox++)
				{
					if ((ox != 0 || oy != 0) && map.InBounds(x + ox, y + oy))
					{
						const int value = water.Carried[(size_t)(y + oy) * (size_t)map.GetWidth() + (size_t)(x + ox)]
							- (ox != 0 && oy != 0 ? diagonal : step);
						wettest = value > wettest ? value : wettest;
					}
				}
			}
			return (uint8_t)wettest;
		}

		// What it carries (`carried`, else water.Carried), or its rain, snowmelt or groundwater damp if more; walled in by
		// ShelterNeighbours or more wet sheltering tiles (grass), at least their mean carried plus mean Shelter. None passed on.
		static uint8_t ShelteredMoisture(const TileMap& map, const Hydrology& water, int x, int y, const Settings& settings,
			int carried = -1)
		{
			const size_t index = (size_t)y * (size_t)map.GetWidth() + (size_t)x;
			const int carriedHere = carried >= 0 ? carried : (int)water.Carried[index];
			if (map.GetTiles()[index] == settings.WaterTileId)
			{
				return (uint8_t)carriedHere;
			}
			const int rain = index < water.Rain.size() ? (int)water.Rain[index] : 0;
			const int melt = index < water.Melt.size() ? MeltWetness(water.Melt[index], settings) : 0;
			const int ground = index < water.Groundwater.size()
				? GroundwaterRules::Wetness(water.Groundwater[index], settings.Groundwater) : 0;
			const int fed = rain > melt ? (rain > ground ? rain : ground) : (melt > ground ? melt : ground);
			const int own = fed > carriedHere ? fed : carriedHere;
			int walls = 0;
			int wet = 0;
			int shelter = 0;
			for (int oy = -1; oy <= 1; oy++)
			{
				for (int ox = -1; ox <= 1; ox++)
				{
					if ((ox == 0 && oy == 0) || map.InBounds(x + ox, y + oy) == false)
					{
						continue;
					}
					const uint8_t tile = map.Get(x + ox, y + oy);
					if (settings.Shelter[tile] > 0)
					{
						walls++;
						wet += water.Carried[(size_t)(y + oy) * (size_t)map.GetWidth() + (size_t)(x + ox)];
						shelter += settings.Shelter[tile];
					}
				}
			}
			if (walls < settings.ShelterNeighbours || walls == 0 || wet < walls)
			{
				return (uint8_t)own; // not walled in, or the walls are dry (mean carried below 1)
			}
			const int value = wet / walls + shelter / walls;
			const int kept = value < 255 ? value : 255;
			return (uint8_t)(kept > own ? kept : own);
		}

		// Sets the rain of the listed tiles (empty: all) from their height and wakes those whose rain changed.
		static void UpdateRain(const TileMap& map, Hydrology& water, const HeightMap& heights, const std::vector<uint32_t>& tiles,
			const Settings& settings)
		{
			const size_t count = map.GetTiles().size();
			if (heights.GetLevels().size() != count)
			{
				return;
			}
			if (water.Rain.size() != count)
			{
				water.Rain.assign(count, 0);
			}
			const auto update = [&](uint32_t index)
			{
				const uint8_t rain = RainAt(heights.GetLevels()[index], settings);
				if (rain != water.Rain[index])
				{
					water.Rain[index] = rain;
					water.Activate(index);
				}
			};
			if (tiles.empty())
			{
				for (uint32_t i = 0; i < (uint32_t)count; i++)
				{
					update(i);
				}
			}
			for (uint32_t index : tiles)
			{
				if (index < count)
				{
					update(index);
				}
			}
		}

		// Takes new meltwater (MountainRules::Meltwater) and wakes the tiles around every tile where it changed.
		static void UpdateMelt(const TileMap& map, Hydrology& water, std::vector<uint8_t> melt)
		{
			const size_t count = map.GetTiles().size();
			if (melt.size() != count)
			{
				return;
			}
			for (size_t i = 0; i < count; i++)
			{
				const uint8_t old = i < water.Melt.size() ? water.Melt[i] : (uint8_t)0;
				if (old != melt[i])
				{
					ActivateAround(map, water, (uint32_t)i);
				}
			}
			water.Melt.swap(melt);
		}

	private:
		// The moisture meltwater (0 to 255, MountainRules::Meltwater) gives the land: up to MeltMoisture at the snow.
		static int MeltWetness(uint8_t melt, const Settings& settings)
		{
			const float share = settings.MeltMoisture < 0.0f ? 0.0f : (settings.MeltMoisture > 1.0f ? 1.0f : settings.MeltMoisture);
			return (int)((float)melt * share + 0.5f);
		}

		static uint8_t RainAt(uint8_t level, const Settings& settings)
		{
			const float rain = (float)level * settings.RainPerLevel;
			return (uint8_t)(rain >= 1.0f ? 255 : (rain <= 0.0f ? 0 : (int)(rain * 255.0f + 0.5f)));
		}

		static bool IsFresh(const std::vector<uint8_t>& land, size_t index)
		{
			return land.empty() || (index < land.size() && land[index] != 0);
		}

		static int DiagonalStep(int step)
		{
			return (step * 1414 + 500) / 1000;
		}

		static void ActivateAround(const TileMap& map, Hydrology& water, uint32_t index, int radius = 1)
		{
			const int width = map.GetWidth();
			const int x = (int)(index % (uint32_t)width);
			const int y = (int)(index / (uint32_t)width);
			for (int oy = -radius; oy <= radius; oy++)
			{
				for (int ox = -radius; ox <= radius; ox++)
				{
					if (map.InBounds(x + ox, y + oy))
					{
						water.Activate((uint32_t)((y + oy) * width + x + ox));
					}
				}
			}
		}
	};
}
