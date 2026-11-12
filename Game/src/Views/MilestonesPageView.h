#pragma once

#include "Data/MilestoneDefinition.h"
#include "Views/UIBuilder.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// The Milestones page: cards by track, 12 a page (hidden ones ? cards until reached, after the intro), the chosen one on
	// parchment. Buttons send "Milestone <index>", "Track <n>", "Page Prev" / "Page Next" and "Open Technology".
	class MilestonesPageView
	{
	public:
		struct Line
		{
			std::string Name;
			std::string Description;
			std::string Unit;   // what it counts, after the numbers: "tiles of Grass", "trees", "species"
			std::string Hint;   // hidden: shown instead of the description until reached
			MilestoneKind Kind = MilestoneKind::Tiles;
			MilestoneTrack Track = MilestoneTrack::Auto;
			uint32_t Progress = 0;
			int Count = 1;
			int Points = 1;
			bool Reached = false;
			bool Hidden = false;
		};

		// lines: the milestones in table order (only their names, kinds and descriptions are read here).
		void Bind(const UIBuilder& ui, Entity page, const std::vector<Line>& lines)
		{
			m_UI = &ui;
			m_Ribbon = ui.PageHeader(page, "Milestones", "Reached 0 / 0");
			Entity nav = ui.NavPanel(page, "Tracks", TrackCount);
			for (int t = 0; t < TrackCount; t++)
			{
				Entity count;
				m_TrackButtons.push_back(ui.NavButton(nav, t, TrackName(t), "Track " + std::to_string(t), count));
				m_TrackCounts.push_back(count);
			}

			const float h = CardHeight;
			for (size_t i = 0; i < lines.size(); i++)
			{
				const Line& line = lines[i];
				Card card;
				card.Track = TrackOf(line);
				card.Hidden = line.Hidden;
				card.Button = ui.At(page, "Milestone " + line.Name, 0.0f, 0.0f, CardWidth, h);
				ui.Art(card.Button, MenuArt::Sprite::Tile);
				UIBuilder::Button(card.Button, "Milestone " + std::to_string(i));
				card.Picture = ui.At(card.Button, "Picture", 12.0f, 12.0f, CardWidth - 24.0f, std::max(h - 140.0f, 1.0f));
				ui.Art(card.Picture, MenuArt::Sprite::ArtSlot, TrackColor(card.Track));
				UI::SetVisible(card.Picture, h - 140.0f >= 24.0f);
				card.Name = ui.Label(card.Button, "Name", 8.0f, h - 120.0f, CardWidth - 16.0f, 44.0f, line.Name, TextStyle::Body,
					TextAlignH::Center, MenuArt::Ink());
				Entity track = ui.At(card.Button, "Bar", 20.0f, h - 66.0f, BarWidth, 16.0f);
				ui.Art(track, MenuArt::Sprite::BarTrack);
				card.Fill = ui.At(track, "Fill", 0.0f, 0.0f, BarWidth, 16.0f);
				ui.Art(card.Fill, MenuArt::Sprite::BarFill);
				card.Progress = ui.Label(card.Button, "Progress", 8.0f, h - 48.0f, CardWidth - 16.0f, 40.0f, std::string(), TextStyle::Body,
					TextAlignH::Center, MenuArt::Muted());
				card.Check = ui.At(card.Button, "Check", CardWidth - 34.0f, -6.0f, 40.0f, 40.0f);
				ui.Art(card.Check, MenuArt::Sprite::Check);
				card.Lock = ui.At(card.Picture, "Lock", (CardWidth - 24.0f) * 0.5f - 24.0f, (h - 140.0f) * 0.5f - 24.0f, 48.0f, 48.0f);
				ui.Art(card.Lock, MenuArt::Sprite::Lock);
				m_Cards.push_back(card);
			}
			// The open ones first (the intro), then the hidden ones, each in table order.
			for (int pass = 0; pass < 2; pass++)
			{
				for (size_t i = 0; i < m_Cards.size(); i++)
				{
					if (m_Cards[i].Hidden == (pass == 1))
					{
						m_Order.push_back(i);
					}
				}
			}
			const float pagerY = UIBuilder::PageTop + (float)Rows * (CardHeight + Gap);
			const float middle = (UIBuilder::ContentX + UIBuilder::ContentRight) * 0.5f;
			m_Prev = ui.At(page, "Page Prev", middle - 230.0f, pagerY, 96.0f, 64.0f);
			ui.Art(m_Prev, MenuArt::Sprite::ButtonDark);
			UIBuilder::Button(m_Prev, "Page Prev");
			ui.Label(m_Prev, "Label", 0.0f, 0.0f, 96.0f, 64.0f, "<", TextStyle::Heading, TextAlignH::Center, MenuArt::Ink());
			m_PageText = ui.Label(page, "Page", middle - 120.0f, pagerY, 240.0f, 64.0f, std::string(), TextStyle::Body, TextAlignH::Center,
				MenuArt::Muted());
			m_Next = ui.At(page, "Page Next", middle + 134.0f, pagerY, 96.0f, 64.0f);
			ui.Art(m_Next, MenuArt::Sprite::ButtonDark);
			UIBuilder::Button(m_Next, "Page Next");
			ui.Label(m_Next, "Label", 0.0f, 0.0f, 96.0f, 64.0f, ">", TextStyle::Heading, TextAlignH::Center, MenuArt::Ink());
			m_SelectFrame = ui.At(page, "Selected", 0.0f, 0.0f, CardWidth + 24.0f, h + 24.0f);
			ui.Art(m_SelectFrame, MenuArt::Sprite::Select);

			Entity panel = ui.DetailPanel(page);
			m_Letter = ui.Emblem(panel);
			m_Name = ui.Label(panel, "Name", 20.0f, 150.0f, UIBuilder::DetailWidth - 40.0f, 52.0f, std::string(), TextStyle::Heading,
				TextAlignH::Center, MenuArt::DarkInk());
			m_TrackChip = ui.Chip(panel, "Track", 50.0f, 210.0f, 200.0f, MenuArt::Sprite::ButtonBlue, MenuArt::Ink());
			m_RewardChip = ui.Chip(panel, "Reward", 266.0f, 210.0f, 144.0f, MenuArt::Sprite::ParchmentHeader, MenuArt::DarkInk());
			m_Description = ui.Label(panel, "Description", 30.0f, 264.0f, UIBuilder::DetailWidth - 60.0f, 90.0f, std::string(),
				TextStyle::Body, TextAlignH::Left, MenuArt::DarkInk(), true);
			ui.Section(panel, "Progress Band", 360.0f, "Progress");
			m_ProgressText = ui.Label(panel, "Progress", 36.0f, 406.0f, UIBuilder::DetailWidth - 72.0f, 44.0f, std::string(),
				TextStyle::Body, TextAlignH::Left, MenuArt::DarkInk());
			Entity track = ui.At(panel, "Bar", 36.0f, 454.0f, DetailBarWidth, 18.0f);
			ui.Art(track, MenuArt::Sprite::BarTrack);
			m_DetailFill = ui.At(track, "Fill", 0.0f, 0.0f, DetailBarWidth, 18.0f);
			ui.Art(m_DetailFill, MenuArt::Sprite::BarFillGold);
			ui.Section(panel, "Reward Band", 488.0f, "Reward");
			m_Reward = ui.Label(panel, "Reward", 36.0f, 534.0f, UIBuilder::DetailWidth - 72.0f, 44.0f, std::string(), TextStyle::Body,
				TextAlignH::Left, MenuArt::DarkInk());
			m_Status = ui.Label(panel, "Status", 20.0f, 594.0f, UIBuilder::DetailWidth - 40.0f, 44.0f, std::string(), TextStyle::Heading,
				TextAlignH::Center, MenuArt::DarkMuted());
			m_Spend = ui.Element(panel, "Spend", glm::vec2(0.5f, 0.0f), glm::vec2(0.0f, 24.0f), glm::vec2(380.0f, 76.0f));
			ui.Art(m_Spend, MenuArt::Sprite::ButtonPurple);
			UIBuilder::Button(m_Spend, "Open Technology");
			ui.Text(ui.Fill(m_Spend, "Label", glm::vec4(0.0f, 0.0f, 0.0f, 4.0f)), "Spend points", TextStyle::Heading, TextAlignH::Center,
				MenuArt::Ink());

			SetTrack(0);
		}

		void SetTrack(int track)
		{
			m_Track = std::clamp(track, 0, TrackCount - 1);
			m_Page = 0;
			Layout(true);
		}

		// step: -1 the page before, 1 the next.
		void TurnPage(int step)
		{
			m_Page = std::clamp(m_Page + step, 0, std::max(PageCount() - 1, 0));
			Layout(true);
		}

		void Select(size_t index)
		{
			if (index < m_Cards.size())
			{
				m_Selected = index;
				UIBuilder::Move(m_SelectFrame, m_Cards[index].X - 12.0f, m_Cards[index].Y - 12.0f);
				UI::SetVisible(m_SelectFrame, UI::IsVisible(m_Cards[index].Button));
				m_ShownDetail.clear();
			}
		}

		Entity GetSelectedButton() const
		{
			return m_Selected < m_Cards.size() ? m_Cards[m_Selected].Button : Entity();
		}

		// lines: per milestone in the order bound; points: unspent talent points.
		void Present(const std::vector<Line>& lines, int points)
		{
			int reached = 0;
			int perTrackReached[TrackCount] = {};
			int perTrack[TrackCount] = {};
			for (size_t i = 0; i < m_Cards.size() && i < lines.size(); i++)
			{
				const Line& line = lines[i];
				const int track = m_Cards[i].Track;
				perTrack[0]++;
				perTrack[track]++;
				if (line.Reached)
				{
					reached++;
					perTrackReached[0]++;
					perTrackReached[track]++;
				}
				PresentCard(m_Cards[i], line);
			}
			const std::string ribbon = "Reached " + std::to_string(reached) + " / " + std::to_string(lines.size());
			if (ribbon != m_ShownRibbon)
			{
				m_ShownRibbon = ribbon;
				UI::SetText(m_Ribbon, ribbon);
				for (int t = 0; t < TrackCount && t < (int)m_TrackCounts.size(); t++)
				{
					UI::SetText(m_TrackCounts[(size_t)t], std::to_string(perTrackReached[t]) + " / " + std::to_string(perTrack[t]));
				}
			}
			if (m_Selected < lines.size())
			{
				PresentDetail(lines[m_Selected], m_Cards[m_Selected].Track, points);
			}
		}

	private:
		struct Card
		{
			int Track = 0;
			bool Hidden = false;
			Entity Button;
			Entity Picture;
			Entity Name;
			Entity Fill;
			Entity Progress;
			Entity Check;
			Entity Lock;
			float X = 0.0f;
			float Y = 0.0f;
			std::string Shown;
		};

		static constexpr int TrackCount = 6;
		static constexpr int Columns = 4;
		static constexpr int Rows = 3;
		static constexpr int PerPage = Columns * Rows;
		static constexpr float Gap = 16.0f;
		static constexpr float CardWidth = (UIBuilder::ContentRight - UIBuilder::ContentX - (float)(Columns - 1) * Gap) / (float)Columns;
		static constexpr float CardHeight = 244.0f;
		static constexpr float BarWidth = CardWidth - 40.0f;
		static constexpr float DetailBarWidth = UIBuilder::DetailWidth - 72.0f;

		static const char* TrackName(int track)
		{
			const char* names[TrackCount] = { "All", "Land", "Water", "Woods", "Wildlife", "Peoples" };
			return names[std::clamp(track, 0, TrackCount - 1)];
		}

		// The line's Track, else by its kind.
		static int TrackOf(const Line& line)
		{
			if (line.Track != MilestoneTrack::Auto)
			{
				return std::clamp((int)line.Track, 1, TrackCount - 1);
			}
			switch (line.Kind)
			{
			case MilestoneKind::Tiles: return 1;
			case MilestoneKind::Level: return 1;
			case MilestoneKind::RichSoil: return 1;
			case MilestoneKind::Springs: return 2;
			case MilestoneKind::Habitat: return 2;
			case MilestoneKind::Objects: return 3;
			case MilestoneKind::Ancient: return 3;
			case MilestoneKind::Settlers: return 5;
			case MilestoneKind::SettlementLevel: return 5;
			default: return 4;
			}
		}

		static glm::vec4 TrackColor(int track)
		{
			const glm::vec4 colors[TrackCount] = { glm::vec4(1.0f), glm::vec4(0.95f, 0.80f, 0.55f, 1.0f),
				glm::vec4(0.55f, 0.80f, 1.0f, 1.0f), glm::vec4(0.55f, 0.92f, 0.50f, 1.0f), glm::vec4(0.95f, 0.70f, 0.50f, 1.0f),
				glm::vec4(0.90f, 0.65f, 0.95f, 1.0f) };
			return colors[std::clamp(track, 0, TrackCount - 1)];
		}

		static bool Unknown(const Line& line)
		{
			return line.Hidden && line.Reached == false;
		}

		bool InTrack(size_t card) const
		{
			return m_Track == 0 || m_Cards[card].Track == m_Track;
		}

		int PageCount() const
		{
			int shown = 0;
			for (size_t i = 0; i < m_Cards.size(); i++)
			{
				shown += InTrack(i) ? 1 : 0;
			}
			return std::max((shown + PerPage - 1) / PerPage, 1);
		}

		// Shows the track's cards of this page in a grid, the pager when there is more than one, and keeps a shown card chosen.
		void Layout(bool keepSelected)
		{
			const int pages = PageCount();
			m_Page = std::clamp(m_Page, 0, pages - 1);
			int index = 0;
			bool selectedShown = false;
			size_t first = m_Cards.size();
			for (size_t card : m_Order)
			{
				const bool inTrack = InTrack(card);
				const int slot = index - m_Page * PerPage;
				const bool show = inTrack && slot >= 0 && slot < PerPage;
				index += inTrack ? 1 : 0;
				UI::SetVisible(m_Cards[card].Button, show);
				if (show == false)
				{
					continue;
				}
				m_Cards[card].X = UIBuilder::ContentX + (float)(slot % Columns) * (CardWidth + Gap);
				m_Cards[card].Y = UIBuilder::PageTop + (float)(slot / Columns) * (CardHeight + Gap);
				UIBuilder::Move(m_Cards[card].Button, m_Cards[card].X, m_Cards[card].Y);
				selectedShown = selectedShown || card == m_Selected;
				first = first < m_Cards.size() ? first : card;
			}
			for (size_t t = 0; t < m_TrackButtons.size(); t++)
			{
				m_UI->SetNavSelected(m_TrackButtons[t], (int)t == m_Track);
			}
			UI::SetVisible(m_Prev, pages > 1);
			UI::SetVisible(m_Next, pages > 1);
			UI::SetVisible(m_PageText, pages > 1);
			UI::SetText(m_PageText, "Page " + std::to_string(m_Page + 1) + " / " + std::to_string(pages));
			Select(selectedShown && keepSelected ? m_Selected : (first < m_Cards.size() ? first : m_Selected));
		}

		static float Share(const Line& line)
		{
			if (line.Reached)
			{
				return 1.0f;
			}
			return line.Count > 0 ? std::min((float)line.Progress / (float)line.Count, 1.0f) : 0.0f;
		}

		static std::string Numbers(const Line& line)
		{
			const uint32_t shown = line.Reached ? (uint32_t)line.Count : std::min(line.Progress, (uint32_t)line.Count);
			return std::to_string(shown) + " / " + std::to_string(line.Count);
		}

		void PresentCard(Card& card, const Line& line)
		{
			const std::string key = std::to_string(line.Progress) + (line.Reached ? "r" : "o");
			if (key == card.Shown)
			{
				return;
			}
			card.Shown = key;
			const bool unknown = Unknown(line);
			UI::SetVisible(card.Lock, unknown && UI::IsVisible(card.Picture));
			UI::SetText(card.Name, unknown ? std::string("?") : line.Name);
			if (unknown)
			{
				// A discovery not made yet: no name, no numbers (the count would give it away).
				UI::SetVisible(card.Fill, false);
				UI::SetText(card.Progress, "Undiscovered");
				UIBuilder::SetColor(card.Progress, MenuArt::Muted());
				UIBuilder::SetColor(card.Name, MenuArt::Muted());
				UI::SetVisible(card.Check, false);
				m_UI->GetArt().Apply(card.Picture.GetComponent<UIImageComponent>(), MenuArt::Sprite::ArtSlot,
					glm::vec4(0.32f, 0.30f, 0.42f, 1.0f));
				return;
			}
			const float share = Share(line);
			UI::SetVisible(card.Fill, share > 0.0f);
			UIBuilder::Resize(card.Fill, std::max(BarWidth * share, 16.0f), 16.0f);
			m_UI->GetArt().Apply(card.Fill.GetComponent<UIImageComponent>(), line.Reached ? MenuArt::Sprite::BarFillGold
				: MenuArt::Sprite::BarFill);
			UI::SetText(card.Progress, line.Reached ? std::string("Reached") : Numbers(line));
			UIBuilder::SetColor(card.Progress, line.Reached ? MenuArt::Good() : MenuArt::Muted());
			UIBuilder::SetColor(card.Name, line.Reached ? MenuArt::Gold() : MenuArt::Ink());
			UI::SetVisible(card.Check, line.Reached);
			const glm::vec4 tint = TrackColor(card.Track) * (line.Reached ? glm::vec4(1.0f) : glm::vec4(0.55f, 0.55f, 0.6f, 1.0f));
			m_UI->GetArt().Apply(card.Picture.GetComponent<UIImageComponent>(), MenuArt::Sprite::ArtSlot, tint);
		}

		void PresentDetail(const Line& line, int track, int points)
		{
			const std::string key = std::to_string(m_Selected) + "/" + std::to_string(line.Progress) + (line.Reached ? "r" : "o") + "/"
				+ std::to_string(points);
			if (key == m_ShownDetail)
			{
				return;
			}
			m_ShownDetail = key;
			const bool unknown = Unknown(line);
			UI::SetText(m_Letter, unknown ? std::string("?") : std::string(1, TrackName(track)[0]));
			UI::SetText(m_Name, unknown ? std::string("Undiscovered") : line.Name);
			UI::SetText(m_TrackChip, TrackName(track));
			UI::SetText(m_RewardChip, "+" + std::to_string(line.Points) + (line.Points == 1 ? " point" : " points"));
			const std::string riddle = line.Hint.empty() ? std::string("A hidden discovery: keep shaping the land and water to find it.")
				: line.Hint;
			UI::SetText(m_Description, unknown ? riddle : line.Description);
			UI::SetText(m_ProgressText, unknown ? std::string("?") : Numbers(line) + " " + line.Unit);
			const float share = unknown ? 0.0f : Share(line);
			UI::SetVisible(m_DetailFill, share > 0.0f);
			UIBuilder::Resize(m_DetailFill, std::max(DetailBarWidth * share, 16.0f), 18.0f);
			UI::SetText(m_Reward, "+" + std::to_string(line.Points) + (line.Points == 1 ? " talent point" : " talent points"));
			UI::SetText(m_Status, line.Reached ? "Reached" : (unknown ? "Hidden" : "In progress"));
			UIBuilder::SetColor(m_Status, line.Reached ? MenuArt::DarkGood() : MenuArt::DarkMuted());
			UI::SetVisible(m_Spend, points > 0);
		}

		const UIBuilder* m_UI = nullptr; // the menu view's, which owns this page
		std::vector<Card> m_Cards;
		std::vector<Entity> m_TrackButtons;
		std::vector<Entity> m_TrackCounts;
		Entity m_Ribbon;
		Entity m_SelectFrame;
		Entity m_Letter;
		Entity m_Name;
		Entity m_TrackChip;
		Entity m_RewardChip;
		Entity m_Description;
		Entity m_ProgressText;
		Entity m_DetailFill;
		Entity m_Reward;
		Entity m_Status;
		Entity m_Spend;
		std::vector<size_t> m_Order; // the cards in the order they are laid out
		Entity m_Prev;
		Entity m_Next;
		Entity m_PageText;
		int m_Track = 0;
		int m_Page = 0;
		size_t m_Selected = 0;
		std::string m_ShownRibbon;
		std::string m_ShownDetail;
	};
}
