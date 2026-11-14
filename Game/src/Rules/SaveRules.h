#pragma once

#include "State/HearthlandsSession.h"
#include "State/SaveBytes.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Hydro
{
	// What a save slot shows before it is loaded (the file's first section).
	struct SaveInfo
	{
		std::string Name;    // the world's name in the slot list: its first settlement's, else "Wild island"
		std::string SavedAt; // local time as year-month-day hours:minutes (the caller's clock)
		std::string Season;
		int Year = 1;
		int Day = 1;
		int Settlements = 0;
		int People = 0;
		int Talents = 0;
		int Width = 0;
		int Height = 0;
		int Speed = 1;       // the clock's speed to go on with (the menu pauses it while saving)
	};

	// Saving and loading a run (pure: the World Menu reads and writes the files). The whole session in tagged sections,
	// each with its own version, so a later game skips what it doesn't know and an older save loads with defaults.
	class SaveRules
	{
	public:
		static constexpr uint32_t Magic = SaveTag("HLSV");
		static constexpr uint32_t Version = 1;

		// The slot's line for a session now.
		static SaveInfo Describe(const HearthlandsSession& session, const std::string& savedAt, int speed)
		{
			SaveInfo info;
			info.Name = session.Settlements.List.empty() ? std::string("Wild island") : session.Settlements.List.front().Name;
			info.SavedAt = savedAt;
			info.Season = session.Season.Name;
			info.Year = session.Season.Year;
			info.Day = session.Season.Day;
			info.Settlements = (int)session.Settlements.List.size();
			for (const Settlement& settlement : session.Settlements.List)
			{
				info.People += (int)(settlement.Count + 0.5f);
			}
			info.Talents = (int)session.Talents.Owned.size();
			info.Width = session.Map.GetWidth();
			info.Height = session.Map.GetHeight();
			info.Speed = speed;
			return info;
		}

		static std::vector<uint8_t> Write(const HearthlandsSession& session, const SaveInfo& info)
		{
			SaveWriter out;
			out.Value(Magic);
			out.Value(Version);
			WriteInfo(out, info);
			WriteMap(out, session);
			WriteWater(out, session.Water);
			out.Begin(SaveTag("OBJS"), 1);
			out.Values(session.Objects.GetIds());
			out.End();
			out.Begin(SaveTag("SOIL"), 1);
			out.Values(session.Soil.Fertility);
			out.Values(session.Soil.Sand);
			out.End();
			WriteWildlife(out, session.Wildlife);
			WriteTalents(out, session.Talents);
			WriteSettlements(out, session.Settlements);
			out.Begin(SaveTag("RUN "), 1);
			out.Value(session.Clock.Seconds);
			out.Value(session.Clock.Ticks);
			out.Value(session.Rng.GetState());
			out.Value(session.Brush.TileId);
			out.Value(session.Brush.Radius);
			out.Value(session.CameraTile.x);
			out.Value(session.CameraTile.y);
			out.Value((int32_t)session.CameraZoom);
			out.End();
			out.Begin(SaveTag("END "), 1); // a file cut short (a crash while writing) is missing this
			out.End();
			return out.GetBytes();
		}

		// Only the slot's line (cheap: the first section).
		static bool ReadInfo(const std::vector<uint8_t>& bytes, SaveInfo& info)
		{
			SaveReader in(bytes.data(), bytes.size());
			if (ReadHeader(in) == false)
			{
				return false;
			}
			uint32_t tag = 0;
			uint32_t version = 0;
			SaveReader content;
			if (in.NextSection(tag, version, content) == false || tag != SaveTag("INFO"))
			{
				return false;
			}
			info = ReadInfoSection(content);
			return content.IsGood();
		}

		// The whole run into `session`: read into a fresh session first, so a damaged or newer file changes nothing.
		static bool Read(const std::vector<uint8_t>& bytes, HearthlandsSession& session, SaveInfo& info)
		{
			SaveReader in(bytes.data(), bytes.size());
			if (ReadHeader(in) == false)
			{
				return false;
			}
			HearthlandsSession loaded;
			int width = 0;
			int height = 0;
			std::vector<uint8_t> tiles;
			std::vector<uint8_t> levels;
			std::vector<uint8_t> objects;
			bool hasMap = false;
			bool complete = false;
			uint32_t tag = 0;
			uint32_t version = 0;
			SaveReader content;
			while (in.NextSection(tag, version, content))
			{
				if (tag == SaveTag("INFO"))
				{
					info = ReadInfoSection(content);
				}
				else if (tag == SaveTag("END "))
				{
					complete = true;
				}
				else if (tag == SaveTag("MAP "))
				{
					width = content.Value<int32_t>();
					height = content.Value<int32_t>();
					tiles = content.Values<uint8_t>();
					levels = content.Values<uint8_t>();
					loaded.LevelHeight = content.Value<float>(loaded.LevelHeight);
					loaded.UnlockedTiles = content.Values<uint8_t>();
					loaded.Land = content.Values<uint8_t>();
					loaded.Groundwater = content.Values<uint8_t>();
					loaded.ClueClarity = content.Value<float>(loaded.ClueClarity);
					loaded.ClueFrom = content.Value<float>(loaded.ClueFrom);
					loaded.DryCreeks = content.Values<uint32_t>();
					hasMap = true;
				}
				else if (tag == SaveTag("WATR"))
				{
					ReadWater(content, loaded.Water);
				}
				else if (tag == SaveTag("OBJS"))
				{
					objects = content.Values<uint8_t>();
				}
				else if (tag == SaveTag("SOIL"))
				{
					loaded.Soil.Fertility = content.Values<uint8_t>();
					loaded.Soil.Sand = content.Values<uint8_t>();
				}
				else if (tag == SaveTag("WILD"))
				{
					ReadWildlife(content, loaded.Wildlife);
				}
				else if (tag == SaveTag("TLNT"))
				{
					ReadTalents(content, loaded.Talents);
				}
				else if (tag == SaveTag("SETL"))
				{
					ReadSettlements(content, version, loaded.Settlements);
				}
				else if (tag == SaveTag("RUN "))
				{
					loaded.Clock.Seconds = content.Value<double>();
					loaded.Clock.Ticks = content.Value<uint64_t>();
					loaded.Rng.SetState(content.Value<uint64_t>(1));
					loaded.Brush.TileId = content.Value<uint8_t>(loaded.Brush.TileId);
					loaded.Brush.Radius = content.Value<float>(loaded.Brush.Radius);
					loaded.CameraTile.x = content.Value<float>();
					loaded.CameraTile.y = content.Value<float>();
					loaded.CameraZoom = content.Value<int32_t>(-1);
				}
				if (content.IsGood() == false)
				{
					return false;
				}
			}
			const size_t count = (size_t)width * (size_t)height;
			const bool fits = hasMap && complete && in.IsGood() && width > 0 && height > 0 && tiles.size() == count && Fits(levels, count)
				&& Fits(loaded.Land, count) && Fits(loaded.Groundwater, count) && Fits(objects, count)
				&& Fits(loaded.Soil.Fertility, count) && Fits(loaded.Soil.Sand, count) && WaterFits(loaded.Water, count);
			if (fits == false)
			{
				return false;
			}
			Apply(session, loaded, width, height, tiles, levels, objects);
			return true;
		}

	private:
		// Per-tile data: as many as the map's tiles, or none (worked out again when the world starts).
		static bool Fits(const std::vector<uint8_t>& values, size_t count)
		{
			return values.empty() || values.size() == count;
		}

		static bool WaterFits(const Hydrology& water, size_t count)
		{
			const bool head = water.Head.empty() || water.Head.size() == count;
			const bool bytes = Fits(water.Source, count) && Fits(water.Carried, count) && Fits(water.Moisture, count)
				&& Fits(water.Rain, count) && Fits(water.Groundwater, count) && Fits(water.Melt, count) && Fits(water.Sealed, count)
				&& Fits(water.Native, count) && Fits(water.Habitat, count) && Fits(water.Queued, count);
			bool active = true;
			for (uint32_t index : water.Active)
			{
				active = active && index < water.Queued.size();
			}
			return head && bytes && active;
		}

		static bool ReadHeader(SaveReader& in)
		{
			const uint32_t magic = in.Value<uint32_t>();
			const uint32_t version = in.Value<uint32_t>();
			return in.IsGood() && magic == Magic && version >= 1 && version <= Version;
		}

		static void WriteInfo(SaveWriter& out, const SaveInfo& info)
		{
			out.Begin(SaveTag("INFO"), 1);
			out.Text(info.Name);
			out.Text(info.SavedAt);
			out.Text(info.Season);
			for (int value : { info.Year, info.Day, info.Settlements, info.People, info.Talents, info.Width, info.Height, info.Speed })
			{
				out.Value((int32_t)value);
			}
			out.End();
		}

		static SaveInfo ReadInfoSection(SaveReader& in)
		{
			SaveInfo info;
			info.Name = in.Text();
			info.SavedAt = in.Text();
			info.Season = in.Text();
			info.Year = in.Value<int32_t>(1);
			info.Day = in.Value<int32_t>(1);
			info.Settlements = in.Value<int32_t>();
			info.People = in.Value<int32_t>();
			info.Talents = in.Value<int32_t>();
			info.Width = in.Value<int32_t>();
			info.Height = in.Value<int32_t>();
			info.Speed = in.Value<int32_t>(1);
			return info;
		}

		static void WriteMap(SaveWriter& out, const HearthlandsSession& session)
		{
			out.Begin(SaveTag("MAP "), 1);
			out.Value((int32_t)session.Map.GetWidth());
			out.Value((int32_t)session.Map.GetHeight());
			out.Values(session.Map.GetTiles());
			out.Values(session.Heights.GetLevels());
			out.Value(session.LevelHeight);
			out.Values(session.UnlockedTiles);
			out.Values(session.Land);
			out.Values(session.Groundwater);
			out.Value(session.ClueClarity);
			out.Value(session.ClueFrom);
			out.Values(session.DryCreeks);
			out.End();
		}

		static void WriteWater(SaveWriter& out, const Hydrology& water)
		{
			out.Begin(SaveTag("WATR"), 1);
			out.Value(water.WaterTileId);
			out.Value(water.BedTileId);
			out.Values(water.Head);
			for (const std::vector<uint8_t>* layer : { &water.Source, &water.Carried, &water.Moisture, &water.Rain, &water.Groundwater,
				&water.Melt, &water.Sealed, &water.Native, &water.Habitat, &water.Queued })
			{
				out.Values(*layer);
			}
			out.Values(water.Active);
			out.End();
		}

		static void ReadWater(SaveReader& in, Hydrology& water)
		{
			water.WaterTileId = in.Value<uint8_t>(water.WaterTileId);
			water.BedTileId = in.Value<uint8_t>(water.BedTileId);
			water.Head = in.Values<float>();
			for (std::vector<uint8_t>* layer : { &water.Source, &water.Carried, &water.Moisture, &water.Rain, &water.Groundwater,
				&water.Melt, &water.Sealed, &water.Native, &water.Habitat, &water.Queued })
			{
				*layer = in.Values<uint8_t>();
			}
			water.Active = in.Values<uint32_t>();
		}

		static void WriteWildlife(SaveWriter& out, const WildlifeState& wildlife)
		{
			out.Begin(SaveTag("WILD"), 1);
			out.Value((int32_t)wildlife.RegionSize);
			out.Value((int32_t)wildlife.RegionsX);
			out.Value((int32_t)wildlife.RegionsY);
			out.Value((uint32_t)wildlife.Population.size());
			for (size_t species = 0; species < wildlife.Population.size(); species++)
			{
				out.Values(wildlife.Population[species]);
				out.Values(species < wildlife.Capacity.size() ? wildlife.Capacity[species] : std::vector<float>());
			}
			out.Values(wildlife.Discovered);
			out.Values(wildlife.Found);
			out.Value(wildlife.NextRegion);
			out.End();
		}

		static void ReadWildlife(SaveReader& in, WildlifeState& wildlife)
		{
			wildlife.RegionSize = in.Value<int32_t>(wildlife.RegionSize);
			wildlife.RegionsX = in.Value<int32_t>();
			wildlife.RegionsY = in.Value<int32_t>();
			const uint32_t species = in.Value<uint32_t>();
			for (uint32_t s = 0; s < species && in.IsGood(); s++)
			{
				wildlife.Population.push_back(in.Values<float>());
				wildlife.Capacity.push_back(in.Values<float>());
			}
			wildlife.Discovered = in.Values<uint8_t>();
			wildlife.Found = in.Values<int>();
			wildlife.NextRegion = in.Value<uint32_t>();
		}

		static void WriteTalents(SaveWriter& out, const TalentState& talents)
		{
			out.Begin(SaveTag("TLNT"), 1);
			out.Value((int32_t)talents.Points);
			out.Value((int32_t)talents.Earned);
			out.Values(talents.Owned);
			out.Values(talents.Reached);
			out.Value((int32_t)talents.SpringsFound);
			out.End();
		}

		// What the owned talents add up to is not saved: the game recounts it from the talents table (TalentRules::Recount).
		static void ReadTalents(SaveReader& in, TalentState& talents)
		{
			talents.Points = in.Value<int32_t>();
			talents.Earned = in.Value<int32_t>();
			talents.Owned = in.Values<int>();
			talents.Reached = in.Values<int>();
			talents.SpringsFound = in.Value<int32_t>();
		}

		static void WriteSettlements(SaveWriter& out, const SettlementState& state)
		{
			out.Begin(SaveTag("SETL"), 1);
			out.Value((int32_t)state.NextId);
			out.Values(state.SearchWait);
			out.Values(state.Founded);
			out.Value((uint32_t)state.Ruins.size());
			for (const Ruin& ruin : state.Ruins)
			{
				out.Value((int32_t)ruin.X);
				out.Value((int32_t)ruin.Y);
				out.Value(ruin.Days);
			}
			out.Value((uint32_t)state.List.size());
			for (const Settlement& settlement : state.List)
			{
				WriteSettlement(out, settlement);
			}
			out.End();
		}

		static void WriteSettlement(SaveWriter& out, const Settlement& s)
		{
			out.Text(s.Name);
			for (int value : { s.Id, s.NameIndex, s.X, s.Y, s.People, s.Level, s.Fields, s.FieldsGrowing, s.FieldsRipe, s.Harvests,
				s.Shelters, s.Room, s.OldShelters, s.Needed, s.NeededTiles, s.NextVillager, s.VillagerNames, s.FeudWith, s.EvolveTo,
				s.EvolveHave, s.EvolveWant })
			{
				out.Value((int32_t)value);
			}
			for (float value : { s.Count, s.Food, s.Wood, s.Felling, s.Building, s.Planting, s.FarmWork, s.Water, s.FoodNearby,
				s.WoodNearby, s.Tension, s.Days })
			{
				out.Value(value);
			}
			out.Value((uint8_t)s.Focus);
			out.Value((uint8_t)(s.EvolveAllowed ? 1 : 0));
			out.Value((uint32_t)s.Villagers.size());
			for (const Villager& villager : s.Villagers)
			{
				out.Value((int32_t)villager.Id);
				out.Text(villager.Name);
				out.Value((uint8_t)villager.Job);
				out.Value(villager.HurtDays);
				out.Value(villager.Days);
			}
		}

		static void ReadSettlements(SaveReader& in, uint32_t version, SettlementState& state)
		{
			state.NextId = in.Value<int32_t>(1);
			state.SearchWait = in.Values<float>();
			state.Founded = in.Values<int>();
			const uint32_t ruins = in.Value<uint32_t>();
			for (uint32_t r = 0; r < ruins && in.IsGood(); r++)
			{
				Ruin ruin;
				ruin.X = in.Value<int32_t>();
				ruin.Y = in.Value<int32_t>();
				ruin.Days = in.Value<float>();
				state.Ruins.push_back(ruin);
			}
			const uint32_t settlements = in.Value<uint32_t>();
			for (uint32_t i = 0; i < settlements && in.IsGood(); i++)
			{
				state.List.push_back(ReadSettlement(in, version));
			}
		}

		static Settlement ReadSettlement(SaveReader& in, [[maybe_unused]] uint32_t version)
		{
			Settlement s;
			s.Name = in.Text();
			for (int* value : { &s.Id, &s.NameIndex, &s.X, &s.Y, &s.People, &s.Level, &s.Fields, &s.FieldsGrowing, &s.FieldsRipe,
				&s.Harvests, &s.Shelters, &s.Room, &s.OldShelters, &s.Needed, &s.NeededTiles, &s.NextVillager, &s.VillagerNames,
				&s.FeudWith, &s.EvolveTo, &s.EvolveHave, &s.EvolveWant })
			{
				*value = in.Value<int32_t>();
			}
			for (float* value : { &s.Count, &s.Food, &s.Wood, &s.Felling, &s.Building, &s.Planting, &s.FarmWork, &s.Water,
				&s.FoodNearby, &s.WoodNearby, &s.Tension, &s.Days })
			{
				*value = in.Value<float>();
			}
			const uint8_t focus = in.Value<uint8_t>();
			s.Focus = focus <= (uint8_t)SettlementFocus::Building ? (SettlementFocus)focus : SettlementFocus::None;
			s.EvolveAllowed = in.Value<uint8_t>() != 0;
			const uint32_t villagers = in.Value<uint32_t>();
			for (uint32_t v = 0; v < villagers && in.IsGood(); v++)
			{
				Villager villager;
				villager.Id = in.Value<int32_t>();
				villager.Name = in.Text();
				const uint8_t job = in.Value<uint8_t>();
				villager.Job = job <= (uint8_t)VillagerJob::Farmer ? (VillagerJob)job : VillagerJob::Resting;
				villager.HurtDays = in.Value<float>();
				villager.Days = in.Value<float>();
				s.Villagers.push_back(villager);
			}
			return s;
		}

		// Moves the loaded run into the session. Revisions move past the old ones so every view sees a change; what is
		// only for this moment (the selection, news, the farmland reach) starts empty.
		static void Apply(HearthlandsSession& session, HearthlandsSession& loaded, int width, int height,
			const std::vector<uint8_t>& tiles, const std::vector<uint8_t>& levels, const std::vector<uint8_t>& objects)
		{
			session.Map.Replace(TileMap::FromTiles(width, height, tiles));
			session.Heights.Replace(levels.empty() ? HeightMap(width, height) : HeightMap::FromLevels(width, height, levels));
			session.LevelHeight = loaded.LevelHeight;
			session.UnlockedTiles = std::move(loaded.UnlockedTiles);
			session.Land = loaded.Land.empty() ? std::vector<uint8_t>((size_t)width * (size_t)height, 1) : std::move(loaded.Land);
			session.Groundwater = std::move(loaded.Groundwater);
			session.ClueClarity = loaded.ClueClarity;
			session.ClueFrom = loaded.ClueFrom;
			session.Talents = loaded.Talents;
			session.MilestoneProgress.clear();
			session.Brush = loaded.Brush;
			session.Clock.Seconds = loaded.Clock.Seconds;
			session.Clock.Ticks = loaded.Clock.Ticks;
			session.Water = std::move(loaded.Water);
			session.Objects.Reset((size_t)width * (size_t)height);
			for (size_t i = 0; i < objects.size(); i++)
			{
				session.Objects.Set(i, objects[i]);
			}
			const uint64_t soil = session.Soil.Revision;
			session.Soil = std::move(loaded.Soil);
			session.Soil.Revision = soil + 1;
			const uint64_t wildlife = session.Wildlife.Revision;
			session.Wildlife = std::move(loaded.Wildlife);
			session.Wildlife.Revision = wildlife + 1;
			const uint64_t settlements = session.Settlements.Revision;
			session.Settlements = std::move(loaded.Settlements);
			session.Settlements.Revision = settlements + 1;
			session.DryCreeks = std::move(loaded.DryCreeks);
			session.Rng = loaded.Rng;
			session.CameraTile = loaded.CameraTile;
			session.CameraZoom = loaded.CameraZoom;
			session.SelectedSettlement = 0;
			session.RenameRequested = 0;
			session.FieldTileId = -1;
			session.FarmReach.clear();
			session.Notices.clear();
			session.Painting = false;
			session.MountainsChanged = true; // the meltwater is worked out again for the loaded heights
		}
	};
}
