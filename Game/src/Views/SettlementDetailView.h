#pragma once

#include "Data/BuildingDefinition.h"
#include "Data/PeopleDefinition.h"
#include "Rules/BuildingRules.h"
#include "Rules/SettlementRules.h"
#include "State/SettlementState.h"
#include "Views/MenuArt.h"
#include "Views/SettlementsPanelView.h"
#include "Views/UIBuilder.h"

#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/UI/UI.h"
#include "Hydro/Scene/UI/UILayout.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace Hydro
{
	// The selected settlement in depth (beside the Settlers panel): level, stores, focus, tension, what its next level needs,
	// its people's buildings and every villager's job. Sends "Detail Close", "Rename <id>", "Focus <id> <0-3>".
	class SettlementDetailView
	{
	public:
		static constexpr int MaxNeeds = 5;
		static constexpr int MaxVillagers = 28; // two columns
		static constexpr int MaxBuildings = 4;

		void Bind(Entity root, const UIBuilder& ui)
		{
			m_Root = root;
			m_UI = &ui;
			m_Panel = ui.At(root, "Settlement Detail", Left, Top, Width, 400.0f);
			ui.Art(m_Panel, MenuArt::Sprite::Panel);

			Entity header = ui.At(m_Panel, "Header", Inset, 12.0f, Width - 2.0f * Inset, 64.0f);
			ui.Art(header, MenuArt::Sprite::ButtonDark);
			ui.Art(ui.At(header, "Icon", 12.0f, 14.0f, 36.0f, 36.0f), MenuArt::Sprite::IconHouse);
			m_Name = ui.Label(header, "Name", 58.0f, 0.0f, 260.0f, 40.0f, std::string(), TextStyle::Heading, TextAlignH::Left,
				MenuArt::Gold());
			m_Kind = ui.Label(header, "Kind", 58.0f, 34.0f, 260.0f, 28.0f, std::string(), TextStyle::Body, TextAlignH::Left,
				MenuArt::Muted());
			m_Rename = SmallButton(header, "Rename", Width - 2.0f * Inset - 188.0f, 12.0f, 116.0f, "Rename");
			m_Close = SmallButton(header, "Close", Width - 2.0f * Inset - 64.0f, 12.0f, 52.0f, "X");
			ButtonEvent(m_Close, "Detail Close");

			const MenuArt::Sprite icons[4] = { MenuArt::Sprite::IconPeople, MenuArt::Sprite::IconTent, MenuArt::Sprite::IconFood,
				MenuArt::Sprite::IconWood };
			const float step = (Width - 2.0f * Inset) / 4.0f;
			for (int s = 0; s < 4; s++)
			{
				const float x = Inset + 8.0f + (float)s * step;
				ui.Art(ui.At(m_Panel, "Stat Icon", x, 92.0f, 30.0f, 30.0f), icons[s]);
				m_Stats[s] = ui.Label(m_Panel, "Stat", x + 36.0f, 82.0f, step - 40.0f, 50.0f, "0", TextStyle::Body, TextAlignH::Left,
					MenuArt::Ink());
			}

			ui.Label(m_Panel, "Focus Label", Inset, 140.0f, 90.0f, 48.0f, "Focus", TextStyle::Body, TextAlignH::Left, MenuArt::Muted());
			const char* focuses[4] = { "Any", "Food", "Wood", "Build" };
			const float focusWidth = (Width - 2.0f * Inset - 96.0f - 3.0f * 8.0f) / 4.0f;
			for (int f = 0; f < 4; f++)
			{
				const float x = Inset + 96.0f + (float)f * (focusWidth + 8.0f);
				m_Focus[f] = SmallButton(m_Panel, std::string("Focus ") + focuses[f], x, 144.0f, focusWidth, focuses[f]);
			}

			m_Tension = ui.Label(m_Panel, "Tension", Inset, 196.0f, Width - 2.0f * Inset, 40.0f, std::string(), TextStyle::Body,
				TextAlignH::Left, MenuArt::Muted(), true);
			m_Evolve = ui.Label(m_Panel, "Evolve", Inset, 236.0f, Width - 2.0f * Inset, 36.0f, std::string(), TextStyle::Body,
				TextAlignH::Left, MenuArt::Muted(), true);
			m_LevelTitle = ui.Label(m_Panel, "Level Title", Inset, 276.0f, Width - 2.0f * Inset, 40.0f, std::string(), TextStyle::Heading,
				TextAlignH::Left, MenuArt::Gold());
			m_LevelText = ui.Label(m_Panel, "Level Text", Inset, 312.0f, Width - 2.0f * Inset, 32.0f, std::string(), TextStyle::Body,
				TextAlignH::Left, MenuArt::Muted(), true);
			for (int n = 0; n < MaxNeeds; n++)
			{
				NeedLine line;
				line.Mark = ui.At(m_Panel, "Need Mark", Inset + 6.0f, NeedsTop + (float)n * NeedStep + 6.0f, 22.0f, 22.0f);
				ui.Art(line.Mark, MenuArt::Sprite::Check);
				line.Name = ui.Label(m_Panel, "Need", Inset + 40.0f, NeedsTop + (float)n * NeedStep, 280.0f, NeedStep, std::string(),
					TextStyle::Body, TextAlignH::Left, MenuArt::Ink());
				line.Value = ui.Label(m_Panel, "Need Value", Width - Inset - 180.0f, NeedsTop + (float)n * NeedStep, 180.0f, NeedStep,
					std::string(), TextStyle::Body, TextAlignH::Right, MenuArt::Ink());
				m_Needs.push_back(line);
			}
			m_BuildingsTitle = ui.Label(m_Panel, "Buildings Title", Inset, 0.0f, Width - 2.0f * Inset, 40.0f, "Buildings",
				TextStyle::Heading, TextAlignH::Left, MenuArt::Gold());
			const float half = (Width - 2.0f * Inset) / 2.0f;
			for (int b = 0; b < MaxBuildings; b++)
			{
				m_Buildings.push_back(ui.Label(m_Panel, "Building", Inset + (float)(b % 2) * half, 0.0f, half - 8.0f, VillagerStep,
					std::string(), TextStyle::Body, TextAlignH::Left, MenuArt::Muted()));
			}
			m_VillagersTitle = ui.Label(m_Panel, "Villagers Title", Inset, 0.0f, Width - 2.0f * Inset, 40.0f, std::string(),
				TextStyle::Heading, TextAlignH::Left, MenuArt::Gold());
			const float column = (Width - 2.0f * Inset) / 2.0f;
			for (int v = 0; v < MaxVillagers; v++)
			{
				m_Villagers.push_back(ui.Label(m_Panel, "Villager", Inset + (float)(v % 2) * column, 0.0f, column - 8.0f, VillagerStep,
					std::string(), TextStyle::Body, TextAlignH::Left, MenuArt::Ink()));
			}
			UI::SetVisible(m_Panel, false);
		}

		// The settlement with Id `selected` (0 or gone: the panel hides); buildings: the Buildings table, owned: the talents.
		void Present(const SettlementState& state, const std::vector<PeopleDefinition>& peoples,
			const std::vector<BuildingDefinition>& buildings, const std::vector<int>& owned, int selected)
		{
			const Settlement* settlement = nullptr;
			for (const Settlement& candidate : state.List)
			{
				settlement = candidate.Id == selected ? &candidate : settlement;
			}
			const bool known = settlement != nullptr && settlement->People >= 0 && settlement->People < (int)peoples.size();
			if (known == false)
			{
				if (m_Shown.empty() == false)
				{
					UI::SetVisible(m_Panel, false);
					m_Shown.clear();
				}
				return;
			}
			const std::vector<BuildingRules::Line> lines = BuildingRules::Lines(buildings, peoples[(size_t)settlement->People],
				*settlement, owned);
			std::string key = Key(*settlement, state);
			for (const BuildingRules::Line& line : lines)
			{
				key += "/" + line.State;
			}
			if (key == m_Shown)
			{
				return;
			}
			m_Shown = key;
			UI::SetVisible(m_Panel, true);
			const BuildingRules::Bonus bonus = BuildingRules::Of(buildings, *settlement);
			Show(*settlement, peoples[(size_t)settlement->People], state, lines, bonus.StoreDays);
			ShowEvolve(*settlement, peoples);
		}

		bool ContainsPointer(Scene& scene) const
		{
			const Scene::UIPointer& pointer = scene.GetUIPointer();
			UICanvasMetrics metrics;
			if (m_Shown.empty() || pointer.Inside == false || m_Root.IsValid() == false
				|| scene.GetUICanvasMetrics(m_Root.GetUUID(), metrics) == false || metrics.Scale <= 0.0f)
			{
				return false;
			}
			const glm::vec2 point = (pointer.Position - metrics.Offset) / metrics.Scale;
			return UIBuilder::Contains(m_Panel, point);
		}

	private:
		struct NeedLine
		{
			Entity Mark;
			Entity Name;
			Entity Value;
		};

		static constexpr float Left = 16.0f + SettlementsPanelView::PanelWidth + 12.0f;
		static constexpr float Top = 86.0f;
		static constexpr float Width = 540.0f;
		static constexpr float Inset = 18.0f;
		static constexpr float NeedsTop = 348.0f;
		static constexpr float NeedStep = 36.0f;
		static constexpr float VillagerStep = 30.0f;

		// What the panel shows, as text: it changes when any of it does.
		static std::string Key(const Settlement& settlement, const SettlementState& state)
		{
			const int values[] = { settlement.Id, settlement.People, settlement.EvolveTo, settlement.EvolveHave,
				settlement.EvolveAllowed ? 1 : 0, settlement.Fields, settlement.FieldsGrowing, settlement.FieldsRipe,
				settlement.Level, (int)(settlement.Count + 0.5f), settlement.Room, settlement.Shelters,
				(int)settlement.Food, (int)settlement.Wood, (int)settlement.Focus, (int)(settlement.Tension * 100.0f), settlement.FeudWith,
				settlement.Needed, settlement.NeededTiles, (int)state.List.size(), settlement.FieldsMost };
			std::string key = settlement.Name;
			for (int value : values)
			{
				key += "/" + std::to_string(value);
			}
			for (const Villager& villager : settlement.Villagers)
			{
				key += "/" + std::to_string(villager.Id) + ":" + std::to_string((int)villager.Job);
			}
			return key;
		}

		void Show(const Settlement& settlement, const PeopleDefinition& people, const SettlementState& state,
			const std::vector<BuildingRules::Line>& buildings, float storeDays)
		{
			const PeopleLevel& level = SettlementRules::LevelOf(people, settlement.Level);
			const int count = (int)(settlement.Count + 0.5f);
			UI::SetText(m_Name, settlement.Name);
			UI::SetText(m_Kind, people.Name + "  -  " + (level.Name.empty() ? std::string("Settlement") : level.Name) + " (level "
				+ std::to_string(settlement.Level + 1) + ")");
			ButtonEvent(m_Rename, "Rename " + std::to_string(settlement.Id));
			const float store = std::max(1.0f, settlement.Count) * people.FoodPerPerson * (level.StoreDays + storeDays);
			UI::SetText(m_Stats[0], std::to_string(count) + " / " + std::to_string(settlement.Room));
			UI::SetText(m_Stats[1], std::to_string(settlement.Shelters));
			UI::SetText(m_Stats[2], std::to_string((int)settlement.Food) + " / " + std::to_string((int)store));
			UI::SetText(m_Stats[3], std::to_string((int)settlement.Wood));
			for (int f = 0; f < 4; f++)
			{
				ButtonEvent(m_Focus[f], "Focus " + std::to_string(settlement.Id) + " " + std::to_string(f));
				m_UI->Art(m_Focus[f], (int)settlement.Focus == f ? MenuArt::Sprite::ButtonBlue : MenuArt::Sprite::ButtonDark);
			}

			std::string foe;
			for (const Settlement& other : state.List)
			{
				foe = other.Id == settlement.FeudWith ? other.Name : foe;
			}
			const int tension = (int)(settlement.Tension * 100.0f + 0.5f);
			if (settlement.Tension <= 0.0f || foe.empty())
			{
				UI::SetText(m_Tension, "At peace with its neighbours");
				UIBuilder::SetColor(m_Tension, MenuArt::Muted());
			}
			else
			{
				const char* stage = settlement.Tension >= 0.95f ? "deadly" : (settlement.Tension >= 0.75f ? "fights"
					: (settlement.Tension >= 0.5f ? "raids" : (settlement.Tension >= 0.25f ? "quarrels" : "uneasy")));
				UI::SetText(m_Tension, "Tension with " + foe + ": " + std::to_string(tension) + "% (" + stage + ")");
				UIBuilder::SetColor(m_Tension, settlement.Tension >= 0.5f ? SettlementsPanelView::Bad()
					: (settlement.Tension >= 0.25f ? SettlementsPanelView::Warn() : MenuArt::Muted()));
			}

			const std::vector<SettlementRules::Need> needs = SettlementRules::NextNeeds(settlement, people);
			const bool top = SettlementRules::HasNextLevel(people, settlement.Level) == false;
			const PeopleLevel& next = SettlementRules::LevelOf(people, settlement.Level + 1);
			UI::SetText(m_LevelTitle, top ? "Top level for now" : "To grow into a " + next.Name);
			UI::SetText(m_LevelText, top ? "Later ages will bring more" : next.Description);
			for (int n = 0; n < MaxNeeds; n++)
			{
				NeedLine& line = m_Needs[(size_t)n];
				const bool shown = n < (int)needs.size();
				UI::SetVisible(line.Mark, shown);
				UI::SetVisible(line.Name, shown);
				UI::SetVisible(line.Value, shown);
				if (shown == false)
				{
					continue;
				}
				const SettlementRules::Need& need = needs[(size_t)n];
				const bool met = need.Have >= need.Want;
				UI::SetText(line.Name, need.Name);
				UI::SetText(line.Value, std::to_string((int)need.Have) + " / " + std::to_string((int)need.Want));
				UIBuilder::SetColor(line.Value, met ? MenuArt::Good() : SettlementsPanelView::Bad());
				m_UI->Art(line.Mark, MenuArt::Sprite::Check, met ? MenuArt::Good() : glm::vec4(1.0f, 1.0f, 1.0f, 0.18f));
			}

			// Their buildings, two to a line: built, being built, or what holds them back.
			const float buildingsTop = NeedsTop + (float)std::max(1, (int)needs.size()) * NeedStep + 14.0f;
			const size_t built = std::min(buildings.size(), (size_t)MaxBuildings);
			UI::SetVisible(m_BuildingsTitle, built > 0);
			UIBuilder::Move(m_BuildingsTitle, Inset, buildingsTop);
			for (size_t b = 0; b < m_Buildings.size(); b++)
			{
				Entity label = m_Buildings[b];
				UI::SetVisible(label, b < built);
				if (b >= built)
				{
					continue;
				}
				const BuildingRules::Line& line = buildings[b];
				const float half = (Width - 2.0f * Inset) / 2.0f;
				UIBuilder::Move(label, Inset + (float)(b % 2) * half, buildingsTop + 42.0f + (float)(b / 2) * VillagerStep);
				UI::SetText(label, line.Name + ": " + line.State);
				const bool building = line.State == "building";
				UIBuilder::SetColor(label, line.Standing ? MenuArt::Good() : (building ? MenuArt::Sky() : MenuArt::Muted()));
			}
			const float villagersTop = built > 0 ? buildingsTop + 42.0f + (float)((built + 1) / 2) * VillagerStep + 10.0f : buildingsTop;
			UIBuilder::Move(m_VillagersTitle, Inset, villagersTop);
			UI::SetText(m_VillagersTitle, "Villagers (" + std::to_string(settlement.Villagers.size()) + ")");
			const size_t shown = std::min(settlement.Villagers.size(), (size_t)MaxVillagers);
			for (size_t v = 0; v < m_Villagers.size(); v++)
			{
				Entity label = m_Villagers[v];
				UI::SetVisible(label, v < shown);
				if (v >= shown)
				{
					continue;
				}
				const Villager& villager = settlement.Villagers[v];
				const float column = (Width - 2.0f * Inset) / 2.0f;
				UIBuilder::Move(label, Inset + (float)(v % 2) * column, villagersTop + 42.0f + (float)(v / 2) * VillagerStep);
				UI::SetText(label, villager.Name + "  -  " + SettlementRules::JobName(villager.Job));
				UIBuilder::SetColor(label, JobColor(villager.Job));
			}
			const float rows = (float)((shown + 1) / 2);
			UIBuilder::Resize(m_Panel, Width, villagersTop + 42.0f + rows * VillagerStep + 16.0f);
		}

		// The branch its people could become: "Could become wood elves: old trees 2 / 4 near" (learned: "Becoming ...").
		void ShowEvolve(const Settlement& settlement, const std::vector<PeopleDefinition>& peoples)
		{
			const bool known = settlement.EvolveTo >= 0 && settlement.EvolveTo < (int)peoples.size();
			const PeopleDefinition* own = settlement.People >= 0 && settlement.People < (int)peoples.size()
				? &peoples[(size_t)settlement.People] : nullptr;
			const bool farms = known == false && own != nullptr && own->FarmTileId >= 0;
			UI::SetVisible(m_Evolve, known || farms);
			if (farms)
			{
				ShowFields(settlement, *own);
				return;
			}
			if (known == false)
			{
				return;
			}
			const PeopleDefinition& to = peoples[(size_t)settlement.EvolveTo];
			const std::string needs = to.NeedObjects > 0 ? to.NeedObjectsName : to.NeedTilesName;
			const bool ready = settlement.EvolveHave >= settlement.EvolveWant;
			const std::string count = std::to_string(settlement.EvolveHave) + " / " + std::to_string(settlement.EvolveWant) + " " + needs;
			UI::SetText(m_Evolve, settlement.EvolveAllowed ? (ready ? "Becoming " + Lower(to.Name) + ": " + count + " near"
				: "Could become " + Lower(to.Name) + " with " + count + " near") : Lower(to.Name, true) + " arise once learned ("
				+ count + " near)");
			UIBuilder::SetColor(m_Evolve, settlement.EvolveAllowed && ready ? MenuArt::Good() : MenuArt::Muted());
		}

		// Its fields: "Fields: 24 (10 growing, 4 ripe)"; before its farming level, when farming comes.
		void ShowFields(const Settlement& settlement, const PeopleDefinition& people)
		{
			const bool farming = SettlementRules::Farming(people, settlement);
			std::string text;
			if (farming == false)
			{
				text = "Farming from a " + Lower(SettlementRules::LevelOf(people, people.FarmFromLevel).Name);
			}
			else if (settlement.Fields == 0)
			{
				text = "No farmland near: paint fields with the Farmland brush";
			}
			else if (settlement.FieldsMost > 0 && settlement.Fields > settlement.FieldsMost)
			{
				text = "Fields: " + std::to_string(settlement.FieldsMost) + " of " + std::to_string(settlement.Fields) + " worked ("
					+ std::to_string(settlement.FieldsGrowing) + " growing, " + std::to_string(settlement.FieldsRipe) + " ripe)";
			}
			else
			{
				text = "Fields: " + std::to_string(settlement.Fields) + " (" + std::to_string(settlement.FieldsGrowing) + " growing, "
					+ std::to_string(settlement.FieldsRipe) + " ripe)";
			}
			UI::SetText(m_Evolve, text);
			UIBuilder::SetColor(m_Evolve, farming && settlement.Fields > 0 ? MenuArt::Good() : MenuArt::Muted());
		}

		// Lower case, or with `capital` only the first letter upper.
		static std::string Lower(std::string text, bool capital = false)
		{
			for (size_t i = 0; i < text.size(); i++)
			{
				const char c = text[i];
				const bool upper = i == 0 && capital;
				text[i] = upper ? (c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c) : (c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c);
			}
			return text;
		}

		static glm::vec4 JobColor(VillagerJob job)
		{
			switch (job)
			{
			case VillagerJob::Gatherer: return glm::vec4(0.70f, 0.90f, 0.55f, 1.0f);
			case VillagerJob::Hunter: return glm::vec4(0.95f, 0.78f, 0.55f, 1.0f);
			case VillagerJob::Woodcutter: return glm::vec4(0.85f, 0.70f, 0.50f, 1.0f);
			case VillagerJob::Builder: return MenuArt::Gold();
			case VillagerJob::Guard: return MenuArt::Sky();
			case VillagerJob::Forester: return glm::vec4(0.55f, 0.85f, 0.70f, 1.0f);
			case VillagerJob::Farmer: return glm::vec4(0.98f, 0.86f, 0.46f, 1.0f);
			case VillagerJob::Hurt: return SettlementsPanelView::Bad();
			default: return MenuArt::Muted();
			}
		}

		Entity SmallButton(Entity parent, const std::string& name, float x, float y, float width, const std::string& text) const
		{
			Entity button = m_UI->At(parent, name, x, y, width, 40.0f);
			m_UI->Art(button, MenuArt::Sprite::ButtonDark);
			ButtonEvent(button, name);
			m_UI->Text(m_UI->Fill(button, "Label"), text, TextStyle::Body, TextAlignH::Center, MenuArt::Ink());
			return button;
		}

		static void ButtonEvent(Entity element, const std::string& event)
		{
			UIButtonComponent& button = element.HasComponent<UIButtonComponent>() ? element.GetComponent<UIButtonComponent>()
				: element.AddComponent<UIButtonComponent>();
			button.Event = event;
			button.Navigation = UINavigation::None; // WASD moves the camera, not the focus
		}

		Entity m_Root;
		const UIBuilder* m_UI = nullptr;
		Entity m_Panel;
		Entity m_Name;
		Entity m_Kind;
		Entity m_Rename;
		Entity m_Close;
		Entity m_Stats[4];
		Entity m_Focus[4];
		Entity m_Tension;
		Entity m_Evolve;
		Entity m_LevelTitle;
		Entity m_LevelText;
		std::vector<NeedLine> m_Needs;
		Entity m_BuildingsTitle;
		std::vector<Entity> m_Buildings;
		Entity m_VillagersTitle;
		std::vector<Entity> m_Villagers;
		std::string m_Shown;
	};
}
