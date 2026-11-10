#pragma once

#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/UI/UI.h"
#include "Hydro/Scene/UI/UILayout.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace Hydro
{
	// The world's HUD, built in code on the root (made a 1920 x 1080 canvas): brush bar (unlocked types, size), clock (day,
	// speeds), controls line. Buttons send "Brush <id>", "Size -", "Size +", "Speed <n>" to the root's scripts.
	class HudView
	{
	public:
		struct BrushChoice
		{
			uint8_t TileId = 0;
			std::string Name;
			glm::vec4 Color{ 1.0f };
		};

		static constexpr int Speeds[4] = { 0, 1, 3, 10 };

		void Bind(Entity root, AssetHandle font)
		{
			m_Root = root;
			m_Font = font;
			UICanvasComponent& canvas = root.HasComponent<UICanvasComponent>() ? root.GetComponent<UICanvasComponent>()
				: root.AddComponent<UICanvasComponent>();
			canvas.ReferenceResolution = glm::vec2(1920.0f, 1080.0f);
			canvas.ScaleMode = UIScaleMode::ScaleWithScreen;
			canvas.SortOrder = 10;

			m_BrushBar = Element(root, "Brush Bar", glm::vec2(0.5f, 0.0f), glm::vec2(0.5f, 16.0f), glm::vec2(400.0f, BarHeight));
			Image(m_BrushBar, PanelColor());
			m_SizeDown = Button(m_BrushBar, "Size Down", "Size -", "-");
			m_SizeLabel = Element(m_BrushBar, "Size Label", glm::vec2(0.0f, 0.5f), glm::vec2(0.0f), glm::vec2(SizeLabelWidth, 112.0f));
			Text(m_SizeLabel, "Size", TextAlignH::Center);
			m_SizeUp = Button(m_BrushBar, "Size Up", "Size +", "+");

			m_ClockBar = Element(root, "Clock Bar", glm::vec2(1.0f, 1.0f), glm::vec2(-16.0f, -16.0f), glm::vec2(ClockBarWidth, 72.0f));
			Image(m_ClockBar, PanelColor());
			m_DayLabel = Element(m_ClockBar, "Day", glm::vec2(0.0f, 0.5f), glm::vec2(Padding, 0.0f), glm::vec2(DayWidth, 56.0f));
			Text(m_DayLabel, "Day 1", TextAlignH::Left);
			const char* speedLabels[4] = { "II", "1x", "3x", "10x" };
			for (int i = 0; i < 4; i++)
			{
				m_SpeedButtons[i] = Button(m_ClockBar, std::string("Speed ") + speedLabels[i], "Speed " + std::to_string(Speeds[i]),
					speedLabels[i]);
				Place(m_SpeedButtons[i], glm::vec2(Padding + DayWidth + 8.0f + (float)i * (SpeedWidth + Gap), 0.0f),
					glm::vec2(SpeedWidth, 56.0f));
			}

			Entity help = Element(root, "Controls", glm::vec2(0.0f, 1.0f), glm::vec2(16.0f, -16.0f), glm::vec2(1300.0f, 40.0f));
			Text(help, "Left drag: paint    Right drag / WASD: move    Q / E: zoom    1-9: brush    [ ]: size    P: pause    M: moisture",
				TextAlignH::Left).Color = glm::vec4(1.0f, 1.0f, 1.0f, 0.8f);
			m_Notice = Element(root, "Notice", glm::vec2(0.5f, 1.0f), glm::vec2(0.0f, -104.0f), glm::vec2(640.0f, 64.0f));
			Image(m_Notice, PanelColor());
			Entity noticeText = Element(m_Notice, "Notice Text", glm::vec2(0.5f, 0.5f), glm::vec2(0.0f), glm::vec2(620.0f, 56.0f));
			Text(noticeText, "", TextAlignH::Center).Color = SelectedColor();
			m_NoticeText = noticeText;
			UI::SetVisible(m_Notice, false);
			LayoutBrushBar();
		}

		// A message in the middle at the top for `seconds` (Present counts it down); a new one replaces it.
		void ShowNotice(const std::string& text, float seconds)
		{
			UI::SetText(m_NoticeText, text);
			UI::SetVisible(m_Notice, true);
			m_NoticeSeconds = seconds;
		}

		// Rebuilds the brush buttons when the choices changed (a new unlock).
		void SetBrushes(const std::vector<BrushChoice>& choices)
		{
			if (SameChoices(choices))
			{
				return;
			}
			Scene& scene = *m_Root.GetScene();
			for (const Entity& button : m_BrushButtons)
			{
				scene.DestroyEntity(button);
			}
			m_BrushButtons.clear();
			m_Choices = choices;
			for (size_t i = 0; i < choices.size(); i++)
			{
				const BrushChoice& choice = choices[i];
				Entity button = Button(m_BrushBar, "Brush " + choice.Name, "Brush " + std::to_string((int)choice.TileId), "");
				Entity swatch = Element(button, "Swatch", glm::vec2(0.5f, 1.0f), glm::vec2(0.0f, -8.0f), glm::vec2(64.0f, 64.0f));
				Image(swatch, choice.Color);
				Entity key = Element(button, "Key", glm::vec2(0.0f, 1.0f), glm::vec2(6.0f, -2.0f), glm::vec2(30.0f, 32.0f));
				Text(key, i < 9 ? std::to_string(i + 1) : std::string(), TextAlignH::Left);
				Entity name = Element(button, "Name", glm::vec2(0.5f, 0.0f), glm::vec2(0.0f, 4.0f), glm::vec2(BrushWidth, 34.0f));
				Text(name, choice.Name, TextAlignH::Center);
				m_BrushButtons.push_back(button);
			}
			LayoutBrushBar();
			m_ShownTile = -1;
		}

		void Present(uint8_t brushTile, float radius, int speed, int day, float deltaTime)
		{
			if (m_NoticeSeconds > 0.0f)
			{
				m_NoticeSeconds -= deltaTime;
				if (m_NoticeSeconds <= 0.0f)
				{
					UI::SetVisible(m_Notice, false);
				}
			}
			if ((int)brushTile != m_ShownTile)
			{
				for (size_t i = 0; i < m_BrushButtons.size(); i++)
				{
					SetSelected(m_BrushButtons[i], m_Choices[i].TileId == brushTile);
				}
				m_ShownTile = brushTile;
			}
			if (radius != m_ShownRadius)
			{
				char text[32];
				std::snprintf(text, sizeof(text), "Size\n%.1f", radius);
				UI::SetText(m_SizeLabel, text);
				m_ShownRadius = radius;
			}
			if (speed != m_ShownSpeed)
			{
				for (int i = 0; i < 4; i++)
				{
					SetSelected(m_SpeedButtons[i], Speeds[i] == speed);
				}
				m_ShownSpeed = speed;
			}
			if (day != m_ShownDay)
			{
				UI::SetText(m_DayLabel, "Day " + std::to_string(day));
				m_ShownDay = day;
			}
		}

		// Whether the pointer (Scene::GetUIPointer) is over one of the bars, as of the last layout.
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
			return Contains(m_BrushBar, point) || Contains(m_ClockBar, point);
		}

	private:
		static constexpr float BarHeight = 132.0f;
		static constexpr float BrushWidth = 96.0f;
		static constexpr float SizeButtonWidth = 48.0f;
		static constexpr float SizeLabelWidth = 110.0f;
		static constexpr float SpeedWidth = 72.0f;
		static constexpr float DayWidth = 150.0f;
		static constexpr float Padding = 12.0f;
		static constexpr float Gap = 8.0f;
		static constexpr float ClockBarWidth = Padding + DayWidth + 8.0f + 4.0f * SpeedWidth + 3.0f * Gap + Padding;
		static glm::vec4 PanelColor()
		{
			return glm::vec4(0.07f, 0.08f, 0.10f, 0.82f);
		}

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

		// The brush buttons left to right, a gap, then the size controls; the bar fits them.
		void LayoutBrushBar()
		{
			float x = Padding;
			for (const Entity& button : m_BrushButtons)
			{
				Place(button, glm::vec2(x, 0.0f), glm::vec2(BrushWidth, 112.0f));
				x += BrushWidth + Gap;
			}
			x += m_BrushButtons.empty() ? 0.0f : 12.0f;
			Place(m_SizeDown, glm::vec2(x, 0.0f), glm::vec2(SizeButtonWidth, 112.0f));
			x += SizeButtonWidth + 4.0f;
			Place(m_SizeLabel, glm::vec2(x, 0.0f), glm::vec2(SizeLabelWidth, 112.0f));
			x += SizeLabelWidth + 4.0f;
			Place(m_SizeUp, glm::vec2(x, 0.0f), glm::vec2(SizeButtonWidth, 112.0f));
			x += SizeButtonWidth + Padding;
			m_BrushBar.GetComponent<UIRectComponent>().SizeDelta = glm::vec2(x, BarHeight);
		}

		bool SameChoices(const std::vector<BrushChoice>& choices) const
		{
			if (choices.size() != m_Choices.size() || m_BrushButtons.size() != choices.size())
			{
				return false;
			}
			for (size_t i = 0; i < choices.size(); i++)
			{
				if (choices[i].TileId != m_Choices[i].TileId)
				{
					return false;
				}
			}
			return true;
		}

		static void SetSelected(Entity button, bool selected)
		{
			if (button.IsValid())
			{
				button.GetComponent<UIImageComponent>().Color = selected ? SelectedColor() : FrameColor();
			}
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

		// Left-middle anchored, for the rows of the bars.
		static void Place(Entity element, const glm::vec2& position, const glm::vec2& size)
		{
			UIRectComponent& rect = element.GetComponent<UIRectComponent>();
			rect.AnchorMin = glm::vec2(0.0f, 0.5f);
			rect.AnchorMax = glm::vec2(0.0f, 0.5f);
			rect.Pivot = glm::vec2(0.0f, 0.5f);
			rect.AnchoredPosition = position;
			rect.SizeDelta = size;
		}

		static UIImageComponent& Image(Entity element, const glm::vec4& color)
		{
			UIImageComponent& image = element.AddComponent<UIImageComponent>();
			image.Color = color;
			return image;
		}

		UITextComponent& Text(Entity element, const std::string& text, TextAlignH align)
		{
			UITextComponent& component = element.AddComponent<UITextComponent>();
			component.Text = text;
			component.Font = m_Font;
			component.AlignH = align;
			component.AlignV = TextAlignV::Middle;
			component.Wrap = false;
			component.ShadowColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.6f);
			return component;
		}

		// A framed button sending `event`, with an optional centred label.
		Entity Button(Entity parent, const std::string& name, const std::string& event, const std::string& label)
		{
			Entity button = Element(parent, name, glm::vec2(0.0f, 0.5f), glm::vec2(0.0f), glm::vec2(64.0f, 64.0f));
			Image(button, FrameColor());
			UIButtonComponent& component = button.AddComponent<UIButtonComponent>();
			component.Event = event;
			component.Navigation = UINavigation::None; // WASD moves the camera, not the focus
			if (label.empty() == false)
			{
				Entity text = Element(button, "Label", glm::vec2(0.5f, 0.5f), glm::vec2(0.0f), glm::vec2(64.0f, 56.0f));
				Text(text, label, TextAlignH::Center);
			}
			return button;
		}

		Entity m_Root;
		AssetHandle m_Font = 0;
		Entity m_BrushBar;
		Entity m_SizeDown;
		Entity m_SizeLabel;
		Entity m_SizeUp;
		Entity m_ClockBar;
		Entity m_DayLabel;
		Entity m_Notice;
		Entity m_NoticeText;
		float m_NoticeSeconds = 0.0f;
		Entity m_SpeedButtons[4];
		std::vector<Entity> m_BrushButtons;
		std::vector<BrushChoice> m_Choices;
		int m_ShownTile = -1;
		float m_ShownRadius = -1.0f;
		int m_ShownSpeed = -1;
		int m_ShownDay = -1;
	};
}
