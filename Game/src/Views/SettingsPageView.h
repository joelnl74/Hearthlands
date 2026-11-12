#pragma once

#include "Views/UIBuilder.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace Hydro
{
	// The World Menu's Settings page: sections on the left (Gameplay, Controls, the rest empty until they have options), the
	// rows in the middle, the section on parchment. Buttons send "Section <n>", "Toggle Moisture", "Toggle Soil", "Toggle Pause".
	class SettingsPageView
	{
	public:
		void Bind(const UIBuilder& ui, Entity page)
		{
			m_UI = &ui;
			ui.PageHeader(page, "Settings", std::string());
			Entity nav = ui.NavPanel(page, "Sections", SectionCount);
			for (int s = 0; s < SectionCount; s++)
			{
				Entity count;
				m_SectionButtons.push_back(ui.NavButton(nav, s, SectionName(s), "Section " + std::to_string(s), count));
			}

			const float width = UIBuilder::ContentRight - UIBuilder::ContentX;
			Entity panel = ui.At(page, "Rows", UIBuilder::ContentX, UIBuilder::PageTop, width, PanelHeight);
			ui.Art(panel, MenuArt::Sprite::Panel);
			for (int s = 0; s < SectionCount; s++)
			{
				Entity section = ui.Fill(panel, std::string("Section ") + SectionName(s));
				ui.Label(section, "Heading", 32.0f, 22.0f, width - 64.0f, 52.0f, SectionName(s), TextStyle::Heading, TextAlignH::Left,
					MenuArt::Gold());
				m_Sections.push_back(section);
			}

			Entity gameplay = m_Sections[0];
			m_MoistureToggle = ToggleRow(gameplay, 0, "Moisture overlay (M)", "Toggle Moisture");
			m_SoilToggle = ToggleRow(gameplay, 1, "Soil overlay (F)", "Toggle Soil");
			m_PauseToggle = ToggleRow(gameplay, 2, "Pause while the menu is open", "Toggle Pause");

			const char* keys[][2] = { { "Left mouse", "Paint with the brush" }, { "Right mouse / WASD", "Move the camera" },
				{ "Q / E", "Zoom" }, { "1 - 9", "Choose a brush" }, { "[  ]", "Brush size" }, { "P", "Pause" },
				{ "M / F / G", "Moisture / soil / groundwater overlay" }, { "Click a settlement", "Its villagers, jobs and needs" },
				{ "Tab", "World menu" }, { "T", "Technology" }, { "Esc", "Back / close the menu" } };
			for (int i = 0; i < 11; i++)
			{
				const float y = RowsTop + (float)i * 56.0f;
				Entity band = ui.At(m_Sections[1], "Row", 24.0f, y, width - 48.0f, 52.0f);
				UIBuilder::Flat(band, MenuArt::Band());
				ui.Label(band, "Key", 20.0f, 0.0f, 300.0f, 52.0f, keys[i][0], TextStyle::Body, TextAlignH::Left, MenuArt::Gold());
				ui.Label(band, "Action", 340.0f, 0.0f, width - 420.0f, 52.0f, keys[i][1], TextStyle::Body, TextAlignH::Left,
					MenuArt::Ink());
			}
			for (int s = 2; s < SectionCount; s++)
			{
				ui.Label(m_Sections[(size_t)s], "Empty", 32.0f, 300.0f, width - 64.0f, 60.0f, "Nothing to set here yet", TextStyle::Heading,
					TextAlignH::Center, MenuArt::Faint());
			}

			Entity detail = ui.DetailPanel(page);
			m_Letter = ui.Emblem(detail);
			m_Name = ui.Label(detail, "Name", 20.0f, 150.0f, UIBuilder::DetailWidth - 40.0f, 52.0f, std::string(), TextStyle::Heading,
				TextAlignH::Center, MenuArt::DarkInk());
			m_About = ui.Label(detail, "About", 30.0f, 220.0f, UIBuilder::DetailWidth - 60.0f, 300.0f, std::string(), TextStyle::Body,
				TextAlignH::Left, MenuArt::DarkInk(), true);
			SetSection(0);
		}

		void SetSection(int section)
		{
			m_Section = section < 0 || section >= SectionCount ? 0 : section;
			for (size_t s = 0; s < m_Sections.size(); s++)
			{
				UI::SetVisible(m_Sections[s], (int)s == m_Section);
				m_UI->SetNavSelected(m_SectionButtons[s], (int)s == m_Section);
			}
			UI::SetText(m_Letter, std::string(1, SectionName(m_Section)[0]));
			UI::SetText(m_Name, SectionName(m_Section));
			UI::SetText(m_About, SectionAbout(m_Section));
		}

		Entity GetSectionButton() const
		{
			return m_SectionButtons[(size_t)m_Section];
		}

		void Present(bool moisture, bool soil, bool pauseInMenu)
		{
			PresentToggle(m_MoistureToggle, moisture);
			PresentToggle(m_SoilToggle, soil);
			PresentToggle(m_PauseToggle, pauseInMenu);
		}

	private:
		static constexpr int SectionCount = 5;
		static constexpr float PanelHeight = 760.0f;
		static constexpr float RowsTop = 92.0f;

		static const char* SectionName(int section)
		{
			const char* names[SectionCount] = { "Gameplay", "Controls", "Audio", "Graphics", "Interface" };
			return names[section < 0 || section >= SectionCount ? 0 : section];
		}

		static const char* SectionAbout(int section)
		{
			const char* about[SectionCount] = {
				"How the world runs while you shape it. The overlays show where the land is wet and how rich its soil is; the "
				"menu can stop the clock while it is open.",
				"The keys and the mouse. They can't be changed yet.",
				"Music, ambience and interface sounds, once the game has them.",
				"Resolution, quality and effects, once there is more than one way to draw the island.",
				"The interface's size and readability options." };
			return about[section < 0 || section >= SectionCount ? 0 : section];
		}

		struct Toggle
		{
			Entity Button;
			Entity Label;
			int Shown = -1;
		};

		// A row with a label and an On / Off button.
		Toggle ToggleRow(Entity section, int index, const std::string& label, const std::string& event)
		{
			const float width = UIBuilder::ContentRight - UIBuilder::ContentX;
			const float y = RowsTop + (float)index * 72.0f;
			Entity band = m_UI->At(section, "Row " + label, 24.0f, y, width - 48.0f, 60.0f);
			UIBuilder::Flat(band, MenuArt::Band());
			m_UI->Label(band, "Label", 20.0f, 0.0f, width - 260.0f, 60.0f, label, TextStyle::Body, TextAlignH::Left, MenuArt::Ink());
			Entity button = m_UI->At(band, "Toggle", width - 48.0f - 164.0f, 6.0f, 148.0f, 48.0f);
			m_UI->Art(button, MenuArt::Sprite::ButtonDark);
			UIBuilder::Button(button, event);
			Toggle toggle;
			toggle.Button = button;
			toggle.Label = m_UI->Fill(button, "Label");
			m_UI->Text(toggle.Label, "Off", TextStyle::Body, TextAlignH::Center, MenuArt::Ink());
			return toggle;
		}

		void PresentToggle(Toggle& toggle, bool on) const
		{
			if (toggle.Shown == (int)on)
			{
				return;
			}
			toggle.Shown = (int)on;
			m_UI->Art(toggle.Button, on ? MenuArt::Sprite::ButtonBlue : MenuArt::Sprite::ButtonDark);
			UI::SetText(toggle.Label, on ? "On" : "Off");
		}

		const UIBuilder* m_UI = nullptr; // the menu view's, which owns this page
		std::vector<Entity> m_SectionButtons;
		std::vector<Entity> m_Sections;
		Toggle m_MoistureToggle;
		Toggle m_SoilToggle;
		Toggle m_PauseToggle;
		Entity m_Letter;
		Entity m_Name;
		Entity m_About;
		int m_Section = 0;
	};
}
