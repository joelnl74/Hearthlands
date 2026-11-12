#pragma once

#include "Views/MenuArt.h"

#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/UI/UI.h"

#include <glm/glm.hpp>

#include <string>

namespace Hydro
{
	// The fonts of a menu: running text, headings and page titles (Assets/Fonts, scripts/fonts/make_font.py).
	struct MenuFonts
	{
		AssetHandle Body = 0;
		AssetHandle Heading = 0;
		AssetHandle Title = 0;
	};

	enum class TextStyle : uint8_t
	{
		Body,
		Heading,
		Title
	};

	// Makes UI elements in code for the menu views: rects placed from a corner of their parent (canvas pixels, Y up), images
	// from MenuArt, text in the menu's fonts and buttons that send an event to the canvas's scripts.
	class UIBuilder
	{
	public:
		void Bind(Scene& scene, const MenuFonts& fonts, const MenuArt& art)
		{
			m_Scene = &scene;
			m_Fonts = fonts;
			m_Art = &art;
		}

		const MenuArt& GetArt() const
		{
			return *m_Art;
		}

		// A child anchored at one point of its parent (the pivot there too), `position` from it.
		Entity Element(Entity parent, const std::string& name, const glm::vec2& anchor, const glm::vec2& position,
			const glm::vec2& size) const
		{
			Entity element = m_Scene->CreateEntity(name);
			m_Scene->SetParent(element, parent, false);
			UIRectComponent& rect = element.AddComponent<UIRectComponent>();
			rect.AnchorMin = anchor;
			rect.AnchorMax = anchor;
			rect.Pivot = anchor;
			rect.AnchoredPosition = position;
			rect.SizeDelta = size;
			return element;
		}

		// A child from its parent's top-left corner: x right, y down.
		Entity At(Entity parent, const std::string& name, float x, float y, float width, float height) const
		{
			return Element(parent, name, glm::vec2(0.0f, 1.0f), glm::vec2(x, -y), glm::vec2(width, height));
		}

		// A child filling its parent less `insets` (left, bottom, right, top).
		Entity Fill(Entity parent, const std::string& name, const glm::vec4& insets = glm::vec4(0.0f)) const
		{
			Entity element = m_Scene->CreateEntity(name);
			m_Scene->SetParent(element, parent, false);
			UIRectComponent& rect = element.AddComponent<UIRectComponent>();
			rect.AnchorMin = glm::vec2(0.0f);
			rect.AnchorMax = glm::vec2(1.0f);
			rect.Pivot = glm::vec2(0.5f);
			rect.AnchoredPosition = glm::vec2((insets.x - insets.z) * 0.5f, (insets.y - insets.w) * 0.5f);
			rect.SizeDelta = glm::vec2(-(insets.x + insets.z), -(insets.y + insets.w));
			return element;
		}

		static void Move(Entity element, float x, float y)
		{
			element.GetComponent<UIRectComponent>().AnchoredPosition = glm::vec2(x, -y);
		}

		static void Resize(Entity element, float width, float height)
		{
			element.GetComponent<UIRectComponent>().SizeDelta = glm::vec2(width, height);
		}

		static UIImageComponent& Flat(Entity element, const glm::vec4& color)
		{
			UIImageComponent& image = element.AddComponent<UIImageComponent>();
			image.Color = color;
			return image;
		}

		UIImageComponent& Art(Entity element, MenuArt::Sprite sprite, const glm::vec4& tint = glm::vec4(1.0f)) const
		{
			UIImageComponent& image = element.HasComponent<UIImageComponent>() ? element.GetComponent<UIImageComponent>()
				: element.AddComponent<UIImageComponent>();
			m_Art->Apply(image, sprite, tint);
			return image;
		}

		UITextComponent& Text(Entity element, const std::string& text, TextStyle style, TextAlignH align, const glm::vec4& color,
			bool wrap = false) const
		{
			UITextComponent& component = element.AddComponent<UITextComponent>();
			component.Text = text;
			component.Font = style == TextStyle::Title ? m_Fonts.Title : (style == TextStyle::Heading ? m_Fonts.Heading : m_Fonts.Body);
			if (component.Font == 0)
			{
				component.Font = m_Fonts.Body;
			}
			component.AlignH = align;
			component.AlignV = wrap ? TextAlignV::Top : TextAlignV::Middle;
			component.Wrap = wrap;
			component.Color = color;
			const bool dark = color.r + color.g + color.b < 1.2f;
			component.ShadowColor = dark ? glm::vec4(0.0f) : glm::vec4(0.0f, 0.0f, 0.0f, 0.6f);
			return component;
		}

		// A text element from the parent's top-left corner.
		Entity Label(Entity parent, const std::string& name, float x, float y, float width, float height, const std::string& text,
			TextStyle style, TextAlignH align, const glm::vec4& color, bool wrap = false) const
		{
			Entity element = At(parent, name, x, y, width, height);
			Text(element, text, style, align, color, wrap);
			return element;
		}

		// Makes the element a button sending `event`; keys move between buttons (the menu's canvas is modal).
		static UIButtonComponent& Button(Entity element, const std::string& event)
		{
			UIButtonComponent& button = element.AddComponent<UIButtonComponent>();
			button.Event = event;
			button.Navigation = UINavigation::Auto;
			button.FocusedTint = glm::vec4(1.12f, 1.08f, 0.96f, 1.0f);
			button.PressedTint = glm::vec4(0.82f, 0.82f, 0.86f, 1.0f);
			button.DisabledTint = glm::vec4(0.75f, 0.75f, 0.78f, 0.7f);
			return button;
		}

		// The pieces every page shares (the canvas is 1920 x 1080).
		static constexpr float PageMargin = 40.0f;
		static constexpr float PageTop = 190.0f;         // from the top: where a page's content starts
		static constexpr float DetailWidth = 460.0f;     // the parchment detail panel on the right
		static constexpr float DetailHeight = 790.0f;
		static constexpr float NavWidth = 300.0f;        // the track / section list on the left
		static constexpr float ContentX = PageMargin + NavWidth + 30.0f; // a page with a list: its content's left edge
		static constexpr float ContentRight = 1920.0f - PageMargin - DetailWidth - 30.0f;

		// The title at the top middle with a ribbon under it (none for an empty subtitle); returns the ribbon's text.
		Entity PageHeader(Entity page, const std::string& title, const std::string& subtitle) const
		{
			Entity heading = Element(page, "Title", glm::vec2(0.5f, 1.0f), glm::vec2(0.0f, -24.0f), glm::vec2(900.0f, 80.0f));
			Text(heading, title, TextStyle::Title, TextAlignH::Center, MenuArt::Gold());
			for (int side = -1; side <= 1; side += 2)
			{
				Entity diamond = Element(page, "Title Diamond", glm::vec2(0.5f, 1.0f), glm::vec2((float)side * 250.0f, -52.0f),
					glm::vec2(24.0f, 24.0f));
				Art(diamond, MenuArt::Sprite::Diamond);
			}
			Entity ribbonText;
			if (subtitle.empty() == false)
			{
				Entity ribbon = Element(page, "Ribbon", glm::vec2(0.5f, 1.0f), glm::vec2(0.0f, -108.0f), glm::vec2(380.0f, 52.0f));
				Art(ribbon, MenuArt::Sprite::Ribbon);
				ribbonText = Fill(ribbon, "Label", glm::vec4(12.0f, 0.0f, 12.0f, 2.0f));
				Text(ribbonText, subtitle, TextStyle::Heading, TextAlignH::Center, MenuArt::Ink());
			}
			else
			{
				Entity ornament = Element(page, "Ornament", glm::vec2(0.5f, 1.0f), glm::vec2(0.0f, -112.0f), glm::vec2(512.0f, 24.0f));
				Art(ornament, MenuArt::Sprite::Ornament);
			}
			return ribbonText;
		}

		// The parchment panel on the right of a page.
		Entity DetailPanel(Entity page) const
		{
			Entity panel = Element(page, "Detail", glm::vec2(1.0f, 1.0f), glm::vec2(-PageMargin, -(PageTop - 40.0f)),
				glm::vec2(DetailWidth, DetailHeight));
			Art(panel, MenuArt::Sprite::Parchment);
			return panel;
		}

		// The dark panel on the left of a page with a heading; its buttons go under it (NavButton).
		Entity NavPanel(Entity page, const std::string& heading, int buttons) const
		{
			Entity panel = At(page, "Nav", PageMargin, PageTop, NavWidth, 96.0f + (float)buttons * 76.0f);
			Art(panel, MenuArt::Sprite::Panel);
			Label(panel, "Heading", 0.0f, 18.0f, NavWidth, 48.0f, heading, TextStyle::Heading, TextAlignH::Center, MenuArt::Gold());
			return panel;
		}

		// A list button in a NavPanel; `count` shows on its right (empty: none). Returns {button, count text}.
		Entity NavButton(Entity panel, int index, const std::string& label, const std::string& event, Entity& countText) const
		{
			Entity button = At(panel, "Nav " + label, 20.0f, 84.0f + (float)index * 76.0f, NavWidth - 40.0f, 64.0f);
			Art(button, MenuArt::Sprite::ButtonDark);
			Button(button, event);
			Label(button, "Label", 18.0f, 0.0f, NavWidth - 120.0f, 64.0f, label, TextStyle::Body, TextAlignH::Left, MenuArt::Ink());
			countText = Label(button, "Count", NavWidth - 140.0f, 0.0f, 84.0f, 64.0f, std::string(), TextStyle::Body, TextAlignH::Right,
				MenuArt::Muted());
			return button;
		}

		void SetNavSelected(Entity button, bool selected) const
		{
			Art(button, selected ? MenuArt::Sprite::ButtonBlue : MenuArt::Sprite::ButtonDark);
		}

		// A parchment heading band across a detail panel at `y` from its top; returns the band.
		Entity Section(Entity panel, const std::string& name, float y, const std::string& text) const
		{
			Entity band = At(panel, name, 20.0f, y, DetailWidth - 40.0f, 44.0f);
			Art(band, MenuArt::Sprite::ParchmentHeader);
			Label(band, "Label", 16.0f, 0.0f, DetailWidth - 72.0f, 44.0f, text, TextStyle::Body, TextAlignH::Left, MenuArt::DarkInk());
			return band;
		}

		// The round emblem at the top of a detail panel with a letter in it (until icons are painted); returns the letter.
		Entity Emblem(Entity panel) const
		{
			Entity medallion = Element(panel, "Emblem", glm::vec2(0.5f, 1.0f), glm::vec2(0.0f, -18.0f), glm::vec2(128.0f, 128.0f));
			Art(medallion, MenuArt::Sprite::Medallion);
			Entity letter = Fill(medallion, "Letter", glm::vec4(0.0f, 6.0f, 0.0f, 0.0f));
			Text(letter, std::string(), TextStyle::Title, TextAlignH::Center, MenuArt::Gold());
			return letter;
		}

		// A small rounded label (chips under a detail panel's name).
		Entity Chip(Entity parent, const std::string& name, float x, float y, float width, MenuArt::Sprite sprite,
			const glm::vec4& ink) const
		{
			Entity chip = At(parent, name, x, y, width, 42.0f);
			Art(chip, sprite);
			Entity text = Fill(chip, "Label", glm::vec4(8.0f, 0.0f, 8.0f, 0.0f));
			Text(text, std::string(), TextStyle::Body, TextAlignH::Center, ink);
			return text;
		}

		static void SetColor(Entity element, const glm::vec4& color)
		{
			if (UITextComponent* text = element.TryGetComponent<UITextComponent>())
			{
				text->Color = color;
			}
		}

		// Whether the canvas-pixel point is inside the element's laid-out rect (as of the last layout).
		static bool Contains(Entity element, const glm::vec2& point)
		{
			const UIResolvedRectComponent* rect = element.IsValid() ? element.TryGetComponent<UIResolvedRectComponent>() : nullptr;
			return rect != nullptr && rect->Visible && point.x >= rect->Min.x && point.y >= rect->Min.y && point.x <= rect->Max.x
				&& point.y <= rect->Max.y;
		}

	private:
		Scene* m_Scene = nullptr; // the scene of the canvas the builder makes elements in (it outlives the view)
		MenuFonts m_Fonts;
		const MenuArt* m_Art = nullptr;
	};
}
