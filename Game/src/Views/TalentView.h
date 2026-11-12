#pragma once

#include "Data/MilestoneDefinition.h"
#include "Data/TalentDefinition.h"
#include "Rules/TalentRules.h"
#include "State/TalentState.h"

#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/UI/UI.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// The talent panel on the HUD canvas: a button (points to spend) opens four branch columns of talent cards and the
	// milestones with their progress. Buttons send "Talents", "Talents Close" and "Talent <id>" to the root's scripts.
	class TalentView
	{
	public:
		struct MilestoneLine
		{
			std::string Name;
			std::string Description;
			uint32_t Progress = 0;
			int Count = 1;
			int Points = 1;
			bool Reached = false;
		};

		void Bind(Entity root, AssetHandle font, const std::vector<TalentDefinition>& talents, size_t milestones)
		{
			m_Root = root;
			m_Font = font;
			m_Toggle = Element(root, "Talents Button", glm::vec2(1.0f, 1.0f), glm::vec2(-16.0f, -100.0f), glm::vec2(240.0f, 56.0f));
			Image(m_Toggle, FrameColor());
			ButtonEvent(m_Toggle, "Talents");
			m_ToggleText = Element(m_Toggle, "Label", glm::vec2(0.5f, 0.5f), glm::vec2(0.0f), glm::vec2(230.0f, 48.0f));
			Text(m_ToggleText, "Talents (T)", TextAlignH::Center, false);

			m_Panel = Element(root, "Talent Panel", glm::vec2(0.5f, 0.5f), glm::vec2(0.0f, 20.0f), glm::vec2(PanelWidth, PanelHeight));
			Image(m_Panel, glm::vec4(0.06f, 0.07f, 0.09f, 0.94f));
			m_Title = Element(m_Panel, "Title", glm::vec2(0.0f, 1.0f), glm::vec2(Margin, -16.0f), glm::vec2(1200.0f, 48.0f));
			Text(m_Title, "Talents", TextAlignH::Left, false).Color = SelectedColor();
			Entity close = Element(m_Panel, "Close", glm::vec2(1.0f, 1.0f), glm::vec2(-16.0f, -16.0f), glm::vec2(56.0f, 48.0f));
			Image(close, FrameColor());
			ButtonEvent(close, "Talents Close");
			Text(Element(close, "Label", glm::vec2(0.5f, 0.5f), glm::vec2(0.0f), glm::vec2(48.0f, 40.0f)), "X", TextAlignH::Center, false);

			const char* branches[4] = { "Water", "Land", "Life", "Insight" };
			for (int i = 0; i < 4; i++)
			{
				Entity header = Element(m_Panel, std::string("Branch ") + branches[i], glm::vec2(0.0f, 1.0f),
					glm::vec2(Margin + (float)i * ColumnStep, -76.0f), glm::vec2(CardWidth, 40.0f));
				Text(header, branches[i], TextAlignH::Left, false).Color = glm::vec4(0.75f, 0.85f, 1.0f, 1.0f);
			}
			for (const TalentDefinition& talent : talents)
			{
				Card card;
				card.TalentId = talent.TalentId;
				const float x = Margin + (float)(int)talent.Branch * ColumnStep;
				const float y = -124.0f - (float)(talent.Row - 1) * (CardHeight + 12.0f);
				card.Button = Element(m_Panel, "Talent " + talent.Name, glm::vec2(0.0f, 1.0f), glm::vec2(x, y),
					glm::vec2(CardWidth, CardHeight));
				Image(card.Button, FrameColor());
				ButtonEvent(card.Button, "Talent " + std::to_string(talent.TalentId));
				card.Name = Element(card.Button, "Name", glm::vec2(0.0f, 1.0f), glm::vec2(10.0f, -4.0f),
					glm::vec2(CardWidth - 20.0f, 36.0f));
				Text(card.Name, talent.Name + "   " + std::to_string(talent.Cost), TextAlignH::Left, false);
				card.Description = Element(card.Button, "Description", glm::vec2(0.0f, 1.0f), glm::vec2(10.0f, -40.0f),
					glm::vec2(CardWidth - 20.0f, 76.0f));
				UITextComponent& text = Text(card.Description, talent.Description, TextAlignH::Left, true);
				text.AlignV = TextAlignV::Top;
				m_Cards.push_back(card);
			}

			Entity heading = Element(m_Panel, "Milestones", glm::vec2(0.0f, 1.0f), glm::vec2(MilestonesX, -76.0f),
				glm::vec2(MilestonesWidth, 40.0f));
			Text(heading, "Milestones (points)", TextAlignH::Left, false).Color = glm::vec4(0.75f, 0.85f, 1.0f, 1.0f);
			for (size_t i = 0; i < milestones; i++)
			{
				Entity line = Element(m_Panel, "Milestone " + std::to_string(i), glm::vec2(0.0f, 1.0f),
					glm::vec2(MilestonesX, -124.0f - (float)i * 56.0f), glm::vec2(MilestonesWidth, 54.0f));
				UITextComponent& text = Text(line, "", TextAlignH::Left, true);
				text.AlignV = TextAlignV::Top;
				text.LineSpacing = 0.8f;
				m_Lines.push_back(line);
			}
			UI::SetVisible(m_Panel, false);
		}

		bool IsOpen() const
		{
			return m_Open;
		}

		void SetOpen(bool open)
		{
			m_Open = open;
			UI::SetVisible(m_Panel, open);
		}

		// verdicts: per talent in the order bound (TalentRules::Check); lines: per milestone in the order bound.
		void Present(const TalentState& state, const std::vector<TalentRules::Verdict>& verdicts, const std::vector<MilestoneLine>& lines)
		{
			if (state.Points != m_ShownPoints)
			{
				UI::SetText(m_ToggleText, state.Points > 0 ? "Talents: " + std::to_string(state.Points) + " (T)" : "Talents (T)");
				m_Toggle.GetComponent<UIImageComponent>().Color = state.Points > 0 ? glm::vec4(0.45f, 0.36f, 0.12f, 1.0f) : FrameColor();
				UI::SetText(m_Title, "Talents: " + std::to_string(state.Points) + (state.Points == 1 ? " point" : " points")
					+ " to spend  (" + std::to_string(state.Earned) + " earned)");
				m_ShownPoints = state.Points;
			}
			if (m_Open == false)
			{
				return;
			}
			for (size_t i = 0; i < m_Cards.size() && i < verdicts.size(); i++)
			{
				const TalentRules::Verdict verdict = verdicts[i];
				const bool owned = verdict == TalentRules::Verdict::Owned;
				const bool buyable = verdict == TalentRules::Verdict::Buyable;
				m_Cards[i].Button.GetComponent<UIImageComponent>().Color = owned ? glm::vec4(0.42f, 0.34f, 0.12f, 1.0f)
					: (buyable ? glm::vec4(0.20f, 0.26f, 0.30f, 1.0f) : glm::vec4(0.11f, 0.12f, 0.14f, 1.0f));
				const glm::vec4 ink = owned || buyable ? glm::vec4(1.0f) : glm::vec4(0.55f, 0.55f, 0.58f, 1.0f);
				m_Cards[i].Name.GetComponent<UITextComponent>().Color = buyable ? SelectedColor() : ink;
				m_Cards[i].Description.GetComponent<UITextComponent>().Color = ink * glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
			}
			for (size_t i = 0; i < m_Lines.size() && i < lines.size(); i++)
			{
				const MilestoneLine& line = lines[i];
				const uint32_t shown = line.Progress < (uint32_t)line.Count ? line.Progress : (uint32_t)line.Count;
				const std::string head = (line.Reached ? "[x] " : "[  ] ") + line.Name + "  +" + std::to_string(line.Points)
					+ (line.Reached ? std::string() : "   " + std::to_string(shown) + " / " + std::to_string(line.Count));
				const std::string text = head + "\n" + line.Description;
				if (i >= m_ShownLines.size() || m_ShownLines[i] != text)
				{
					m_ShownLines.resize(m_Lines.size());
					m_ShownLines[i] = text;
					UI::SetText(m_Lines[i], text);
					m_Lines[i].GetComponent<UITextComponent>().Color = line.Reached ? glm::vec4(0.6f, 0.62f, 0.66f, 1.0f) : glm::vec4(1.0f);
				}
			}
		}

		// Whether the pointer is over the button or the open panel, as of the last layout.
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
			return Contains(m_Toggle, point) || (m_Open && Contains(m_Panel, point));
		}

	private:
		struct Card
		{
			int TalentId = 0;
			Entity Button;
			Entity Name;
			Entity Description;
		};

		static constexpr float Margin = 32.0f;
		static constexpr float CardWidth = 300.0f;
		static constexpr float CardHeight = 124.0f;
		static constexpr float ColumnStep = 316.0f;
		static constexpr float MilestonesX = Margin + 4.0f * ColumnStep + 16.0f;
		static constexpr float MilestonesWidth = 500.0f;
		static constexpr float PanelWidth = MilestonesX + MilestonesWidth + Margin;
		static constexpr float PanelHeight = 900.0f;

		static glm::vec4 FrameColor()
		{
			return glm::vec4(0.20f, 0.21f, 0.24f, 1.0f);
		}

		static glm::vec4 SelectedColor()
		{
			return glm::vec4(0.95f, 0.78f, 0.30f, 1.0f);
		}

		static bool Contains(Entity element, const glm::vec2& point)
		{
			const UIResolvedRectComponent* rect = element.IsValid() ? element.TryGetComponent<UIResolvedRectComponent>() : nullptr;
			return rect != nullptr && rect->Visible && point.x >= rect->Min.x && point.y >= rect->Min.y && point.x <= rect->Max.x
				&& point.y <= rect->Max.y;
		}

		// A child element anchored at one point of its parent (the pivot there too), placed at `position` from it.
		Entity Element(Entity parent, const std::string& name, const glm::vec2& anchor, const glm::vec2& position, const glm::vec2& size)
		{
			Scene& scene = *m_Root.GetScene();
			Entity element = scene.CreateEntity(name);
			scene.SetParent(element, parent, false);
			UIRectComponent& rect = element.AddComponent<UIRectComponent>();
			rect.AnchorMin = anchor;
			rect.AnchorMax = anchor;
			rect.Pivot = anchor;
			rect.AnchoredPosition = position;
			rect.SizeDelta = size;
			return element;
		}

		static UIImageComponent& Image(Entity element, const glm::vec4& color)
		{
			UIImageComponent& image = element.AddComponent<UIImageComponent>();
			image.Color = color;
			return image;
		}

		static void ButtonEvent(Entity element, const std::string& event)
		{
			UIButtonComponent& button = element.AddComponent<UIButtonComponent>();
			button.Event = event;
			button.Navigation = UINavigation::None; // WASD moves the camera, not the focus
		}

		UITextComponent& Text(Entity element, const std::string& text, TextAlignH align, bool wrap)
		{
			UITextComponent& component = element.AddComponent<UITextComponent>();
			component.Text = text;
			component.Font = m_Font;
			component.AlignH = align;
			component.AlignV = TextAlignV::Middle;
			component.Wrap = wrap;
			component.ShadowColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.6f);
			return component;
		}

		Entity m_Root;
		AssetHandle m_Font = 0;
		Entity m_Toggle;
		Entity m_ToggleText;
		Entity m_Panel;
		Entity m_Title;
		std::vector<Card> m_Cards;
		std::vector<Entity> m_Lines;
		std::vector<std::string> m_ShownLines;
		bool m_Open = false;
		int m_ShownPoints = -1;
	};
}
