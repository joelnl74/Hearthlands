#pragma once

#include "Hydro.h"
#include "Hydro/Renderer/Font.h"
#include "Hydro/Scene/UI/UI.h"
#include "Data/AnimalDefinition.h"
#include "Data/MilestoneDefinition.h"
#include "Data/TalentDefinition.h"
#include "Data/TerrainTypeDefinition.h"
#include "Rules/SaveRules.h"
#include "Rules/TalentRules.h"
#include "State/HearthlandsSession.h"
#include "Views/WorldMenuView.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace Hydro
{
	// The World Menu on its own entity (a canvas over the HUD): Tab opens and closes it, T opens Technology, Esc goes back or
	// closes it. Open, it takes the keyboard and stops the clock (a setting); buys talents, sets options, presents the session.
	HY_SCRIPT()
	class WorldMenuScript : public ScriptableEntity
	{
	public:
		static constexpr uint64_t DefaultFont = 7311820458392716001ull;         // Assets/Fonts/OpenSans.fnt
		static constexpr uint64_t DefaultHeadingFont = 7311820458392716021ull;  // Assets/Fonts/MenuHeading.fnt
		static constexpr uint64_t DefaultTitleFont = 7311820458392716011ull;    // Assets/Fonts/MenuTitle.fnt
		static constexpr uint64_t DefaultArt = 4973082495598781234ull;          // Assets/Textures/UI/MenuArt.png
		static constexpr uint64_t DefaultTerrainTypes = 4392505292887402202ull; // Assets/Data/TerrainTypes.hytable
		static constexpr uint64_t DefaultTalents = 3307419921861244467ull;      // Assets/Data/Talents.hytable
		static constexpr uint64_t DefaultMilestones = 5179923417704519381ull;   // Assets/Data/Milestones.hytable
		static constexpr uint64_t DefaultAnimals = 8152222171692113247ull;      // Assets/Data/Animals.hytable

		HY_PROPERTY(Asset(Font), Tooltip("Running text"))
		AssetHandle UIFont = DefaultFont;
		HY_PROPERTY(Asset(Font), Tooltip("Headings: tiles, panels, sections"))
		AssetHandle HeadingFont = DefaultHeadingFont;
		HY_PROPERTY(Asset(Font), Tooltip("Page titles"))
		AssetHandle TitleFont = DefaultTitleFont;
		HY_PROPERTY(Asset(Texture2D), Tooltip("The menu's sprites by name (scripts/art/make_ui_art.py); none draws flat colours"))
		AssetHandle Art = DefaultArt;
		HY_PROPERTY(Asset(DataTable, TerrainTypeDefinition), Tooltip("The brushes' names (talents that unlock one, milestones)"))
		AssetHandle TerrainTypes = DefaultTerrainTypes;
		HY_PROPERTY(Asset(DataTable, TalentDefinition), Tooltip("What talent points buy"))
		AssetHandle Talents = DefaultTalents;
		HY_PROPERTY(Asset(DataTable, MilestoneDefinition), Tooltip("What gives talent points (the same table as SimulationScript's)"))
		AssetHandle Milestones = DefaultMilestones;
		HY_PROPERTY(Asset(DataTable, AnimalDefinition), Tooltip("The species (the same table as SimulationScript's)"))
		AssetHandle Animals = DefaultAnimals;
		HY_PROPERTY(Tooltip("Stop the clock while the menu is open (Settings > Gameplay)"))
		bool PauseInMenu = true;
		HY_PROPERTY(Tooltip("The world's scene (its file name): loading a save starts it again with the loaded run"))
		std::string WorldScene = "World";

	protected:
		void OnCreate() override
		{
			MenuFonts fonts;
			fonts.Body = UIFont;
			fonts.Heading = HeadingFont;
			fonts.Title = TitleFont;
			for (AssetHandle font : { UIFont, HeadingFont, TitleFont })
			{
				if (font != 0)
				{
					AssetManager::GetAsset<Font>(font); // the renderer never loads
				}
			}
			for (const auto& [id, type] : Data::GetTable<TerrainTypeDefinition>(TerrainTypes).Rows())
			{
				m_Types.push_back(type);
			}
			for (const auto& [id, talent] : Data::GetTable<TalentDefinition>(Talents).Rows())
			{
				m_Talents.push_back(talent);
			}
			for (const auto& [id, milestone] : Data::GetTable<MilestoneDefinition>(Milestones).Rows())
			{
				m_Milestones.push_back(milestone);
			}
			for (const auto& [id, animal] : Data::GetTable<AnimalDefinition>(Animals).Rows())
			{
				m_Animals.push_back(animal);
			}
			std::vector<std::string> effects;
			for (const TalentDefinition& talent : m_Talents)
			{
				effects.push_back(TechnologyPageView::EffectLine(talent, TileName((int)talent.Amount, true)));
			}
			m_View.Bind(GetEntity(), fonts, Art, m_Talents, effects, MilestoneLines(HearthlandsSession::Get()));
			m_View.GetTechnology().SetMilestones(m_Milestones);
		}

		void OnUpdate(float deltaTime) override
		{
			HearthlandsSession& session = HearthlandsSession::Get();
			if (m_Reloading)
			{
				// Still here a frame after asking for the scene again: it didn't load. Go on with the views as they are.
				HY_WARN("WorldMenuScript: the scene '{}' didn't load again after loading a save", WorldScene);
				session.Loading = false;
				m_Reloading = false;
			}
			if (session.Loading)
			{
				return;
			}
			ReadKeys(session);
			ReadSaveKeys(session);
			m_ArmedLeft -= deltaTime;
			if (m_Armed.empty() == false && m_ArmedLeft <= 0.0f)
			{
				SetArmed(std::string());
			}
			session.PointerOverMenu = m_View.ContainsPointer(*GetEntity().GetScene());
			m_View.PresentButton(session.Talents.Points);
			if (m_View.IsOpen() == false)
			{
				return;
			}
			if (PauseInMenu && session.Clock.Speed != 0)
			{
				m_SpeedBeforeMenu = session.Clock.Speed; // set by something else while open: kept for closing
				session.Clock.Speed = 0;
			}
			switch (m_View.GetPage())
			{
			case MenuPage::Home: m_View.PresentHome(HomeProgress(session)); break;
			case MenuPage::Technology: m_View.GetTechnology().Present(session.Talents, Verdicts(session)); break;
			case MenuPage::Milestones: m_View.GetMilestones().Present(MilestoneLines(session), session.Talents.Points); break;
			case MenuPage::Settings: m_View.GetSettings().Present(session.ShowMoisture, session.ShowSoil, PauseInMenu); break;
			case MenuPage::Saves: m_View.GetSaves().Present(m_Slots); break;
			default: break;
			}
		}

		bool OnUIEvent(const UIEvent& event) override
		{
			if (event.Type == UIEventType::Cancel)
			{
				return true; // Esc is read in OnUpdate (it also opens the menu)
			}
			if (event.Type != UIEventType::Submit)
			{
				return false;
			}
			HearthlandsSession& session = HearthlandsSession::Get();
			const std::string& name = event.Name;
			if (name == "Menu")
			{
				Open(session, MenuPage::Home);
			}
			else if (name == "Close")
			{
				Close(session);
			}
			else if (name == "Home")
			{
				Open(session, MenuPage::Home);
			}
			else if (name.rfind("Open ", 0) == 0)
			{
				Open(session, PageNamed(name.substr(5)));
			}
			else if (name.rfind("Branch ", 0) == 0)
			{
				m_View.GetTechnology().SetBranch(std::stoi(name.substr(7)));
			}
			else if (name.rfind("Talent ", 0) == 0)
			{
				m_View.GetTechnology().Select(std::stoi(name.substr(7)));
			}
			else if (name == "Learn")
			{
				Learn(session, m_View.GetTechnology().GetSelected());
			}
			else if (name.rfind("Milestone ", 0) == 0)
			{
				m_View.GetMilestones().Select((size_t)std::stoi(name.substr(10)));
			}
			else if (name.rfind("Track ", 0) == 0)
			{
				m_View.GetMilestones().SetTrack(std::stoi(name.substr(6)));
			}
			else if (name == "Page Prev" || name == "Page Next")
			{
				m_View.GetMilestones().TurnPage(name == "Page Next" ? 1 : -1);
			}
			else if (name.rfind("Section ", 0) == 0)
			{
				m_View.GetSettings().SetSection(std::stoi(name.substr(8)));
			}
			else if (name == "Toggle Moisture")
			{
				session.ShowMoisture = session.ShowMoisture == false;
				session.ShowSoil = false; // one overlay at a time, like M and F
			}
			else if (name == "Toggle Soil")
			{
				session.ShowSoil = session.ShowSoil == false;
				session.ShowMoisture = false;
			}
			else if (name == "Toggle Pause")
			{
				SetPauseInMenu(session, PauseInMenu == false);
			}
			else if (name.rfind("Save Slot ", 0) == 0 || name.rfind("Load Slot ", 0) == 0)
			{
				SlotPressed(session, name);
			}
			if (m_View.IsOpen() == false)
			{
				UI::ClearFocus(*GetEntity().GetScene()); // so Space and Enter stay with the game
			}
			return true;
		}

	private:
		static MenuPage PageNamed(const std::string& name)
		{
			const char* names[7] = { "Home", "Technology", "Milestones", "Settings", "Codex", "Map", "Saves" };
			for (int p = 0; p < 7; p++)
			{
				if (name == names[p])
				{
					return (MenuPage)p;
				}
			}
			return MenuPage::Home;
		}

		void Open(HearthlandsSession& session, MenuPage page)
		{
			if (m_View.IsOpen() == false)
			{
				m_SpeedBeforeMenu = session.Clock.Speed;
				if (PauseInMenu)
				{
					session.Clock.Speed = 0;
				}
			}
			if (page == MenuPage::Technology && m_View.GetPage() != MenuPage::Technology)
			{
				m_View.GetTechnology().SelectBuyable(Verdicts(session));
			}
			if (page == MenuPage::Saves)
			{
				RefreshSlots();
				SetArmed(std::string());
				m_View.GetSaves().SetNotice(std::string("Saves are kept in ") + SaveFolder().string(), false);
			}
			m_View.Show(page);
			UI::Focus(m_View.GetDefaultFocus());
		}

		void Close(HearthlandsSession& session)
		{
			if (m_View.IsOpen() == false)
			{
				return;
			}
			if (PauseInMenu && session.Clock.Speed == 0)
			{
				session.Clock.Speed = m_SpeedBeforeMenu;
			}
			m_View.Hide();
			UI::ClearFocus(*GetEntity().GetScene());
		}

		void SetPauseInMenu(HearthlandsSession& session, bool pause)
		{
			PauseInMenu = pause;
			if (pause)
			{
				m_SpeedBeforeMenu = session.Clock.Speed;
				session.Clock.Speed = 0;
			}
			else if (session.Clock.Speed == 0)
			{
				session.Clock.Speed = m_SpeedBeforeMenu;
			}
		}

		// Tab opens and closes, T opens Technology (or closes it), Esc goes back to the tiles, then closes; Esc also opens.
		void ReadKeys(HearthlandsSession& session)
		{
			const bool tab = Pressed(Key::Tab, m_TabHeld);
			const bool talents = Pressed(Key::T, m_TalentsHeld);
			const bool escape = Pressed(Key::Escape, m_EscapeHeld);
			const bool open = m_View.IsOpen();
			if (open == false && (tab || escape))
			{
				Open(session, MenuPage::Home);
			}
			else if (talents && (open == false || m_View.GetPage() != MenuPage::Technology))
			{
				Open(session, MenuPage::Technology);
			}
			else if (open && (tab || talents))
			{
				Close(session);
			}
			else if (open && escape)
			{
				if (m_View.GetPage() == MenuPage::Home)
				{
					Close(session);
				}
				else
				{
					Open(session, MenuPage::Home);
				}
			}
		}

		// F5 saves to the quick save, F9 loads it (no second press: it is quick).
		void ReadSaveKeys(HearthlandsSession& session)
		{
			const bool save = Pressed(Key::F5, m_QuickSaveHeld);
			const bool load = Pressed(Key::F9, m_QuickLoadHeld);
			if (save)
			{
				SaveTo(session, 0);
			}
			else if (load)
			{
				LoadFrom(session, 0);
			}
		}

		// Save over a used slot and every load ask for a second press ("Sure?") within a few seconds.
		void SlotPressed(HearthlandsSession& session, const std::string& event)
		{
			const bool load = event.rfind("Load", 0) == 0;
			const int slot = std::stoi(event.substr(10));
			if (slot < 0 || slot >= SavesPageView::SlotCount)
			{
				return;
			}
			const bool used = m_Slots[(size_t)slot].Used;
			if ((load || used) && event != m_Armed)
			{
				SetArmed(event);
				return;
			}
			SetArmed(std::string());
			if (load)
			{
				LoadFrom(session, slot);
			}
			else
			{
				SaveTo(session, slot);
			}
		}

		void SetArmed(const std::string& event)
		{
			m_Armed = event;
			m_ArmedLeft = 4.0f;
			m_View.GetSaves().SetArmed(event);
		}

		// Saves live in "Saves" under the folder the game runs in.
		static std::filesystem::path SaveFolder()
		{
			return std::filesystem::current_path() / "Saves";
		}

		static std::filesystem::path SlotPath(int slot)
		{
			return SaveFolder() / (slot == 0 ? std::string("quicksave.hlsave") : "slot" + std::to_string(slot) + ".hlsave");
		}

		static std::string SlotName(int slot)
		{
			return slot == 0 ? std::string("the quick save") : "slot " + std::to_string(slot);
		}

		// The local time now, as year-month-day hours:minutes.
		static std::string Now()
		{
			const std::time_t now = std::time(nullptr);
			std::tm local{};
#ifdef _WIN32
			localtime_s(&local, &now);
#else
			localtime_r(&now, &local);
#endif
			char text[32] = {};
			std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M", &local);
			return text;
		}

		// Reads at most `most` bytes of a file (all of it for 0); empty when it can't be read.
		static std::vector<uint8_t> ReadFile(const std::filesystem::path& path, size_t most)
		{
			std::ifstream file(path, std::ios::binary);
			std::vector<uint8_t> bytes;
			if (file.is_open() == false)
			{
				return bytes;
			}
			file.seekg(0, std::ios::end);
			const std::streamoff size = file.tellg();
			file.seekg(0, std::ios::beg);
			if (size <= 0)
			{
				return bytes;
			}
			bytes.resize(most > 0 && (size_t)size > most ? most : (size_t)size);
			file.read((char*)bytes.data(), (std::streamsize)bytes.size());
			bytes.resize((size_t)file.gcount());
			return bytes;
		}

		// What each slot holds (only the start of each file is read).
		void RefreshSlots()
		{
			m_Slots.assign(SavesPageView::SlotCount, SavesPageView::Slot{});
			for (int slot = 0; slot < SavesPageView::SlotCount; slot++)
			{
				SaveInfo info;
				if (SaveRules::ReadInfo(ReadFile(SlotPath(slot), 4096), info) == false)
				{
					continue;
				}
				SavesPageView::Slot& shown = m_Slots[(size_t)slot];
				shown.Used = true;
				shown.Title = info.Name;
				shown.Line = "Year " + std::to_string(info.Year) + ", " + info.Season + " day " + std::to_string(info.Day) + "  -  "
					+ std::to_string(info.Settlements) + (info.Settlements == 1 ? " settlement, " : " settlements, ")
					+ std::to_string(info.People) + " people  -  saved " + info.SavedAt;
			}
		}

		void Tell(HearthlandsSession& session, const std::string& text, bool good)
		{
			if (m_View.IsOpen() && m_View.GetPage() == MenuPage::Saves)
			{
				m_View.GetSaves().SetNotice(text, good);
			}
			else
			{
				session.Notices.push_back(text);
			}
		}

		// Writes the run to a slot (a new file first, then over the old, so a failed save keeps the old one).
		void SaveTo(HearthlandsSession& session, int slot)
		{
			const int speed = m_View.IsOpen() && PauseInMenu ? m_SpeedBeforeMenu : session.Clock.Speed;
			const std::vector<uint8_t> bytes = SaveRules::Write(session, SaveRules::Describe(session, Now(), speed));
			const std::filesystem::path path = SlotPath(slot);
			std::filesystem::path temporary = path;
			temporary += ".new";
			std::error_code error;
			std::filesystem::create_directories(SaveFolder(), error);
			bool written = false;
			{
				std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
				if (file.is_open())
				{
					file.write((const char*)bytes.data(), (std::streamsize)bytes.size());
					written = file.good();
				}
			}
			if (written)
			{
				std::filesystem::rename(temporary, path, error);
				written = error.value() == 0;
			}
			if (written == false)
			{
				HY_WARN("WorldMenuScript: couldn't write {}", path.string());
				Tell(session, "Couldn't save to " + SlotName(slot), false);
				return;
			}
			HY_INFO("Saved the world to {} ({} KB)", path.string(), bytes.size() / 1024);
			Tell(session, "Saved to " + SlotName(slot), true);
			RefreshSlots();
		}

		// Loads a slot into the session, then starts the world's scene again so every view is built for the loaded run.
		void LoadFrom(HearthlandsSession& session, int slot)
		{
			const std::vector<uint8_t> bytes = ReadFile(SlotPath(slot), 0);
			SaveInfo info;
			if (bytes.empty())
			{
				Tell(session, std::string("Nothing saved in ") + SlotName(slot) + " yet", false);
				return;
			}
			if (SaveRules::Read(bytes, session, info) == false)
			{
				Tell(session, "Couldn't load " + SlotName(slot) + ": the file is damaged or from a newer game", false);
				return;
			}
			TalentRules::Recount(m_Talents, session.Talents); // what the owned talents add up to, from today's table
			session.Clock.Speed = std::clamp(info.Speed, 0, 10);
			session.FocusTile = session.CameraTile;
			session.FocusRequested = true;
			session.Notices.push_back("Loaded " + info.Name + " (saved " + info.SavedAt + ")");
			session.Loading = true;
			m_Reloading = true;
			HY_INFO("Loaded {} from {}", info.Name, SlotPath(slot).string());
			SceneTransition::Replace(WorldScene);
		}

		// True on the frame the key goes down.
		static bool Pressed(KeyCode key, bool& held)
		{
			const bool down = Input::IsKeyPressed(key);
			const bool pressed = down && held == false;
			held = down;
			return pressed;
		}

		// Buys the chosen talent if the points and its requirement allow; a talent that unlocks a brush adds it to the brushes
		// (the HUD tells of the new brush).
		void Learn(HearthlandsSession& session, int talentId)
		{
			const TalentDefinition* talent = TalentRules::Find(m_Talents, talentId);
			if (talent == nullptr)
			{
				return;
			}
			const TalentRules::Verdict verdict = TalentRules::Buy(m_Talents, session.Talents, talentId);
			TechnologyPageView& page = m_View.GetTechnology();
			if (verdict == TalentRules::Verdict::Buyable)
			{
				for (uint8_t tileId : TalentRules::UnlockedTiles(m_Talents, session.Talents))
				{
					if (std::find(session.UnlockedTiles.begin(), session.UnlockedTiles.end(), tileId) == session.UnlockedTiles.end())
					{
						session.UnlockedTiles.push_back(tileId);
					}
				}
				page.SetNotice("Learned " + talent->Name, true);
			}
			else if (verdict == TalentRules::Verdict::NeedsTalent)
			{
				const TalentDefinition* first = TalentRules::Find(m_Talents, talent->RequiresTalentId);
				page.SetNotice("Learn " + (first != nullptr ? first->Name : std::string("its prerequisite")) + " first", false);
			}
			else if (verdict == TalentRules::Verdict::NeedsMilestone)
			{
				page.SetNotice("Reach " + MilestoneName(talent->RequiresMilestoneId) + " first", false);
			}
			else if (verdict == TalentRules::Verdict::TooDear)
			{
				page.SetNotice("Needs " + std::to_string(talent->Cost) + " points", false);
			}
		}

		std::string MilestoneName(int milestoneId) const
		{
			for (const MilestoneDefinition& milestone : m_Milestones)
			{
				if (milestone.MilestoneId == milestoneId)
				{
					return milestone.Name;
				}
			}
			return "a milestone";
		}

		std::vector<TalentRules::Verdict> Verdicts(const HearthlandsSession& session) const
		{
			std::vector<TalentRules::Verdict> verdicts;
			for (const TalentDefinition& talent : m_Talents)
			{
				verdicts.push_back(TalentRules::Check(m_Talents, session.Talents, talent.TalentId));
			}
			return verdicts;
		}

		std::vector<MilestonesPageView::Line> MilestoneLines(const HearthlandsSession& session) const
		{
			std::vector<MilestonesPageView::Line> lines;
			for (size_t i = 0; i < m_Milestones.size(); i++)
			{
				const MilestoneDefinition& milestone = m_Milestones[i];
				MilestonesPageView::Line line;
				line.Name = milestone.Name;
				line.Description = milestone.Description;
				line.Hint = milestone.Hint;
				line.Hidden = milestone.Hidden;
				line.Track = milestone.Track;
				line.Unit = Unit(milestone);
				line.Kind = milestone.Kind;
				line.Count = milestone.Count;
				line.Points = milestone.Points;
				line.Progress = i < session.MilestoneProgress.size() ? session.MilestoneProgress[i] : 0u;
				line.Reached = TalentRules::Contains(session.Talents.Reached, milestone.MilestoneId);
				lines.push_back(line);
			}
			return lines;
		}

		// What a milestone counts, after its numbers ("12 / 50 tiles of Grass").
		std::string Unit(const MilestoneDefinition& milestone) const
		{
			switch (milestone.Kind)
			{
			case MilestoneKind::Tiles: return "tiles of " + TileName(milestone.TargetId, false);
			case MilestoneKind::Objects:
			{
				const char* kinds[4] = { "ground plants", "shrubs", "trees", "rocks" };
				return milestone.TargetId >= 0 && milestone.TargetId < 4 ? kinds[milestone.TargetId] : "plants";
			}
			case MilestoneKind::Springs: return "springs dug";
			case MilestoneKind::Level: return "levels high";
			case MilestoneKind::Animals:
			{
				for (const AnimalDefinition& animal : m_Animals)
				{
					if (animal.AnimalId == milestone.TargetId)
					{
						return animal.Name;
					}
				}
				return "animals";
			}
			case MilestoneKind::Species:
			{
				const char* kinds[5] = { "kinds of beasts", "kinds of birds", "kinds of fish", "kinds of insects", "kinds of flocks" };
				return milestone.TargetId >= 0 && milestone.TargetId < 5 ? kinds[milestone.TargetId] : "species";
			}
			case MilestoneKind::Habitat:
			{
				const char* waters[10] = { "tiles of water", "tiles of sea", "tiles of stream", "tiles of big river", "tiles of lake",
					"tiles of running water", "tiles of river mouth", "tiles of water joined to the sea", "tiles of mountain stream",
					"tiles of fresh water" };
				return milestone.TargetId >= 0 && milestone.TargetId < 10 ? waters[milestone.TargetId] : "tiles of water";
			}
			case MilestoneKind::RichSoil: return "tiles of rich soil";
			case MilestoneKind::Settlers: return "settlers";
			case MilestoneKind::Ancient: return "ancient trees";
			case MilestoneKind::SettlementLevel: return "settlement level";
			default: return std::string();
			}
		}

		WorldMenuView::Progress HomeProgress(const HearthlandsSession& session) const
		{
			WorldMenuView::Progress progress;
			const SeasonState& season = session.Season;
			progress.Season = season.Name.empty() ? "Day " + std::to_string(session.Clock.GetDay())
				: season.Name + ", day " + std::to_string(season.Day);
			progress.Year = season.Year;
			progress.Points = session.Talents.Points;
			progress.Talents = (int)session.Talents.Owned.size();
			progress.TalentCount = (int)m_Talents.size();
			progress.Milestones = (int)session.Talents.Reached.size();
			progress.MilestoneCount = (int)m_Milestones.size();
			progress.SpeciesCount = (int)m_Animals.size();
			for (size_t s = 0; s < session.Wildlife.Population.size(); s++)
			{
				const uint32_t total = (uint32_t)session.Wildlife.Total(s);
				progress.Species += total > 0 ? 1 : 0;
				progress.Animals += (int)total;
			}
			progress.Brushes = (int)session.UnlockedTiles.size();
			progress.Tip = Tip(session);
			return progress;
		}

		// Points to spend first, else the next milestone not reached yet.
		std::string Tip(const HearthlandsSession& session) const
		{
			if (session.Talents.Points > 0)
			{
				return "You have talent points to spend: open Technology (T).";
			}
			for (const MilestoneDefinition& milestone : m_Milestones)
			{
				if (milestone.Hidden == false && TalentRules::Contains(session.Talents.Reached, milestone.MilestoneId) == false)
				{
					return "Next milestone, " + milestone.Name + ": " + milestone.Description + ".";
				}
			}
			if (session.Talents.Reached.size() < m_Milestones.size())
			{
				return "The rest are hidden: shape the land and water and see what you discover.";
			}
			return "Every milestone reached. Shape the island as you like it.";
		}

		// brush: the brush's name where it has its own (Dig, Raise), else the type's.
		std::string TileName(int tileId, bool brush) const
		{
			for (const TerrainTypeDefinition& type : m_Types)
			{
				if (type.TileId == tileId)
				{
					return brush && type.BrushName.empty() == false ? type.BrushName : type.Name;
				}
			}
			return "tile " + std::to_string(tileId);
		}

		WorldMenuView m_View;
		std::vector<TerrainTypeDefinition> m_Types;
		std::vector<TalentDefinition> m_Talents;
		std::vector<MilestoneDefinition> m_Milestones;
		std::vector<AnimalDefinition> m_Animals;
		int m_SpeedBeforeMenu = 1;
		bool m_TabHeld = false;
		bool m_TalentsHeld = false;
		bool m_EscapeHeld = false;
		bool m_QuickSaveHeld = false;
		bool m_QuickLoadHeld = false;
		std::vector<SavesPageView::Slot> m_Slots = std::vector<SavesPageView::Slot>(SavesPageView::SlotCount);
		std::string m_Armed;     // the slot button waiting for its second press
		float m_ArmedLeft = 0.0f; // seconds it waits
		bool m_Reloading = false; // a save was loaded and the scene asked for again this frame
	};
}
