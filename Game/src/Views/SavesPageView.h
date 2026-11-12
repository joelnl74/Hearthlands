#pragma once

#include "Views/MenuArt.h"
#include "Views/UIBuilder.h"

#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/UI/UI.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace Hydro
{
	// The World Menu's Saved Worlds page: a row per slot (the quick save first) with what it holds and Save / Load buttons.
	// Buttons send "Save Slot <n>" and "Load Slot <n>"; a second press within a few seconds confirms (SetArmed).
	class SavesPageView
	{
	public:
		static constexpr int SlotCount = 7; // 0 is the quick save (F5 / F9)

		// A slot as the page shows it.
		struct Slot
		{
			bool Used = false;
			std::string Title; // the world's name
			std::string Line;  // year, season, settlements, when saved
		};

		void Bind(const UIBuilder& ui, Entity page)
		{
			m_UI = &ui;
			ui.PageHeader(page, "Saved Worlds", "Save and come back later");
			m_Notice = ui.Label(page, "Notice", RowX, UIBuilder::PageTop - 6.0f, RowWidth, 40.0f, std::string(), TextStyle::Body,
				TextAlignH::Center, MenuArt::Muted());
			for (int s = 0; s < SlotCount; s++)
			{
				const float y = UIBuilder::PageTop + 44.0f + (float)s * (RowHeight + RowGap);
				Entity row = ui.At(page, "Slot " + std::to_string(s), RowX, y, RowWidth, RowHeight);
				ui.Art(row, MenuArt::Sprite::Panel);
				Row shown;
				ui.Label(row, "Slot", 24.0f, 8.0f, 260.0f, 40.0f, s == 0 ? std::string("Quick save") : "Slot " + std::to_string(s),
					TextStyle::Body, TextAlignH::Left, MenuArt::Muted());
				shown.Title = ui.Label(row, "Title", 24.0f, 40.0f, 520.0f, 50.0f, std::string(), TextStyle::Heading, TextAlignH::Left,
					MenuArt::Gold());
				shown.Line = ui.Label(row, "Line", 300.0f, 8.0f, RowWidth - 300.0f - 2.0f * ButtonWidth - 72.0f, 40.0f, std::string(),
					TextStyle::Body, TextAlignH::Left, MenuArt::Ink());
				shown.Save = Button(row, "Save", "Save Slot " + std::to_string(s), RowWidth - 2.0f * ButtonWidth - 40.0f, shown.SaveLabel);
				shown.Load = Button(row, "Load", "Load Slot " + std::to_string(s), RowWidth - ButtonWidth - 24.0f, shown.LoadLabel);
				m_Rows.push_back(shown);
			}
			m_Shown.assign(SlotCount, Slot{});
			m_ShownUsed.assign(SlotCount, -1);
		}

		Entity GetFirstButton() const
		{
			return m_Rows.empty() ? Entity() : m_Rows.front().Save;
		}

		// What the slots hold (SlotCount, in order); only changed rows are redrawn.
		void Present(const std::vector<Slot>& slots)
		{
			for (int s = 0; s < SlotCount && s < (int)slots.size(); s++)
			{
				const Slot& slot = slots[(size_t)s];
				const bool same = m_ShownUsed[(size_t)s] == (slot.Used ? 1 : 0) && m_Shown[(size_t)s].Title == slot.Title
					&& m_Shown[(size_t)s].Line == slot.Line;
				if (same)
				{
					continue;
				}
				const Row& row = m_Rows[(size_t)s];
				UI::SetText(row.Title, slot.Used ? slot.Title : std::string("Empty"));
				UIBuilder::SetColor(row.Title, slot.Used ? MenuArt::Gold() : MenuArt::Faint());
				UI::SetText(row.Line, slot.Used ? slot.Line : std::string());
				UI::SetVisible(row.Load, slot.Used);
				m_Shown[(size_t)s] = slot;
				m_ShownUsed[(size_t)s] = slot.Used ? 1 : 0;
			}
		}

		// The button waiting for a second press ("Save Slot 2", "Load Slot 0"; empty: none) shows "Sure?".
		void SetArmed(const std::string& event)
		{
			for (int s = 0; s < (int)m_Rows.size(); s++)
			{
				const std::string slot = std::to_string(s);
				UI::SetText(m_Rows[(size_t)s].SaveLabel, event == "Save Slot " + slot ? std::string("Sure?") : std::string("Save"));
				UI::SetText(m_Rows[(size_t)s].LoadLabel, event == "Load Slot " + slot ? std::string("Sure?") : std::string("Load"));
			}
		}

		void SetNotice(const std::string& text, bool good)
		{
			UI::SetText(m_Notice, text);
			UIBuilder::SetColor(m_Notice, good ? MenuArt::Good() : MenuArt::Muted());
		}

	private:
		struct Row
		{
			Entity Title;
			Entity Line;
			Entity Save;
			Entity Load;
			Entity SaveLabel;
			Entity LoadLabel;
		};

		Entity Button(Entity row, const std::string& label, const std::string& event, float x, Entity& text)
		{
			Entity button = m_UI->At(row, label, x, (RowHeight - 64.0f) * 0.5f, ButtonWidth, 64.0f);
			m_UI->Art(button, label == "Save" ? MenuArt::Sprite::ButtonBlue : MenuArt::Sprite::ButtonDark);
			UIBuilder::Button(button, event);
			text = m_UI->Fill(button, "Label");
			m_UI->Text(text, label, TextStyle::Body, TextAlignH::Center, MenuArt::Ink());
			return button;
		}

		static constexpr float RowWidth = 1240.0f;
		static constexpr float RowX = (1920.0f - RowWidth) * 0.5f;
		static constexpr float RowHeight = 100.0f;
		static constexpr float RowGap = 10.0f;
		static constexpr float ButtonWidth = 180.0f;

		const UIBuilder* m_UI = nullptr;
		Entity m_Notice;
		std::vector<Row> m_Rows;
		std::vector<Slot> m_Shown;
		std::vector<int> m_ShownUsed;
	};
}
