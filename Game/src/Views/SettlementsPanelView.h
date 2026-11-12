#pragma once

#include "Data/PeopleDefinition.h"
#include "State/SettlementState.h"
#include "Views/MenuArt.h"
#include "Views/UIBuilder.h"

#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/UI/UI.h"
#include "Hydro/Scene/UI/UILayout.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace Hydro
{
	// The Settlers panel (HUD, top left): "Settlers (n)" and a card per settlement (name, level or tension; people and
	// room, shelters, food, wood). Settlements come on their own; a card opens the settlement's detail. Sends "Camp <id>".
	class SettlementsPanelView
	{
	public:
		static constexpr int MaxLines = 7;

		// ui: the HUD script's, made for this root's scene (it outlives the view).
		void Bind(Entity root, const UIBuilder& ui)
		{
			m_Root = root;
			m_UI = &ui;
			m_Panel = ui.At(root, "Settlers", 16.0f, 86.0f, PanelWidth, CardsTop);
			ui.Art(m_Panel, MenuArt::Sprite::Panel);
			ui.Art(ui.At(m_Panel, "Icon", 18.0f, 14.0f, 32.0f, 32.0f), MenuArt::Sprite::IconPeople);
			m_Title = ui.Label(m_Panel, "Title", 58.0f, 6.0f, PanelWidth - 76.0f, 48.0f, "Settlers", TextStyle::Heading, TextAlignH::Left,
				MenuArt::Gold());
			m_Hint = ui.Label(m_Panel, "Hint", Inset, CardsTop - 4.0f, PanelWidth - 2.0f * Inset, 44.0f, std::string(), TextStyle::Body,
				TextAlignH::Left, MenuArt::Muted(), true);
			for (int i = 0; i < MaxLines; i++)
			{
				m_Cards.push_back(MakeCard(i));
			}
			UI::SetVisible(m_Panel, false);
		}

		// settling: some people may settle (a talent is learned); selected: the settlement whose detail is open.
		void Present(const SettlementState& state, const std::vector<PeopleDefinition>& peoples, bool settling, int selected)
		{
			const bool shown = settling || state.List.empty() == false;
			const int count = (int)std::min(state.List.size(), m_Cards.size());
			const bool hint = state.List.empty();
			const std::string key = std::to_string(count) + (shown ? "S" : "-") + (hint ? "H" : "-");
			if (key != m_ShownKey)
			{
				UI::SetVisible(m_Panel, shown);
				UI::SetVisible(m_Hint, hint);
				UI::SetText(m_Hint, "Settlers come on their own to the land that suits them best");
				UI::SetText(m_Title, "Settlers (" + std::to_string(state.List.size()) + ")");
				const float height = hint ? CardsTop + 48.0f : CardsTop + (float)count * (CardHeight + CardGap) + 6.0f;
				UIBuilder::Resize(m_Panel, PanelWidth, height);
				m_ShownKey = key;
			}
			for (size_t i = 0; i < m_Cards.size(); i++)
			{
				PresentCard(m_Cards[i], i < state.List.size() ? &state.List[i] : nullptr, peoples, state, selected);
			}
		}

		// Whether the pointer is over the panel, as of the last layout.
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
			return UIBuilder::Contains(m_Panel, point);
		}

		static constexpr float PanelWidth = 420.0f;

		// The light warm red of trouble on the dark panels, and the orange of tension rising.
		static glm::vec4 Bad()
		{
			return glm::vec4(1.0f, 0.46f, 0.36f, 1.0f);
		}

		static glm::vec4 Warn()
		{
			return glm::vec4(1.0f, 0.72f, 0.32f, 1.0f);
		}

		// "Tension 62%" while it shows (from quarrels up), else empty.
		static std::string TensionText(const Settlement& settlement)
		{
			return settlement.Tension >= 0.25f ? "Tension " + std::to_string((int)(settlement.Tension * 100.0f + 0.5f)) + "%"
				: std::string();
		}

	private:
		struct Card
		{
			Entity Button;
			Entity Strip;
			Entity Name;
			Entity Status;   // the level, or the tension
			Entity Stats[4]; // people / room, shelters, food, wood
			std::string Shown;
			int Id = -1;
		};

		static constexpr float Inset = 16.0f;
		static constexpr float CardsTop = 64.0f;
		static constexpr float CardHeight = 104.0f;
		static constexpr float CardGap = 8.0f;
		static constexpr float CardWidth = PanelWidth - 2.0f * Inset;

		Card MakeCard(int index)
		{
			const UIBuilder& ui = *m_UI;
			Card card;
			card.Button = ui.At(m_Panel, "Settlement " + std::to_string(index), Inset, CardsTop + (float)index * (CardHeight + CardGap),
				CardWidth, CardHeight);
			ui.Art(card.Button, MenuArt::Sprite::Tile);
			ButtonEvent(card.Button, "Camp");
			card.Strip = ui.At(card.Button, "Header", 6.0f, 6.0f, CardWidth - 12.0f, 46.0f);
			ui.Art(card.Strip, MenuArt::Sprite::ButtonDark);
			ui.Art(ui.At(card.Strip, "Icon", 10.0f, 9.0f, 28.0f, 28.0f), MenuArt::Sprite::IconHouse);
			card.Name = ui.Label(card.Strip, "Name", 46.0f, 0.0f, CardWidth - 200.0f, 46.0f, std::string(), TextStyle::Heading,
				TextAlignH::Left, MenuArt::Ink());
			card.Status = ui.Label(card.Strip, "Status", CardWidth - 12.0f - 150.0f, 0.0f, 140.0f, 46.0f, std::string(), TextStyle::Body,
				TextAlignH::Right, MenuArt::Muted());
			const MenuArt::Sprite icons[4] = { MenuArt::Sprite::IconPeople, MenuArt::Sprite::IconTent, MenuArt::Sprite::IconFood,
				MenuArt::Sprite::IconWood };
			const float step = (CardWidth - 24.0f) / 4.0f;
			for (int s = 0; s < 4; s++)
			{
				const float x = 14.0f + (float)s * step;
				ui.Art(ui.At(card.Button, "Icon", x, 62.0f, 30.0f, 30.0f), icons[s]);
				card.Stats[s] = ui.Label(card.Button, "Value", x + 34.0f, 52.0f, step - 36.0f, 50.0f, "0", TextStyle::Body,
					TextAlignH::Left, MenuArt::Ink());
			}
			UI::SetVisible(card.Button, false);
			return card;
		}

		void PresentCard(Card& card, const Settlement* settlement, const std::vector<PeopleDefinition>& peoples,
			const SettlementState& state, int selected)
		{
			std::string key;
			const int people = settlement != nullptr ? (int)(settlement->Count + 0.5f) : 0;
			std::string status;
			if (settlement != nullptr)
			{
				const bool known = settlement->People >= 0 && settlement->People < (int)peoples.size();
				const std::vector<PeopleLevel>* levels = known ? &peoples[(size_t)settlement->People].Levels : nullptr;
				const bool named = levels != nullptr && settlement->Level < (int)levels->size();
				status = TensionText(*settlement);
				status = status.empty() && named ? (*levels)[(size_t)settlement->Level].Name : status;
				key = settlement->Name + "/" + std::to_string(people) + "/" + std::to_string(settlement->Room) + "/"
					+ std::to_string(settlement->Shelters) + "/" + std::to_string((int)settlement->Food) + "/"
					+ std::to_string((int)settlement->Wood) + "/" + status + (settlement->Id == selected ? "S" : "")
					+ std::to_string(state.List.size());
			}
			if (key == card.Shown)
			{
				return;
			}
			card.Shown = key;
			UI::SetVisible(card.Button, settlement != nullptr);
			if (settlement == nullptr)
			{
				return;
			}
			if (settlement->Id != card.Id)
			{
				ButtonEvent(card.Button, "Camp " + std::to_string(settlement->Id));
				card.Id = settlement->Id;
			}
			m_UI->Art(card.Strip, settlement->Id == selected ? MenuArt::Sprite::ButtonBlue : MenuArt::Sprite::ButtonDark);
			UI::SetText(card.Name, settlement->Name);
			UI::SetText(card.Status, status);
			const float tension = settlement->Tension;
			UIBuilder::SetColor(card.Status, tension >= 0.5f ? Bad() : (tension >= 0.25f ? Warn() : MenuArt::Muted()));
			UI::SetText(card.Stats[0], std::to_string(people) + "/" + std::to_string(settlement->Room));
			UI::SetText(card.Stats[1], std::to_string(settlement->Shelters));
			UI::SetText(card.Stats[2], std::to_string((int)settlement->Food));
			UI::SetText(card.Stats[3], std::to_string((int)settlement->Wood));
		}

		static void ButtonEvent(Entity element, const std::string& event)
		{
			UIButtonComponent& button = element.HasComponent<UIButtonComponent>() ? element.GetComponent<UIButtonComponent>()
				: element.AddComponent<UIButtonComponent>();
			button.Event = event;
			button.Navigation = UINavigation::None; // WASD moves the camera, not the focus
		}

		Entity m_Root;
		const UIBuilder* m_UI = nullptr; // the HUD script's
		Entity m_Panel;
		Entity m_Title;
		Entity m_Hint;
		std::vector<Card> m_Cards;
		std::string m_ShownKey;
	};
}
