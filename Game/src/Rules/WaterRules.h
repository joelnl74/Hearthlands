#pragma once

#include "Data/TerrainTypeDefinition.h"
#include "State/Hydrology.h"
#include "State/TileMap.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// How water moves and wets the land: a tile's head is its strongest water neighbour's less a loss (smaller in wide channels);
	// riverbed fills while that is above 0, water dries below it. Fresh water wets the land; one tile per tick (Hydrology::Active).
	class WaterRules
	{
	public:
		struct Settings
		{
			uint8_t WaterTileId = 3;
			uint8_t BedTileId = 5;
			float SourceHead = 24.0f; // how far a narrow channel carries water from a source, in tiles
			float Loss = 1.0f;        // head lost per tile in a one-tile-wide channel; wider ones lose less
			int MoistureReach = 6;    // tiles over which moisture fades from fresh water to nothing
			int MaxPerTick = 40000;   // active tiles looked at per tick; the rest wait
		};

		struct TickReport
		{
			int Filled = 0;  // riverbed tiles that took water
			int Dried = 0;   // water tiles that dried
			int Looked = 0;  // active tiles looked at
			std::vector<uint32_t> Changes; // the map's changes taken this tick (brush, growth): for other rules to react
		};

		// `settings` with the water and riverbed ids from the types' roles (unchanged when a role has no type).
		static Settings FromTypes(const std::vector<TerrainTypeDefinition>& types, Settings settings)
		{
			for (const TerrainTypeDefinition& type : types)
			{
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

		// Fields for a new map: every water tile now is a source at SourceHead; moisture from the fresh ones (land[i] != 0;
		// land empty = all fresh) fades with the steps to them.
		static Hydrology Start(const TileMap& map, const std::vector<uint8_t>& land, const Settings& settings)
		{
			const size_t count = map.GetTiles().size();
			Hydrology water;
			water.WaterTileId = settings.WaterTileId;
			water.BedTileId = settings.BedTileId;
			water.Head.assign(count, 0.0f);
			water.Source.assign(count, 0);
			water.Moisture.assign(count, 0);
			water.Queued.assign(count, 0);
			std::vector<uint32_t> queue;
			std::vector<int> steps(count, -1);
			for (size_t i = 0; i < count; i++)
			{
				if (map.GetTiles()[i] == settings.WaterTileId)
				{
					water.Head[i] = settings.SourceHead;
					water.Source[i] = 1;
					if (IsFresh(land, i))
					{
						steps[i] = 0;
						queue.push_back((uint32_t)i);
					}
				}
			}
			const int width = map.GetWidth();
			for (size_t head = 0; head < queue.size(); head++)
			{
				const int x = (int)(queue[head] % (uint32_t)width);
				const int y = (int)(queue[head] / (uint32_t)width);
				for (int oy = -1; oy <= 1; oy++)
				{
					for (int ox = -1; ox <= 1; ox++)
					{
						const int nx = x + ox;
						const int ny = y + oy;
						const size_t next = (size_t)ny * (size_t)width + (size_t)nx;
						if (map.InBounds(nx, ny) && steps[next] < 0 && steps[queue[head]] < settings.MoistureReach)
						{
							steps[next] = steps[queue[head]] + 1;
							queue.push_back((uint32_t)next);
						}
					}
				}
			}
			for (size_t i = 0; i < count; i++)
			{
				water.Moisture[i] = steps[i] < 0 ? (uint8_t)0 : MoistureAt(steps[i], settings);
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
				ActivateAround(map, water, index);
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
				uint8_t Moisture;
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

				Update update{ index, tile, water.Head[index], water.Moisture[index] };
				if (tile == settings.WaterTileId && water.Source[index] == 0)
				{
					const float head = FlowHead(map, water, x, y, settings);
					update.Tile = head <= 0.0f ? settings.BedTileId : tile;
					update.Head = head <= 0.0f ? 0.0f : head;
				}
				else if (tile == settings.BedTileId)
				{
					const float head = FlowHead(map, water, x, y, settings);
					update.Tile = head > 0.0f ? settings.WaterTileId : tile;
					update.Head = head > 0.0f ? head : 0.0f;
				}
				else if (tile != settings.WaterTileId)
				{
					update.Head = 0.0f;
				}
				update.Moisture = TargetMoisture(map, water, land, x, y, settings);
				const bool changed = update.Tile != tile || std::abs(update.Head - water.Head[index]) > 1e-3f
					|| update.Moisture != water.Moisture[index];
				if (changed)
				{
					updates.push_back(update);
				}
			}

			for (const Update& update : updates)
			{
				const int x = (int)(update.Index % (uint32_t)width);
				const int y = (int)(update.Index / (uint32_t)width);
				if (update.Tile != map.GetTiles()[update.Index])
				{
					report.Filled += update.Tile == settings.WaterTileId ? 1 : 0;
					report.Dried += update.Tile == settings.BedTileId ? 1 : 0;
					map.Set(x, y, update.Tile);
				}
				water.Head[update.Index] = update.Head;
				water.Moisture[update.Index] = update.Moisture;
				ActivateAround(map, water, update.Index);
			}
			return report;
		}

		// The head water would have here: the strongest neighbouring water's head less the loss for the channel's width
		// (water and riverbed neighbours: 2 or fewer lose Loss, 8 lose a quarter of it).
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
					if (tile == settings.WaterTileId || tile == settings.BedTileId)
					{
						channel++;
					}
					if (tile == settings.WaterTileId)
					{
						const float head = water.Head[(size_t)(y + oy) * (size_t)map.GetWidth() + (size_t)(x + ox)];
						strongest = head > strongest ? head : strongest;
					}
				}
			}
			if (strongest <= 0.0f)
			{
				return 0.0f;
			}
			return strongest - settings.Loss * 2.0f / (float)(channel > 2 ? channel : 2);
		}

		// 255 on fresh water; elsewhere the wettest neighbour's moisture less one step of the reach.
		static uint8_t TargetMoisture(const TileMap& map, const Hydrology& water, const std::vector<uint8_t>& land, int x, int y,
			const Settings& settings)
		{
			const size_t index = (size_t)y * (size_t)map.GetWidth() + (size_t)x;
			if (map.GetTiles()[index] == settings.WaterTileId && IsFresh(land, index))
			{
				return 255;
			}
			int wettest = 0;
			for (int oy = -1; oy <= 1; oy++)
			{
				for (int ox = -1; ox <= 1; ox++)
				{
					if ((ox != 0 || oy != 0) && map.InBounds(x + ox, y + oy))
					{
						const int value = water.Moisture[(size_t)(y + oy) * (size_t)map.GetWidth() + (size_t)(x + ox)];
						wettest = value > wettest ? value : wettest;
					}
				}
			}
			const int step = 255 / (settings.MoistureReach > 0 ? settings.MoistureReach : 1);
			return (uint8_t)(wettest - step > 0 ? wettest - step : 0);
		}

	private:
		static bool IsFresh(const std::vector<uint8_t>& land, size_t index)
		{
			return land.empty() || (index < land.size() && land[index] != 0);
		}

		static uint8_t MoistureAt(int steps, const Settings& settings)
		{
			const int step = 255 / (settings.MoistureReach > 0 ? settings.MoistureReach : 1);
			const int value = 255 - steps * step;
			return (uint8_t)(value > 0 ? value : 0);
		}

		static void ActivateAround(const TileMap& map, Hydrology& water, uint32_t index)
		{
			const int width = map.GetWidth();
			const int x = (int)(index % (uint32_t)width);
			const int y = (int)(index / (uint32_t)width);
			for (int oy = -1; oy <= 1; oy++)
			{
				for (int ox = -1; ox <= 1; ox++)
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
