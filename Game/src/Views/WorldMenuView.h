#pragma once

#include "Views/MenuArt.h"
#include "Views/MilestonesPageView.h"
#include "Views/SavesPageView.h"
#include "Views/SettingsPageView.h"
#include "Views/TechnologyPageView.h"
#include "Views/UIBuilder.h"

#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/UI/UI.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace Hydro
{
	enum class MenuPage : uint8_t
	{
		Home,
		Technology,
		Milestones,
		Settings,
		Codex,
		Map,
		Saves
	};

	// The World Menu on its own canvas over the HUD (built in code): a "Menu" button while closed; open (modal), the home
	// page's tiles and the island's progress, or a page. Buttons send "Menu", "Close", "Home", "Open <Page>" and pages' events.
	class WorldMenuView
	{
	public:
		// The home page's panel: the island now.
		struct Progress
		{
			std::string Season;
			int Year = 1;
			int Points = 0;
			int Talents = 0;
			int TalentCount = 0;
			int Milestones = 0;
			int MilestoneCount = 0;
			int Species = 0;
			int SpeciesCount = 0;
			int Animals = 0;
			int Brushes = 0;
			std::string Tip;
		};

		void Bind(Entity root, const MenuFonts& fonts, AssetHandle art, const std::vector<TalentDefinition>& talents,
			const std::vector<std::string>& effects, const std::vector<MilestonesPageView::Line>& milestones)
		{
			m_Root = root;
			m_Art.Load(art);
			m_UI.Bind(*root.GetScene(), fonts, m_Art);
			UICanvasComponent& canvas = root.HasComponent<UICanvasComponent>() ? root.GetComponent<UICanvasComponent>()
				: root.AddComponent<UICanvasComponent>();
			canvas.ReferenceResolution = glm::vec2(1920.0f, 1080.0f);
			canvas.ScaleMode = UIScaleMode::ScaleWithScreen;
			canvas.SortOrder = 20; // over the HUD (10)
			canvas.BlocksGameplayInput = false;

			m_Button = m_UI.Element(root, "Menu Button", glm::vec2(1.0f, 1.0f), glm::vec2(-16.0f, -100.0f), glm::vec2(320.0f, 60.0f));
			m_UI.Art(m_Button, MenuArt::Sprite::ButtonDark);
			UIBuilder::Button(m_Button, "Menu").Navigation = UINavigation::None; // WASD moves the camera while closed
			m_ButtonText = m_UI.Fill(m_Button, "Label");
			m_UI.Text(m_ButtonText, "Menu (Tab)", TextStyle::Body, TextAlignH::Center, MenuArt::Ink());

			m_Window = m_UI.Fill(root, "Menu Window");
			UIBuilder::Flat(m_UI.Fill(m_Window, "Backdrop"), MenuArt::Backdrop());
			for (int p = 0; p <= (int)MenuPage::Saves; p++)
			{
				m_Pages.push_back(m_UI.Fill(m_Window, std::string("Page ") + PageName((MenuPage)p)));
			}
			BindHome(m_Pages[(size_t)MenuPage::Home]);
			m_Technology.Bind(m_UI, m_Pages[(size_t)MenuPage::Technology], talents, effects);
			m_Milestones.Bind(m_UI, m_Pages[(size_t)MenuPage::Milestones], milestones);
			m_Settings.Bind(m_UI, m_Pages[(size_t)MenuPage::Settings]);
			BindComingLater(m_Pages[(size_t)MenuPage::Codex], "Codex",
				"Biomes, peoples, animals and brushes as a reference, filled in as you discover them.");
			BindComingLater(m_Pages[(size_t)MenuPage::Map], "Map Overview",
				"The whole island at a glance: its regions, what grows and lives there, and where water runs.");
			m_Saves.Bind(m_UI, m_Pages[(size_t)MenuPage::Saves]);
			for (int p = 1; p <= (int)MenuPage::Saves; p++)
			{
				BackButton(m_Pages[(size_t)p]);
			}

			Entity close = m_UI.Element(m_Window, "Close", glm::vec2(1.0f, 1.0f), glm::vec2(-40.0f, -36.0f), glm::vec2(64.0f, 64.0f));
			m_UI.Art(close, MenuArt::Sprite::ButtonDark);
			UIBuilder::Button(close, "Close");
			m_UI.Text(m_UI.Fill(close, "Label"), "X", TextStyle::Heading, TextAlignH::Center, MenuArt::Ink());
			UI::SetVisible(m_Window, false);
		}

		bool IsOpen() const
		{
			return m_Open;
		}

		MenuPage GetPage() const
		{
			return m_Page;
		}

		void Show(MenuPage page)
		{
			m_Open = true;
			m_Page = page;
			m_Root.GetComponent<UICanvasComponent>().BlocksGameplayInput = true;
			UI::SetVisible(m_Window, true);
			UI::SetVisible(m_Button, false);
			for (size_t p = 0; p < m_Pages.size(); p++)
			{
				UI::SetVisible(m_Pages[p], p == (size_t)page);
			}
		}

		void Hide()
		{
			m_Open = false;
			m_Root.GetComponent<UICanvasComponent>().BlocksGameplayInput = false;
			UI::SetVisible(m_Window, false);
			UI::SetVisible(m_Button, true);
		}

		// The button the keyboard starts on in the page shown.
		Entity GetDefaultFocus() const
		{
			switch (m_Page)
			{
			case MenuPage::Home: return m_Tiles.empty() ? Entity() : m_Tiles.front();
			case MenuPage::Technology: return m_Technology.GetSelectedButton();
			case MenuPage::Milestones: return m_Milestones.GetSelectedButton();
			case MenuPage::Settings: return m_Settings.GetSectionButton();
			case MenuPage::Saves: return m_Saves.GetFirstButton();
			default: return m_BackButtons.empty() ? Entity() : m_BackButtons.back();
			}
		}

		TechnologyPageView& GetTechnology()
		{
			return m_Technology;
		}

		MilestonesPageView& GetMilestones()
		{
			return m_Milestones;
		}

		SettingsPageView& GetSettings()
		{
			return m_Settings;
		}

		SavesPageView& GetSaves()
		{
			return m_Saves;
		}

		// The closed menu's button: gold with points to spend.
		void PresentButton(int points)
		{
			if (points == m_ShownPoints)
			{
				return;
			}
			m_ShownPoints = points;
			const std::string unit = points == 1 ? " point" : " points";
			UI::SetText(m_ButtonText, points > 0 ? "Menu (Tab):  " + std::to_string(points) + unit : std::string("Menu (Tab)"));
			m_UI.Art(m_Button, points > 0 ? MenuArt::Sprite::ButtonPurple : MenuArt::Sprite::ButtonDark);
		}

		void PresentHome(const Progress& progress)
		{
			const std::string values[RowCount] = { progress.Season.empty() ? std::string("-") : progress.Season,
				std::to_string(progress.Year), std::to_string(progress.Points),
				std::to_string(progress.Talents) + " / " + std::to_string(progress.TalentCount),
				std::to_string(progress.Milestones) + " / " + std::to_string(progress.MilestoneCount),
				std::to_string(progress.Species) + " / " + std::to_string(progress.SpeciesCount), std::to_string(progress.Animals),
				std::to_string(progress.Brushes) };
			for (int i = 0; i < RowCount; i++)
			{
				if (values[i] != m_ShownValues[i])
				{
					m_ShownValues[i] = values[i];
					UI::SetText(m_Values[i], values[i]);
				}
			}
			UIBuilder::SetColor(m_Values[2], progress.Points > 0 ? MenuArt::Gold() : MenuArt::Ink());
			if (progress.Tip != m_ShownTip)
			{
				m_ShownTip = progress.Tip;
				UI::SetText(m_Tip, progress.Tip);
			}
		}

		// Whether the pointer is over the closed menu's button, as of the last layout (the brush doesn't paint there).
		bool ContainsPointer(Scene& scene) const
		{
			const Scene::UIPointer& pointer = scene.GetUIPointer();
			UICanvasMetrics metrics;
			if (pointer.Inside == false || m_Root.IsValid() == false || scene.GetUICanvasMetrics(m_Root.GetUUID(), metrics) == false
				|| metrics.Scale <= 0.0f)
			{
				return false;
			}
			const glm::vec2 point = (pointer.Position - metrics.Offset) / metrics.Scale;
			return m_Open || UIBuilder::Contains(m_Button, point);
		}

	private:
		static constexpr int RowCount = 8;
		static constexpr float TileWidth = 400.0f;
		static constexpr float TileHeight = 380.0f;
		static constexpr float TileGap = 28.0f;
		static constexpr float TilesX = 80.0f;
		static constexpr float TilesY = 180.0f;

		static const char* PageName(MenuPage page)
		{
			const char* names[7] = { "Home", "Technology", "Milestones", "Settings", "Codex", "Map", "Saves" };
			return names[(int)page];
		}

		void BindHome(Entity page)
		{
			Entity emblem = m_UI.At(page, "Emblem", 70.0f, 22.0f, 112.0f, 112.0f);
			m_UI.Art(emblem, MenuArt::Sprite::Medallion);
			m_UI.Text(m_UI.Fill(emblem, "Letter", glm::vec4(0.0f, 6.0f, 0.0f, 0.0f)), "H", TextStyle::Title, TextAlignH::Center,
				MenuArt::Gold());
			m_UI.Label(page, "Title", 200.0f, 36.0f, 800.0f, 80.0f, "World Menu", TextStyle::Title, TextAlignH::Left, MenuArt::Gold());
			m_UI.Art(m_UI.At(page, "Ornament", 150.0f, 116.0f, 512.0f, 24.0f), MenuArt::Sprite::Ornament);

			struct TileInfo
			{
				const char* Title;
				const char* Description;
				const char* Event;
				glm::vec4 Tint;
			};
			const TileInfo tiles[6] = {
				{ "Technology", "Talents and upgrades to shape your world.", "Open Technology",
					glm::vec4(0.78f, 0.62f, 1.0f, 1.0f) },
				{ "Milestones", "World goals that give talent points.", "Open Milestones", glm::vec4(1.0f, 0.84f, 0.50f, 1.0f) },
				{ "Settings", "Controls, audio, visuals and gameplay options.", "Open Settings", glm::vec4(0.70f, 0.78f, 0.90f, 1.0f) },
				{ "Codex", "Biomes, peoples, animals and brushes.", "Open Codex", glm::vec4(0.95f, 0.78f, 0.58f, 1.0f) },
				{ "Map Overview", "Island stats and region summaries.", "Open Map", glm::vec4(0.55f, 0.88f, 0.90f, 1.0f) },
				{ "Return to World", "Close the menu and keep shaping your island.", "Close", glm::vec4(0.62f, 0.95f, 0.55f, 1.0f) } };
			for (int i = 0; i < 6; i++)
			{
				const float x = TilesX + (float)(i % 3) * (TileWidth + TileGap);
				const float y = TilesY + (float)(i / 3) * (TileHeight + TileGap);
				Entity tile = m_UI.At(page, std::string("Tile ") + tiles[i].Title, x, y, TileWidth, TileHeight);
				m_UI.Art(tile, i == 5 ? MenuArt::Sprite::TileGreen : MenuArt::Sprite::Tile);
				UIBuilder::Button(tile, tiles[i].Event);
				m_UI.Art(m_UI.At(tile, "Picture", 14.0f, 14.0f, TileWidth - 28.0f, 210.0f), MenuArt::Sprite::ArtSlot, tiles[i].Tint);
				m_UI.Label(tile, "Title", 12.0f, 234.0f, TileWidth - 24.0f, 52.0f, tiles[i].Title, TextStyle::Heading, TextAlignH::Center,
					MenuArt::Gold());
				m_UI.Label(tile, "Description", 28.0f, 292.0f, TileWidth - 56.0f, 76.0f, tiles[i].Description, TextStyle::Body,
					TextAlignH::Center, MenuArt::Muted(), true);
				m_Tiles.push_back(tile);
			}
			Entity saves = m_UI.At(page, "Saved Worlds", TilesX, TilesY + 2.0f * TileHeight + TileGap + 18.0f,
				3.0f * TileWidth + 2.0f * TileGap, 64.0f);
			m_UI.Art(saves, MenuArt::Sprite::ButtonDark);
			UIBuilder::Button(saves, "Open Saves");
			m_UI.Text(m_UI.Fill(saves, "Label"), "Saved Worlds: save and load   (F5 quick save, F9 quick load)", TextStyle::Body,
				TextAlignH::Center, MenuArt::Ink());

			Entity panel = m_UI.Element(page, "Island Progress", glm::vec2(1.0f, 1.0f),
				glm::vec2(-UIBuilder::PageMargin, -(TilesY - 30.0f)),
				glm::vec2(UIBuilder::DetailWidth, 2.0f * TileHeight + TileGap + 30.0f));
			m_UI.Art(panel, MenuArt::Sprite::Panel);
			m_UI.Label(panel, "Heading", 0.0f, 22.0f, UIBuilder::DetailWidth, 52.0f, "Island Progress", TextStyle::Heading,
				TextAlignH::Center, MenuArt::Gold());
			m_UI.Art(m_UI.At(panel, "Ornament", (UIBuilder::DetailWidth - 420.0f) * 0.5f, 78.0f, 420.0f, 24.0f), MenuArt::Sprite::Ornament);
			const char* labels[RowCount] = { "Season", "Year", "Talent points", "Talents learned", "Milestones", "Animal species",
				"Animals", "Brushes" };
			for (int i = 0; i < RowCount; i++)
			{
				const float y = 116.0f + (float)i * 60.0f;
				Entity band = m_UI.At(panel, std::string("Row ") + labels[i], 20.0f, y, UIBuilder::DetailWidth - 40.0f, 52.0f);
				UIBuilder::Flat(band, MenuArt::Band());
				m_UI.Label(band, "Label", 16.0f, 0.0f, 220.0f, 52.0f, labels[i], TextStyle::Body, TextAlignH::Left, MenuArt::Muted());
				m_Values[i] = m_UI.Label(band, "Value", 200.0f, 0.0f, UIBuilder::DetailWidth - 256.0f, 52.0f, "-", TextStyle::Body,
					TextAlignH::Right, MenuArt::Ink());
			}
			Entity tip = m_UI.At(panel, "Tip", 20.0f, 116.0f + (float)RowCount * 60.0f + 14.0f, UIBuilder::DetailWidth - 40.0f, 166.0f);
			m_UI.Art(tip, MenuArt::Sprite::Parchment);
			m_UI.Label(tip, "Heading", 24.0f, 12.0f, 300.0f, 40.0f, "World Tip", TextStyle::Heading, TextAlignH::Left, MenuArt::DarkInk());
			m_Tip = m_UI.Label(tip, "Text", 24.0f, 56.0f, UIBuilder::DetailWidth - 88.0f, 104.0f, std::string(), TextStyle::Body,
				TextAlignH::Left, MenuArt::DarkInk(), true);
		}

		void BindComingLater(Entity page, const std::string& title, const std::string& text)
		{
			m_UI.PageHeader(page, title, std::string());
			Entity card = m_UI.Element(page, "Coming Later", glm::vec2(0.5f, 0.5f), glm::vec2(0.0f, 20.0f), glm::vec2(900.0f, 340.0f));
			m_UI.Art(card, MenuArt::Sprite::Parchment);
			Entity letter = m_UI.Emblem(card);
			UI::SetText(letter, title.substr(0, 1));
			m_UI.Label(card, "Heading", 0.0f, 156.0f, 900.0f, 52.0f, "Coming later", TextStyle::Heading, TextAlignH::Center,
				MenuArt::DarkInk());
			m_UI.Label(card, "Text", 80.0f, 216.0f, 740.0f, 100.0f, text, TextStyle::Body, TextAlignH::Center, MenuArt::DarkMuted(), true);
		}

		void BackButton(Entity page)
		{
			Entity back = m_UI.Element(page, "Back", glm::vec2(0.0f, 0.0f), glm::vec2(UIBuilder::PageMargin, 32.0f),
				glm::vec2(UIBuilder::NavWidth, 64.0f));
			m_UI.Art(back, MenuArt::Sprite::ButtonDark);
			UIBuilder::Button(back, "Home");
			m_UI.Text(m_UI.Fill(back, "Label"), "<  Back (Esc)", TextStyle::Body, TextAlignH::Center, MenuArt::Ink());
			m_BackButtons.push_back(back);
		}

		Entity m_Root;
		MenuArt m_Art;
		UIBuilder m_UI;
		Entity m_Button;
		Entity m_ButtonText;
		Entity m_Window;
		std::vector<Entity> m_Pages;
		std::vector<Entity> m_Tiles;
		std::vector<Entity> m_BackButtons;
		Entity m_Values[RowCount];
		Entity m_Tip;
		TechnologyPageView m_Technology;
		MilestonesPageView m_Milestones;
		SettingsPageView m_Settings;
		SavesPageView m_Saves;
		bool m_Open = false;
		MenuPage m_Page = MenuPage::Home;
		int m_ShownPoints = -1;
		std::string m_ShownValues[RowCount];
		std::string m_ShownTip;
	};
}
