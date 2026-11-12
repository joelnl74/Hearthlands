#pragma once

#include "Hydro.h"
#include "Hydro/Renderer/Font.h"
#include "Hydro/Scene/UI/UI.h"
#include "Data/AnimalDefinition.h"
#include "Data/MilestoneDefinition.h"
#include "Data/PeopleDefinition.h"
#include "Data/TalentDefinition.h"
#include "Data/TerrainTypeDefinition.h"
#include "Rules/BrushRules.h"
#include "State/HearthlandsSession.h"
#include "Views/HudView.h"
#include "Views/MenuArt.h"
#include "Views/SettlementDetailView.h"
#include "Views/SettlementsPanelView.h"
#include "Views/UIBuilder.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Hydro
{
	// The world's controls on the HUD entity (made the HUD canvas): brush, size, speed, overlays and settlements from buttons
	// and keys (1-9, [ ], P, M, F, G) into the session; tells of new brushes, milestones, settlers and of the pointer.
	HY_SCRIPT()
	class HudScript : public ScriptableEntity
	{
	public:
		static constexpr uint64_t DefaultFont = 7311820458392716001ull;         // Assets/Fonts/OpenSans.fnt
		static constexpr uint64_t DefaultHeadingFont = 7311820458392716021ull;  // Assets/Fonts/MenuHeading.fnt
		static constexpr uint64_t DefaultArt = 4973082495598781234ull;          // Assets/Textures/UI/MenuArt.png
		static constexpr uint64_t DefaultTerrainTypes = 4392505292887402202ull; // Assets/Data/TerrainTypes.hytable
		static constexpr uint64_t DefaultMilestones = 5179923417704519381ull;   // Assets/Data/Milestones.hytable
		static constexpr uint64_t DefaultPeoples = 3987286432867960408ull;      // Assets/Data/Peoples.hytable
		static constexpr uint64_t DefaultAnimals = 8152222171692113247ull;      // Assets/Data/Animals.hytable

		HY_PROPERTY(Asset(Font), Tooltip("The HUD's text"))
		AssetHandle UIFont = DefaultFont;
		HY_PROPERTY(Asset(Font), Tooltip("The clock and the size buttons (the World Menu's heading font)"))
		AssetHandle HeadingFont = DefaultHeadingFont;
		HY_PROPERTY(Asset(Texture2D), Tooltip("The World Menu's sprites by name (scripts/art/make_ui_art.py); none draws flat colours"))
		AssetHandle Art = DefaultArt;
		HY_PROPERTY(Asset(DataTable, TerrainTypeDefinition), Tooltip("The brushes' names and colours"))
		AssetHandle TerrainTypes = DefaultTerrainTypes;
		HY_PROPERTY(Asset(DataTable, MilestoneDefinition), Tooltip("What gives talent points (the same table as SimulationScript's)"))
		AssetHandle Milestones = DefaultMilestones;
		HY_PROPERTY(Asset(DataTable, PeopleDefinition), Tooltip("The peoples, for the settlement panels (as SimulationScript's)"))
		AssetHandle Peoples = DefaultPeoples;
		HY_PROPERTY(Asset(DataTable, AnimalDefinition), Tooltip("The wild animals, for discoveries (the same table as SimulationScript's)"))
		AssetHandle Animals = DefaultAnimals;

	protected:
		void OnCreate() override
		{
			for (AssetHandle font : { UIFont, HeadingFont })
			{
				if (font != 0)
				{
					AssetManager::GetAsset<Font>(font); // the renderer never loads
				}
			}
			const DataTableView<TerrainTypeDefinition> table = Data::GetTable<TerrainTypeDefinition>(TerrainTypes);
			for (const auto& [id, type] : table.Rows())
			{
				m_Types.push_back(type);
			}
			const DataTableView<MilestoneDefinition> milestones = Data::GetTable<MilestoneDefinition>(Milestones);
			for (const auto& [id, milestone] : milestones.Rows())
			{
				m_Milestones.push_back(milestone);
			}
			for (const auto& [id, people] : Data::GetTable<PeopleDefinition>(Peoples).Rows())
			{
				m_Peoples.push_back(people);
			}
			for (const auto& [id, animal] : Data::GetTable<AnimalDefinition>(Animals).Rows())
			{
				m_Animals.push_back(animal);
			}
			MenuFonts fonts;
			fonts.Body = UIFont;
			fonts.Heading = HeadingFont;
			m_Art.Load(Art);
			m_UI.Bind(*GetEntity().GetScene(), fonts, m_Art);
			m_View.Bind(GetEntity(), m_UI);
			m_Settlements.Bind(GetEntity(), m_UI);
			m_Detail.Bind(GetEntity(), m_UI);
		}

		void OnUpdate(float deltaTime) override
		{
			HearthlandsSession& session = HearthlandsSession::Get();
			if (session.Loading)
			{
				return; // a save was loaded: the scene starts again with it
			}
			const TimedScope timed(session.Timings, TimedPart::Hud);
			if (session.UnlockedTiles != m_Unlocked)
			{
				AnnounceUnlocks(session.UnlockedTiles);
				m_Unlocked = session.UnlockedTiles;
				m_View.SetBrushes(Choices(m_Unlocked));
			}
			ReadKeys(session);
			AnnounceMilestones(session.Talents);
			AnnounceDiscoveries(session.Wildlife);
			session.Brush.TileId = BrushRules::BrushTile(session.Brush.TileId, session.UnlockedTiles);
			session.Brush.Radius = BrushRules::StepRadius(session.Brush.Radius, 0, LargestRadius(session));
			Scene& scene = *GetEntity().GetScene();
			session.PointerOverHud = m_View.ContainsPointer(scene) || m_Settlements.ContainsPointer(scene)
				|| m_Detail.ContainsPointer(scene);
			if (m_CreekHinted == false && session.DryCreeks.empty() == false)
			{
				m_View.ShowNotice("Dry creeks: dig at a creek's upper end (Dig brush) to open its spring", 9.0f,
					MenuArt::Sprite::IconDrop);
				m_CreekHinted = true;
			}
			for (const std::string& notice : session.Notices)
			{
				m_View.ShowNotice(notice, 6.0f, MenuArt::Sprite::IconFire); // the settlers' news
			}
			session.Notices.clear();
			m_View.Present(session.Brush.TileId, session.Brush.Radius, session.Clock.Speed, session.Clock.GetDay(), deltaTime,
				SeasonLabel(session.Season));
			const bool groundwaterLearned = session.Talents.Get(TalentEffect::GroundwaterView) > 0.5f;
			// One overlay at a time: another turned on (here or in Settings) turns the groundwater off.
			session.ShowGroundwater = session.ShowGroundwater && groundwaterLearned && session.ShowMoisture == false
				&& session.ShowSoil == false;
			m_View.PresentOverlays(session.ShowMoisture, session.ShowSoil, session.ShowGroundwater, groundwaterLearned);
			m_Settlements.Present(session.Settlements, m_Peoples, Settling(session), session.SelectedSettlement);
			m_Detail.Present(session.Settlements, m_Peoples, session.SelectedSettlement);
		}

		bool OnUIEvent(const UIEvent& event) override
		{
			if (event.Type != UIEventType::Submit)
			{
				return false;
			}
			HearthlandsSession& session = HearthlandsSession::Get();
			const std::string& name = event.Name;
			if (name.rfind("Brush ", 0) == 0)
			{
				session.Brush.TileId = (uint8_t)std::stoi(name.substr(6));
			}
			else if (name == "Size -" || name == "Size +")
			{
				session.Brush.Radius = BrushRules::StepRadius(session.Brush.Radius, name == "Size +" ? 1 : -1, LargestRadius(session));
			}
			else if (name.rfind("Speed ", 0) == 0)
			{
				session.Clock.Speed = std::stoi(name.substr(6));
			}
			else if (name == "Overlay Moisture")
			{
				ToggleMoisture(session);
			}
			else if (name == "Overlay Soil")
			{
				ToggleSoil(session);
			}
			else if (name == "Overlay Groundwater")
			{
				ToggleGroundwater(session);
			}
			else if (name.rfind("Camp ", 0) == 0)
			{
				Select(session, std::stoi(name.substr(5)));
			}
			else if (name == "Detail Close")
			{
				session.SelectedSettlement = 0;
			}
			else if (name.rfind("Focus ", 0) == 0)
			{
				SetFocus(session, name.substr(6));
			}
			else if (name.rfind("Rename ", 0) == 0)
			{
				session.RenameRequested = std::stoi(name.substr(7));
			}
			UI::ClearFocus(*GetEntity().GetScene()); // so Space and Enter stay with the game
			return true;
		}

	private:
		// "Spring, day 3" in the first year, "Spring 3, year 2" after it; empty without seasons.
		static std::string SeasonLabel(const SeasonState& season)
		{
			if (season.Name.empty())
			{
				return std::string();
			}
			const std::string day = std::to_string(season.Day);
			return season.Year <= 1 ? season.Name + ", day " + day : season.Name + " " + day + ", year " + std::to_string(season.Year);
		}

		// Whether a talent lets any people settle.
		bool Settling(const HearthlandsSession& session) const
		{
			for (const PeopleDefinition& people : m_Peoples)
			{
				if (session.Talents.SettleChance(people.PeopleId) > 0.0f)
				{
					return true;
				}
			}
			return false;
		}

		// "<id> <focus>": the settlement leans its villagers towards food, wood or building (0 none).
		static void SetFocus(HearthlandsSession& session, const std::string& text)
		{
			const size_t space = text.find(' ');
			if (space == std::string::npos)
			{
				return;
			}
			const int id = std::stoi(text.substr(0, space));
			const int focus = std::clamp(std::stoi(text.substr(space + 1)), 0, (int)SettlementFocus::Building);
			for (Settlement& settlement : session.Settlements.List)
			{
				if (settlement.Id == id)
				{
					settlement.Focus = (SettlementFocus)focus;
					session.Settlements.Revision++;
				}
			}
		}

		// Its detail opens and the camera goes to its centre.
		static void Select(HearthlandsSession& session, int id)
		{
			for (const Settlement& settlement : session.Settlements.List)
			{
				if (settlement.Id == id)
				{
					session.SelectedSettlement = id;
					session.FocusTile = glm::vec2((float)settlement.X, (float)settlement.Y);
					session.FocusRequested = true;
				}
			}
		}

		static float LargestRadius(const HearthlandsSession& session)
		{
			return 8.0f + session.Talents.Get(TalentEffect::BrushSize);
		}

		// A notice for each milestone reached since the last update.
		void AnnounceMilestones(const TalentState& talents)
		{
			for (size_t i = m_AnnouncedMilestones; i < talents.Reached.size(); i++)
			{
				for (const MilestoneDefinition& milestone : m_Milestones)
				{
					if (milestone.MilestoneId == talents.Reached[i])
					{
						const std::string unit = milestone.Points == 1 ? " talent point" : " talent points";
						m_View.ShowToast(MenuArt::Sprite::IconTrophy, (milestone.Hidden ? "Discovery: " : "Milestone: ") + milestone.Name,
							"+" + std::to_string(milestone.Points) + unit + "  (T: Technology)", 7.0f);
					}
				}
			}
			m_AnnouncedMilestones = talents.Reached.size();
		}

		// A notice for each hidden species seen for the first time since the last update (the world's own count as seen).
		void AnnounceDiscoveries(const WildlifeState& wildlife)
		{
			if (wildlife.Found.size() < m_AnnouncedSpecies)
			{
				m_AnnouncedSpecies = 0; // a new world
			}
			for (size_t i = m_AnnouncedSpecies; i < wildlife.Found.size(); i++)
			{
				const int row = wildlife.Found[i];
				if (row >= 0 && (size_t)row < m_Animals.size() && m_Animals[(size_t)row].Hidden)
				{
					const AnimalDefinition& animal = m_Animals[(size_t)row];
					const bool water = animal.Kind == AnimalKind::Fish;
					m_View.ShowToast(water ? MenuArt::Sprite::IconDrop : MenuArt::Sprite::IconLeaf, "New species: " + animal.Name,
						animal.DiscoveryText, 8.0f);
				}
			}
			m_AnnouncedSpecies = wildlife.Found.size();
		}

		// A notice for each brush that joined the list since the last update (none for the starting brushes).
		void AnnounceUnlocks(const std::vector<uint8_t>& unlocked)
		{
			if (m_Unlocked.empty())
			{
				return;
			}
			for (size_t i = 0; i < unlocked.size(); i++)
			{
				bool known = false;
				for (uint8_t tileId : m_Unlocked)
				{
					known = known || tileId == unlocked[i];
				}
				if (known == false)
				{
					const std::string key = i < 9 ? " (key " + std::to_string(i + 1) + ")" : std::string();
					m_View.ShowNotice("New brush: " + Label(unlocked[i]) + key, 6.0f);
				}
			}
		}

		std::string Label(uint8_t tileId) const
		{
			for (const TerrainTypeDefinition& type : m_Types)
			{
				if (type.TileId == (int)tileId)
				{
					return type.BrushName.empty() ? type.Name : type.BrushName;
				}
			}
			return "Tile " + std::to_string((int)tileId);
		}

		// The unlocked types with their names and colours, in id order.
		std::vector<HudView::BrushChoice> Choices(const std::vector<uint8_t>& unlocked) const
		{
			std::vector<HudView::BrushChoice> choices;
			for (uint8_t tileId : unlocked)
			{
				HudView::BrushChoice choice;
				choice.TileId = tileId;
				choice.Name = Label(tileId);
				for (const TerrainTypeDefinition& type : m_Types)
				{
					if (type.TileId == (int)tileId)
					{
						choice.Color = glm::vec4(type.Color, 1.0f);
					}
				}
				choices.push_back(choice);
			}
			return choices;
		}

		void ReadKeys(HearthlandsSession& session)
		{
			if (UI::CapturesInput(*GetEntity().GetScene()))
			{
				return;
			}
			const KeyCode digits[9] = { Key::D1, Key::D2, Key::D3, Key::D4, Key::D5, Key::D6, Key::D7, Key::D8, Key::D9 };
			for (int i = 0; i < 9 && i < (int)session.UnlockedTiles.size(); i++)
			{
				if (Input::IsKeyPressed(digits[i]))
				{
					session.Brush.TileId = session.UnlockedTiles[(size_t)i];
				}
			}
			if (Pressed(Key::LeftBracket, m_SmallerHeld))
			{
				session.Brush.Radius = BrushRules::StepRadius(session.Brush.Radius, -1, LargestRadius(session));
			}
			if (Pressed(Key::RightBracket, m_LargerHeld))
			{
				session.Brush.Radius = BrushRules::StepRadius(session.Brush.Radius, 1, LargestRadius(session));
			}
			if (Pressed(Key::P, m_PauseHeld))
			{
				if (session.Clock.Speed == 0)
				{
					session.Clock.Speed = m_SpeedBeforePause;
				}
				else
				{
					m_SpeedBeforePause = session.Clock.Speed;
					session.Clock.Speed = 0;
				}
			}
			if (session.Clock.Speed > 0)
			{
				m_SpeedBeforePause = session.Clock.Speed;
			}
			if (Pressed(Key::M, m_MoistureHeld))
			{
				ToggleMoisture(session);
			}
			if (Pressed(Key::F, m_SoilHeld))
			{
				ToggleSoil(session);
			}
			if (Pressed(Key::G, m_GroundwaterHeld))
			{
				ToggleGroundwater(session);
			}
		}

		// One overlay at a time (M or the Inspect bar).
		static void ToggleMoisture(HearthlandsSession& session)
		{
			session.ShowMoisture = session.ShowMoisture == false;
			session.ShowSoil = false;
		}

		void ToggleSoil(HearthlandsSession& session)
		{
			session.ShowSoil = session.ShowSoil == false;
			session.ShowMoisture = false;
			if (session.ShowSoil)
			{
				m_View.ShowNotice("Soil: pale poor, green rich. Dirt over sand makes loam", 6.0f, MenuArt::Sprite::IconSprout);
			}
		}

		// G or the Inspect bar, once Groundwater sight is learned: where painted grass holds (teal) and digging springs (blue).
		void ToggleGroundwater(HearthlandsSession& session)
		{
			if (session.Talents.Get(TalentEffect::GroundwaterView) < 0.5f)
			{
				m_View.ShowNotice("Groundwater: learn Groundwater sight (T: Technology, Insight) to see it", 6.0f,
					MenuArt::Sprite::IconDrop);
				return;
			}
			session.ShowGroundwater = session.ShowGroundwater == false;
			session.ShowMoisture = false;
			session.ShowSoil = false;
			if (session.ShowGroundwater)
			{
				m_View.ShowNotice("Groundwater: teal holds painted grass and grows forest; dig a bed on the dots for a spring", 7.0f,
					MenuArt::Sprite::IconDrop);
			}
		}

		// True on the frame the key goes down.
		static bool Pressed(KeyCode key, bool& held)
		{
			const bool down = Input::IsKeyPressed(key);
			const bool pressed = down && held == false;
			held = down;
			return pressed;
		}

		HudView m_View;
		MenuArt m_Art;
		UIBuilder m_UI; // the views keep a pointer to it, it to m_Art
		SettlementsPanelView m_Settlements;
		SettlementDetailView m_Detail;
		std::vector<TerrainTypeDefinition> m_Types;
		std::vector<MilestoneDefinition> m_Milestones;
		std::vector<PeopleDefinition> m_Peoples;
		size_t m_AnnouncedMilestones = 0;
		size_t m_AnnouncedSpecies = 0;
		std::vector<AnimalDefinition> m_Animals;
		std::vector<uint8_t> m_Unlocked;
		int m_SpeedBeforePause = 1;
		bool m_SmallerHeld = false;
		bool m_LargerHeld = false;
		bool m_PauseHeld = false;
		bool m_MoistureHeld = false;
		bool m_GroundwaterHeld = false;
		bool m_SoilHeld = false;
		bool m_CreekHinted = false; // the dry creeks' hint, once a run
	};
}
