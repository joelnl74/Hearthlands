// Tools/Tests/SaveRulesTest.cpp
// Saving and loading a run (SaveRules): every part of the session comes back as it was, the slot's line reads alone, and a
// damaged, foreign or newer file is refused without touching the session. Run from TestGameRules.

#include "hypch.h"

#include "Rules/SaveRules.h"

#include <cstdio>
#include <vector>

namespace Hydro
{
	static int s_SaveFailures = 0;

	static void SaveCheck(bool condition, const char* what, int line, int& checks)
	{
		checks++;
		if (condition == false)
		{
			s_SaveFailures++;
			std::printf("FAIL  SaveRulesTest.cpp line %d: %s\n", line, what);
		}
	}

#define SAVE_CHECK(condition) SaveCheck((condition), #condition, __LINE__, checks)

	// A small run with something in every part of the session.
	static void MakeSavedRun(HearthlandsSession& session)
	{
		const int width = 8;
		const int height = 6;
		const size_t count = (size_t)width * (size_t)height;
		std::vector<uint8_t> tiles(count, 2);
		tiles[9] = 3;
		tiles[10] = 7;
		session.Map.Replace(TileMap::FromTiles(width, height, tiles));
		std::vector<uint8_t> levels(count, 0);
		levels[20] = 4;
		session.Heights.Replace(HeightMap::FromLevels(width, height, levels));
		session.UnlockedTiles = { 1, 2, 5, 7 };
		session.Land.assign(count, 1);
		session.Land[0] = 0;
		session.Groundwater.assign(count, 90);
		session.ClueClarity = 0.5f;
		session.DryCreeks = { 12, 13 };
		session.Water.Head.assign(count, 0.0f);
		session.Water.Head[9] = 0.75f;
		session.Water.Source.assign(count, 0);
		session.Water.Carried.assign(count, 10);
		session.Water.Moisture.assign(count, 40);
		session.Water.Native.assign(count, 0);
		session.Water.Queued.assign(count, 0);
		session.Water.Activate(9);
		session.Objects.Reset(count);
		session.Objects.Set(11, 72);
		session.Objects.Set(30, 5);
		session.Soil.Fertility.assign(count, 120);
		session.Soil.Sand.assign(count, 30);
		session.Wildlife.RegionsX = 1;
		session.Wildlife.RegionsY = 1;
		session.Wildlife.Population = { { 4.5f }, { 0.0f } };
		session.Wildlife.Capacity = { { 9.0f }, { 2.0f } };
		session.Wildlife.Discovered = { 1, 0 };
		session.Wildlife.Found = { 0 };
		session.Talents.Points = 3;
		session.Talents.Earned = 7;
		session.Talents.Owned = { 14, 45 };
		session.Talents.Reached = { 1, 39 };
		session.Talents.SpringsFound = 2;
		session.Clock.Seconds = 1234.5;
		session.Clock.Ticks = 9876;
		session.Clock.Speed = 3;
		session.Rng.SetState(424242);
		session.Brush.TileId = 7;
		session.Brush.Radius = 3.0f;
		session.CameraTile = glm::vec2(4.5f, 2.0f);
		session.CameraZoom = 2;
		session.Season.Name = "Summer";
		session.Season.Year = 2;
		session.Season.Day = 3;

		SettlementState& state = session.Settlements;
		state.NextId = 3;
		state.SearchWait = { 0.0f, 2.5f };
		state.Founded = { 2, 0 };
		Ruin ruin;
		ruin.X = 1;
		ruin.Y = 2;
		ruin.Days = 6.0f;
		state.Ruins.push_back(ruin);
		Settlement home;
		home.Id = 2;
		home.Name = "Ashford";
		home.X = 3;
		home.Y = 4;
		home.Level = 1;
		home.Count = 7.4f;
		home.Food = 31.5f;
		home.Wood = 4.0f;
		home.Fields = 6;
		home.Harvests = 2;
		home.Focus = SettlementFocus::Food;
		home.Tension = 0.25f;
		home.EvolveAllowed = true;
		home.Buildings = { 1, 3 };
		home.Planned = 2;
		home.FieldsMost = 36;
		Villager aldric;
		aldric.Id = 1;
		aldric.Name = "Aldric";
		aldric.Job = VillagerJob::Farmer;
		aldric.HurtDays = 0.5f;
		home.Villagers.push_back(aldric);
		state.List.push_back(home);
	}

	int TestSaveRules(int& checks)
	{
		HearthlandsSession run;
		MakeSavedRun(run);
		const SaveInfo info = SaveRules::Describe(run, "2026-11-12 15:40", 3);
		SAVE_CHECK(info.Name == "Ashford");
		SAVE_CHECK(info.Settlements == 1 && info.People == 7 && info.Talents == 2 && info.Year == 2 && info.Season == "Summer");
		const std::vector<uint8_t> bytes = SaveRules::Write(run, info);

		// The slot's line alone, from the start of the file.
		SaveInfo line;
		SAVE_CHECK(SaveRules::ReadInfo(std::vector<uint8_t>(bytes.begin(), bytes.begin() + 200), line));
		SAVE_CHECK(line.Name == "Ashford" && line.SavedAt == "2026-11-12 15:40" && line.Speed == 3 && line.Width == 8);

		// Everything comes back, into a session that held another run.
		HearthlandsSession loaded;
		loaded.Map.Replace(TileMap(4, 4, 1));
		loaded.SelectedSettlement = 5;
		loaded.Notices.push_back("old news");
		const uint64_t mapRevision = loaded.Map.GetRevision();
		const uint64_t soilRevision = loaded.Soil.Revision;
		SaveInfo read;
		SAVE_CHECK(SaveRules::Read(bytes, loaded, read));
		SAVE_CHECK(read.Name == "Ashford");
		SAVE_CHECK(loaded.Map.GetWidth() == 8 && loaded.Map.GetHeight() == 6 && loaded.Map.GetTiles() == run.Map.GetTiles());
		SAVE_CHECK(loaded.Map.GetCount(3) == 1 && loaded.Map.GetCount(2) == 46);
		SAVE_CHECK(loaded.Map.GetRevision() > mapRevision);
		SAVE_CHECK(loaded.Heights.Get(4, 2) == 4 && loaded.Heights.GetWidth() == 8);
		SAVE_CHECK(loaded.UnlockedTiles == run.UnlockedTiles && loaded.Land == run.Land && loaded.Groundwater == run.Groundwater);
		SAVE_CHECK(loaded.ClueClarity == 0.5f && loaded.DryCreeks == run.DryCreeks);
		SAVE_CHECK(loaded.Water.Head == run.Water.Head && loaded.Water.Moisture == run.Water.Moisture);
		SAVE_CHECK(loaded.Water.Active.size() == 1 && loaded.Water.Active[0] == 9 && loaded.Water.Queued[9] == 1);
		SAVE_CHECK(loaded.Water.Rain.empty() && loaded.Water.Melt.empty()); // none saved, none loaded
		SAVE_CHECK(loaded.Objects.Get(11) == 72 && loaded.Objects.Get(30) == 5 && loaded.Objects.GetCount() == 2);
		SAVE_CHECK(loaded.Soil.Fertility == run.Soil.Fertility && loaded.Soil.Revision > soilRevision);
		SAVE_CHECK(loaded.Wildlife.Population == run.Wildlife.Population && loaded.Wildlife.Capacity == run.Wildlife.Capacity);
		SAVE_CHECK(loaded.Wildlife.Discovered == run.Wildlife.Discovered && loaded.Wildlife.Found == run.Wildlife.Found);
		SAVE_CHECK(loaded.Talents.Points == 3 && loaded.Talents.Earned == 7 && loaded.Talents.Owned == run.Talents.Owned);
		SAVE_CHECK(loaded.Talents.Reached == run.Talents.Reached && loaded.Talents.SpringsFound == 2);
		SAVE_CHECK(loaded.Clock.Seconds == 1234.5 && loaded.Clock.Ticks == 9876);
		SAVE_CHECK(loaded.Brush.TileId == 7 && loaded.Brush.Radius == 3.0f && loaded.CameraZoom == 2);
		SAVE_CHECK(loaded.CameraTile == glm::vec2(4.5f, 2.0f));
		SAVE_CHECK(loaded.Rng.Next() == run.Rng.Next()); // the same throws go on
		const SettlementState& state = loaded.Settlements;
		SAVE_CHECK(state.NextId == 3 && state.SearchWait == run.Settlements.SearchWait && state.Founded == run.Settlements.Founded);
		SAVE_CHECK(state.Ruins.size() == 1 && state.Ruins[0].X == 1 && state.Ruins[0].Days == 6.0f);
		SAVE_CHECK(state.List.size() == 1);
		if (state.List.size() == 1)
		{
			const Settlement& home = state.List[0];
			SAVE_CHECK(home.Id == 2 && home.Name == "Ashford" && home.X == 3 && home.Y == 4 && home.Level == 1);
			SAVE_CHECK(home.Count == 7.4f && home.Food == 31.5f && home.Fields == 6 && home.Harvests == 2);
			SAVE_CHECK(home.Focus == SettlementFocus::Food && home.Tension == 0.25f && home.EvolveAllowed);
			SAVE_CHECK(home.Villagers.size() == 1 && home.Villagers[0].Name == "Aldric");
			SAVE_CHECK(home.Villagers.size() == 1 && home.Villagers[0].Job == VillagerJob::Farmer);
			SAVE_CHECK(home.Buildings == std::vector<int>({ 1, 3 }) && home.Planned == 2 && home.FieldsMost == 36);
		}
		SAVE_CHECK(loaded.SelectedSettlement == 0 && loaded.Notices.empty() && loaded.MountainsChanged);

		// Refused, and the session left as it was: cut short, not a save, a newer game, a layer that doesn't fit the map.
		HearthlandsSession kept;
		kept.Map.Replace(TileMap(4, 4, 1));
		SaveInfo ignored;
		SAVE_CHECK(SaveRules::Read(std::vector<uint8_t>(bytes.begin(), bytes.begin() + (long)(bytes.size() / 2)), kept, ignored) == false);
		std::vector<uint8_t> foreign = bytes;
		foreign[0] = 'X';
		SAVE_CHECK(SaveRules::Read(foreign, kept, ignored) == false);
		SAVE_CHECK(SaveRules::ReadInfo(foreign, ignored) == false);
		std::vector<uint8_t> newer = bytes;
		newer[4] = (uint8_t)(SaveRules::Version + 1);
		SAVE_CHECK(SaveRules::Read(newer, kept, ignored) == false);
		HearthlandsSession bad;
		MakeSavedRun(bad);
		bad.Soil.Sand.assign(5, 0);
		SAVE_CHECK(SaveRules::Read(SaveRules::Write(bad, info), kept, ignored) == false);
		SAVE_CHECK(SaveRules::Read(std::vector<uint8_t>(), kept, ignored) == false);
		SAVE_CHECK(kept.Map.GetWidth() == 4 && kept.Settlements.List.empty());

		// A section a later game added is skipped.
		SaveWriter later;
		later.Value(SaveRules::Magic);
		later.Value(SaveRules::Version);
		const std::vector<uint8_t>& body = bytes;
		later.Begin(SaveTag("ZZZZ"), 7);
		later.Value((uint32_t)12345);
		later.End();
		std::vector<uint8_t> extended = later.GetBytes();
		extended.insert(extended.end(), body.begin() + 8, body.end());
		HearthlandsSession skipped;
		SAVE_CHECK(SaveRules::Read(extended, skipped, ignored) && skipped.Map.GetWidth() == 8);

		// The byte reader never reads past its end.
		const uint8_t few[3] = { 1, 2, 3 };
		SaveReader reader(few, 3);
		SAVE_CHECK(reader.Value<uint32_t>(77) == 77 && reader.IsGood() == false);
		return s_SaveFailures;
	}
}
