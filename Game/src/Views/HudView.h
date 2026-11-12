#pragma once

#include "Views/MenuArt.h"
#include "Views/UIBuilder.h"

#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/UI/UI.h"
#include "Hydro/Scene/UI/UILayout.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace Hydro
{
	// The world's HUD, built in code on the root (a 1920 x 1080 canvas) in the World Menu's look: brush bar, clock, overlays
	// (Inspect), toasts. Sends "Brush <id>", "Size -", "Size +", "Speed <n>", "Overlay Moisture" / "Soil" / "Groundwater".
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

		// ui: the HUD script's, made for this root's scene (it outlives the view).
		void Bind(Entity root, const UIBuilder& ui)
		{
			m_Root = root;
			m_UI = &ui;
			UICanvasComponent& canvas = root.HasComponent<UICanvasComponent>() ? root.GetComponent<UICanvasComponent>()
				: root.AddComponent<UICanvasComponent>();
			canvas.ReferenceResolution = glm::vec2(1920.0f, 1080.0f);
			canvas.ScaleMode = UIScaleMode::ScaleWithScreen;
			canvas.SortOrder = 10;

			m_BrushBar = ui.Element(root, "Brush Bar", glm::vec2(0.5f, 0.0f), glm::vec2(0.0f, 16.0f), glm::vec2(400.0f, BarHeight));
			ui.Art(m_BrushBar, MenuArt::Sprite::Panel);
			m_SizeDown = Button(m_BrushBar, "Size Down", "Size -", "-", TextStyle::Body);
			m_SizeLabel = ui.Element(m_BrushBar, "Size Label", glm::vec2(0.0f, 0.5f), glm::vec2(0.0f), glm::vec2(SizeLabelWidth, 112.0f));
			ui.Text(m_SizeLabel, "Size", TextStyle::Body, TextAlignH::Center, MenuArt::Ink()).LineSpacing = 0.9f;
			m_SizeUp = Button(m_BrushBar, "Size Up", "Size +", "+", TextStyle::Body);

			m_ClockBar = ui.Element(root, "Clock Bar", glm::vec2(1.0f, 1.0f), glm::vec2(-16.0f, -16.0f), glm::vec2(ClockBarWidth, 76.0f));
			ui.Art(m_ClockBar, MenuArt::Sprite::Panel);
			ui.Art(ui.Element(m_ClockBar, "Season Icon", glm::vec2(0.0f, 0.5f), glm::vec2(Padding, 0.0f), glm::vec2(30.0f, 30.0f)),
				MenuArt::Sprite::IconLeaf);
			m_DayLabel = ui.Element(m_ClockBar, "Day", glm::vec2(0.0f, 0.5f), glm::vec2(Padding + 32.0f, 2.0f),
				glm::vec2(DayWidth - 32.0f, 56.0f));
			ui.Text(m_DayLabel, "Day 1", TextStyle::Heading, TextAlignH::Left, MenuArt::Gold());
			const char* speedLabels[4] = { "II", "1x", "3x", "10x" };
			for (int i = 0; i < 4; i++)
			{
				m_SpeedButtons[i] = Button(m_ClockBar, std::string("Speed ") + speedLabels[i], "Speed " + std::to_string(Speeds[i]),
					speedLabels[i], TextStyle::Body);
				Place(m_SpeedButtons[i], glm::vec2(Padding + DayWidth + 8.0f + (float)i * (SpeedWidth + Gap), 0.0f),
					glm::vec2(SpeedWidth, 52.0f));
			}

			// The overlays to look at the land (top left); the other keys are in the World Menu's Settings > Controls.
			m_InspectBar = ui.At(root, "Inspect Bar", 16.0f, 16.0f, InspectBarWidth, 60.0f);
			ui.Art(m_InspectBar, MenuArt::Sprite::Panel);
			ui.Art(ui.At(m_InspectBar, "Diamond", 16.0f, 20.0f, 20.0f, 20.0f), MenuArt::Sprite::Diamond);
			ui.Label(m_InspectBar, "Label", 42.0f, 2.0f, InspectLabelWidth, 56.0f, "Inspect", TextStyle::Heading, TextAlignH::Left,
				MenuArt::Gold());
			const char* overlays[OverlayCount][2] = { { "Moisture (M)", "Overlay Moisture" }, { "Soil (F)", "Overlay Soil" },
				{ "Groundwater (G)", "Overlay Groundwater" } };
			const MenuArt::Sprite overlayIcons[OverlayCount] = { MenuArt::Sprite::IconDrop, MenuArt::Sprite::IconSprout,
				MenuArt::Sprite::IconDrop };
			const glm::vec4 overlayTints[OverlayCount] = { glm::vec4(1.0f), glm::vec4(1.0f), glm::vec4(0.45f, 0.65f, 1.0f, 1.0f) };
			for (int i = 0; i < OverlayCount; i++)
			{
				const float x = 42.0f + InspectLabelWidth + (float)i * (OverlayWidth + Gap);
				m_OverlayButtons[i] = ui.At(m_InspectBar, overlays[i][0], x, 8.0f, OverlayWidth, 44.0f);
				ui.Art(m_OverlayButtons[i], MenuArt::Sprite::ButtonDark);
				UIBuilder::Button(m_OverlayButtons[i], overlays[i][1]).Navigation = UINavigation::None;
				ui.Art(ui.At(m_OverlayButtons[i], "Icon", 10.0f, 8.0f, 28.0f, 28.0f), overlayIcons[i], overlayTints[i]);
				ui.Text(ui.Fill(m_OverlayButtons[i], "Label", glm::vec4(40.0f, 0.0f, 8.0f, 0.0f)), overlays[i][0], TextStyle::Body,
					TextAlignH::Center, MenuArt::Ink());
			}

			for (int i = 0; i < MaxToasts; i++)
			{
				m_Toasts[i] = MakeToast(i);
			}
			LayoutBrushBar();
		}

		// A toast at the top right for `seconds` (Present counts it down): newest on top, at most MaxToasts. "Fire lit: settlers
		// come ..." shows "Fire lit" as its title and the rest under it.
		void ShowNotice(const std::string& text, float seconds, MenuArt::Sprite icon = MenuArt::Sprite::Diamond)
		{
			const size_t colon = text.find(": ");
			if (colon != std::string::npos && colon <= 24 && text.find('(') > colon)
			{
				ShowToast(icon, text.substr(0, colon), text.substr(colon + 2), seconds);
				return;
			}
			ShowToast(icon, std::string(), text, seconds);
		}

		void ShowToast(MenuArt::Sprite icon, const std::string& title, const std::string& body, float seconds)
		{
			ToastData toast;
			toast.Icon = icon;
			toast.Title = title;
			toast.Body = body;
			toast.Seconds = seconds;
			m_ToastData.insert(m_ToastData.begin(), toast);
			if (m_ToastData.size() > (size_t)MaxToasts)
			{
				m_ToastData.pop_back();
			}
			RefreshToasts();
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
			m_BrushNames.clear();
			m_Choices = choices;
			const UIBuilder& ui = *m_UI;
			for (size_t i = 0; i < choices.size(); i++)
			{
				const BrushChoice& choice = choices[i];
				const std::string event = "Brush " + std::to_string((int)choice.TileId);
				Entity button = Button(m_BrushBar, "Brush " + choice.Name, event, "", TextStyle::Body);
				Entity rim = ui.Element(button, "Picture Rim", glm::vec2(0.5f, 1.0f), glm::vec2(0.0f, -8.0f), glm::vec2(66.0f, 66.0f));
				UIBuilder::Flat(rim, glm::vec4(0.85f, 0.70f, 0.35f, 1.0f));
				Entity picture = ui.Fill(rim, "Picture", glm::vec4(2.0f));
				const std::string sprite = "Brush" + std::to_string((int)choice.TileId);
				ui.GetArt().ApplyNamed(picture.AddComponent<UIImageComponent>(), sprite, choice.Color);
				if (i < 9)
				{
					Entity key = ui.Element(button, "Key", glm::vec2(0.0f, 1.0f), glm::vec2(-8.0f, 8.0f), glm::vec2(32.0f, 32.0f));
					ui.Art(key, MenuArt::Sprite::Diamond); // a gold badge over the corner, the key on it
					ui.Text(ui.Fill(key, "Number"), std::to_string(i + 1), TextStyle::Body, TextAlignH::Center, MenuArt::DarkInk());
				}
				Entity name = ui.Element(button, "Name", glm::vec2(0.5f, 0.0f), glm::vec2(0.0f, 6.0f), glm::vec2(BrushWidth - 8.0f, 34.0f));
				ui.Text(name, choice.Name, TextStyle::Body, TextAlignH::Center, MenuArt::Ink());
				m_BrushButtons.push_back(button);
				m_BrushNames.push_back(name);
			}
			LayoutBrushBar();
			m_ShownTile = -1;
		}

		// season: the clock's label (the season and its day); empty shows the run's day.
		void Present(uint8_t brushTile, float radius, int speed, int day, float deltaTime, const std::string& season = std::string())
		{
			const size_t toasts = m_ToastData.size();
			for (ToastData& toast : m_ToastData)
			{
				toast.Seconds -= deltaTime;
			}
			const auto over = [](const ToastData& toast) { return toast.Seconds <= 0.0f; };
			m_ToastData.erase(std::remove_if(m_ToastData.begin(), m_ToastData.end(), over), m_ToastData.end());
			if (m_ToastData.size() != toasts)
			{
				RefreshToasts();
			}
			if ((int)brushTile != m_ShownTile)
			{
				for (size_t i = 0; i < m_BrushButtons.size(); i++)
				{
					const bool selected = m_Choices[i].TileId == brushTile;
					SetSelected(m_BrushButtons[i], selected);
					UIBuilder::SetColor(m_BrushNames[i], selected ? MenuArt::Gold() : MenuArt::Ink());
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
			const std::string label = season.empty() ? "Day " + std::to_string(day) : season;
			if (label != m_ShownDay)
			{
				UI::SetText(m_DayLabel, label);
				m_ShownDay = label;
			}
		}

		// Which overlay is on (one at a time): its button turns blue. The groundwater button shows once its talent is learned.
		void PresentOverlays(bool moisture, bool soil, bool groundwater = false, bool groundwaterLearned = false)
		{
			const int shown = (moisture ? 1 : 0) | (soil ? 2 : 0) | (groundwater ? 4 : 0) | (groundwaterLearned ? 8 : 0);
			if (shown == m_ShownOverlays)
			{
				return;
			}
			m_ShownOverlays = shown;
			SetSelected(m_OverlayButtons[0], moisture);
			SetSelected(m_OverlayButtons[1], soil);
			SetSelected(m_OverlayButtons[2], groundwater);
			UI::SetVisible(m_OverlayButtons[2], groundwaterLearned);
			UIBuilder::Resize(m_InspectBar, InspectWidth(groundwaterLearned ? OverlayCount : OverlayCount - 1), 60.0f);
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
			return UIBuilder::Contains(m_BrushBar, point) || UIBuilder::Contains(m_ClockBar, point)
				|| UIBuilder::Contains(m_InspectBar, point);
		}

	private:
		static constexpr float BarHeight = 136.0f;
		static constexpr float BrushWidth = 100.0f;
		static constexpr float SizeButtonWidth = 52.0f;
		static constexpr float SizeLabelWidth = 100.0f;
		static constexpr float SpeedWidth = 72.0f;
		static constexpr float DayWidth = 250.0f;
		static constexpr float Padding = 16.0f;
		static constexpr float Gap = 8.0f;
		static constexpr float ClockBarWidth = Padding + DayWidth + 8.0f + 4.0f * SpeedWidth + 3.0f * Gap + Padding;
		static constexpr float InspectLabelWidth = 124.0f;
		static constexpr float OverlayWidth = 214.0f;
		static constexpr int OverlayCount = 3;

		static constexpr float InspectWidth(int buttons)
		{
			return 42.0f + InspectLabelWidth + (float)buttons * OverlayWidth + (float)(buttons - 1) * Gap + Padding;
		}

		static constexpr float InspectBarWidth = 42.0f + InspectLabelWidth + 2.0f * OverlayWidth + Gap + Padding;
		static constexpr int MaxToasts = 3;
		static constexpr float ToastWidth = 500.0f;
		static constexpr float ToastsTop = 176.0f; // under the World Menu's button

		struct ToastData
		{
			MenuArt::Sprite Icon = MenuArt::Sprite::Diamond;
			std::string Title;
			std::string Body;
			float Seconds = 0.0f;
		};

		struct Toast
		{
			Entity Frame;
			Entity Icon;
			Entity Title;
			Entity Body;
		};

		Toast MakeToast(int index)
		{
			const UIBuilder& ui = *m_UI;
			Toast toast;
			toast.Frame = ui.Element(m_Root, "Toast " + std::to_string(index), glm::vec2(1.0f, 1.0f), glm::vec2(-16.0f, -ToastsTop),
				glm::vec2(ToastWidth, 112.0f));
			ui.Art(toast.Frame, MenuArt::Sprite::Panel);
			toast.Icon = ui.At(toast.Frame, "Icon", 16.0f, 16.0f, 40.0f, 40.0f);
			ui.Art(toast.Icon, MenuArt::Sprite::Diamond);
			toast.Title = ui.Label(toast.Frame, "Title", 70.0f, 8.0f, ToastWidth - 86.0f, 40.0f, std::string(), TextStyle::Heading,
				TextAlignH::Left, MenuArt::Gold());
			toast.Body = ui.Label(toast.Frame, "Body", 70.0f, 46.0f, ToastWidth - 86.0f, 60.0f, std::string(), TextStyle::Body,
				TextAlignH::Left, MenuArt::Ink(), true);
			toast.Body.GetComponent<UITextComponent>().LineSpacing = 0.85f;
			UI::SetVisible(toast.Frame, false);
			return toast;
		}

		// The toasts top down, each as tall as its text: a title line and up to two lines under it.
		void RefreshToasts()
		{
			float y = ToastsTop;
			for (int i = 0; i < MaxToasts; i++)
			{
				Toast& toast = m_Toasts[i];
				const bool used = (size_t)i < m_ToastData.size();
				UI::SetVisible(toast.Frame, used);
				if (used == false)
				{
					continue;
				}
				const ToastData& data = m_ToastData[(size_t)i];
				const bool titled = data.Title.empty() == false;
				const float height = titled ? 112.0f : 84.0f;
				m_UI->Art(toast.Icon, data.Icon);
				UIBuilder::Move(toast.Icon, 16.0f, (height - 40.0f) * 0.5f);
				UI::SetVisible(toast.Title, titled);
				UI::SetText(toast.Title, data.Title);
				UI::SetText(toast.Body, data.Body);
				UIBuilder::Move(toast.Body, 70.0f, titled ? 46.0f : 12.0f);
				toast.Frame.GetComponent<UIRectComponent>().AnchoredPosition = glm::vec2(-16.0f, -y);
				toast.Frame.GetComponent<UIRectComponent>().SizeDelta = glm::vec2(ToastWidth, height);
				y += height + 8.0f;
			}
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

		void SetSelected(Entity button, bool selected) const
		{
			if (button.IsValid())
			{
				m_UI->Art(button, selected ? MenuArt::Sprite::ButtonBlue : MenuArt::Sprite::ButtonDark);
			}
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

		// A dark gold-rimmed button sending `event`, with an optional centred label; the keys never move to it.
		Entity Button(Entity parent, const std::string& name, const std::string& event, const std::string& label, TextStyle style)
		{
			const UIBuilder& ui = *m_UI;
			Entity button = ui.Element(parent, name, glm::vec2(0.0f, 0.5f), glm::vec2(0.0f), glm::vec2(64.0f, 64.0f));
			ui.Art(button, MenuArt::Sprite::ButtonDark);
			UIBuilder::Button(button, event).Navigation = UINavigation::None; // WASD moves the camera, not the focus
			if (label.empty() == false)
			{
				ui.Text(ui.Fill(button, "Label"), label, style, TextAlignH::Center, MenuArt::Ink());
			}
			return button;
		}

		Entity m_Root;
		const UIBuilder* m_UI = nullptr; // the HUD script's
		Entity m_BrushBar;
		Entity m_SizeDown;
		Entity m_SizeLabel;
		Entity m_SizeUp;
		Entity m_ClockBar;
		Entity m_InspectBar;
		Entity m_OverlayButtons[OverlayCount];
		Entity m_DayLabel;
		Toast m_Toasts[MaxToasts];
		std::vector<ToastData> m_ToastData; // newest first
		Entity m_SpeedButtons[4];
		std::vector<Entity> m_BrushButtons;
		std::vector<Entity> m_BrushNames;
		std::vector<BrushChoice> m_Choices;
		int m_ShownTile = -1;
		float m_ShownRadius = -1.0f;
		int m_ShownSpeed = -1;
		int m_ShownOverlays = -1;
		std::string m_ShownDay;
	};
}
