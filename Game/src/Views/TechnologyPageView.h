#pragma once

#include "Data/MilestoneDefinition.h"
#include "Data/TalentDefinition.h"
#include "Rules/TalentRules.h"
#include "State/TalentState.h"
#include "Views/UIBuilder.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace Hydro
{
	// The World Menu's Technology page: a tab per branch (Water, Land, Life, Insight) with its talents as cards along a path,
	// three to a line by Row, and the chosen talent on parchment. Sends "Branch <n>", "Talent <id>" (choose) and "Learn".
	class TechnologyPageView
	{
	public:
		static constexpr int BranchCount = 5;

		// effects: per talent in table order, what it does in a line (EffectLine).
		void Bind(const UIBuilder& ui, Entity page, const std::vector<TalentDefinition>& talents, const std::vector<std::string>& effects)
		{
			m_UI = &ui;
			m_Talents = talents;
			m_Effects = effects;
			ui.PageHeader(page, "Technology", "Talent Tree");

			Entity points = ui.At(page, "Points", UIBuilder::PageMargin, 36.0f, 320.0f, 64.0f);
			ui.Art(points, MenuArt::Sprite::ButtonDark);
			ui.Art(ui.At(points, "Diamond", 18.0f, 20.0f, 24.0f, 24.0f), MenuArt::Sprite::Diamond);
			m_PointsText = ui.Label(points, "Text", 54.0f, 0.0f, 250.0f, 64.0f, std::string(), TextStyle::Body, TextAlignH::Left,
				MenuArt::Ink());

			for (int b = 0; b < BranchCount; b++)
			{
				MakeTab(page, b);
			}
			m_SelectRing = ui.At(page, "Selected", 0.0f, 0.0f, CardWidth + 24.0f, CardHeight + 24.0f);
			ui.Art(m_SelectRing, MenuArt::Sprite::Select);
			for (int b = 0; b < BranchCount; b++)
			{
				m_Branches[b] = ui.Fill(page, std::string("Branch ") + BranchName((TalentBranch)b));
				LayOutBranch(b);
			}

			Entity panel = ui.DetailPanel(page);
			m_Letter = ui.Emblem(panel);
			m_Name = ui.Label(panel, "Name", 20.0f, 150.0f, UIBuilder::DetailWidth - 40.0f, 52.0f, std::string(), TextStyle::Heading,
				TextAlignH::Center, MenuArt::DarkInk());
			m_BranchChip = ui.Chip(panel, "Branch", 50.0f, 210.0f, 200.0f, MenuArt::Sprite::ButtonBlue, MenuArt::Ink());
			m_TierChip = ui.Chip(panel, "Tier", 266.0f, 210.0f, 144.0f, MenuArt::Sprite::ParchmentHeader, MenuArt::DarkInk());
			m_Description = ui.Label(panel, "Description", 30.0f, 264.0f, UIBuilder::DetailWidth - 60.0f, 90.0f, std::string(),
				TextStyle::Body, TextAlignH::Left, MenuArt::DarkInk(), true);
			ui.Section(panel, "Effect Band", 360.0f, "Effect");
			m_Effect = ui.Label(panel, "Effect", 36.0f, 406.0f, UIBuilder::DetailWidth - 72.0f, 44.0f, std::string(), TextStyle::Body,
				TextAlignH::Left, MenuArt::DarkGood());
			ui.Section(panel, "Cost Band", 452.0f, "Cost");
			m_Cost = ui.Label(panel, "Cost", 36.0f, 498.0f, UIBuilder::DetailWidth - 72.0f, 44.0f, std::string(), TextStyle::Body,
				TextAlignH::Left, MenuArt::DarkInk());
			ui.Section(panel, "Requires Band", 544.0f, "Prerequisite");
			m_Requires = ui.Label(panel, "Requires", 36.0f, 590.0f, UIBuilder::DetailWidth - 72.0f, 44.0f, std::string(), TextStyle::Body,
				TextAlignH::Left, MenuArt::DarkInk());
			m_Learn = ui.Element(panel, "Learn", glm::vec2(0.5f, 0.0f), glm::vec2(0.0f, 24.0f), glm::vec2(380.0f, 76.0f));
			ui.Art(m_Learn, MenuArt::Sprite::ButtonPurple);
			UIBuilder::Button(m_Learn, "Learn");
			ui.Text(ui.Fill(m_Learn, "Label", glm::vec4(0.0f, 0.0f, 0.0f, 4.0f)), "Learn", TextStyle::Heading, TextAlignH::Center,
				MenuArt::Ink());
			m_Status = ui.Label(panel, "Status", 30.0f, 640.0f, UIBuilder::DetailWidth - 60.0f, 40.0f, std::string(), TextStyle::Body,
				TextAlignH::Center, MenuArt::DarkMuted());

			if (talents.empty() == false)
			{
				Select(talents.front().TalentId);
			}
		}

		// Shows a branch's tab; chooses its first talent unless the chosen one is in it.
		void SetBranch(int branch)
		{
			m_Branch = std::clamp(branch, 0, BranchCount - 1);
			for (int b = 0; b < BranchCount; b++)
			{
				UI::SetVisible(m_Branches[b], b == m_Branch);
			}
			if (m_Selected >= m_Talents.size() || (int)m_Talents[m_Selected].Branch != m_Branch)
			{
				for (size_t i = 0; i < m_Talents.size(); i++)
				{
					if ((int)m_Talents[i].Branch == m_Branch)
					{
						Select(m_Talents[i].TalentId);
						break;
					}
				}
			}
			m_ShownKey.clear();
		}

		// The shown tab's next or previous page of cards; its first talent is chosen.
		void TurnPage(int step)
		{
			const int count = (int)m_BranchPages[m_Branch].size();
			const int page = std::clamp(m_PageOf[m_Branch] + step, 0, std::max(0, count - 1));
			size_t first = m_Talents.size();
			for (size_t i = 0; i < m_Talents.size(); i++)
			{
				const bool here = (int)m_Talents[i].Branch == m_Branch && m_Cards[i].Page == page;
				if (here && (first == m_Talents.size() || m_Talents[i].Row < m_Talents[first].Row))
				{
					first = i;
				}
			}
			if (first < m_Talents.size())
			{
				Select(m_Talents[first].TalentId);
			}
		}

		// Chooses a talent and shows its branch.
		void Select(int talentId)
		{
			for (size_t i = 0; i < m_Talents.size(); i++)
			{
				if (m_Talents[i].TalentId == talentId)
				{
					m_Selected = i;
					UIBuilder::Move(m_SelectRing, m_Cards[i].X - 12.0f, m_Cards[i].Y - 12.0f);
					ShowPage((int)m_Talents[i].Branch, m_Cards[i].Page);
					m_Notice.clear();
					m_ShownKey.clear();
					if ((int)m_Talents[i].Branch != m_Branch)
					{
						SetBranch((int)m_Talents[i].Branch);
					}
				}
			}
		}

		// The first talent that can be learned now (its tab opens), else the chosen one.
		void SelectBuyable(const std::vector<TalentRules::Verdict>& verdicts)
		{
			for (size_t i = 0; i < verdicts.size() && i < m_Talents.size(); i++)
			{
				if (verdicts[i] == TalentRules::Verdict::Buyable)
				{
					Select(m_Talents[i].TalentId);
					return;
				}
			}
		}

		int GetSelected() const
		{
			return m_Selected < m_Talents.size() ? m_Talents[m_Selected].TalentId : -1;
		}

		Entity GetSelectedButton() const
		{
			return m_Selected < m_Cards.size() ? m_Cards[m_Selected].Button : Entity();
		}

		// A line under the Learn button until another talent is chosen ("Needs ... first").
		void SetNotice(const std::string& text, bool good)
		{
			m_Notice = text;
			m_NoticeGood = good;
			m_ShownKey.clear();
		}

		// verdicts: per talent in table order (TalentRules::Check).
		void Present(const TalentState& state, const std::vector<TalentRules::Verdict>& verdicts)
		{
			const std::string key = std::to_string(state.Points) + "/" + std::to_string(state.Owned.size()) + "/"
				+ std::to_string(m_Selected) + "/" + std::to_string(m_Branch) + "/" + std::to_string(verdicts.size());
			if (key == m_ShownKey)
			{
				return;
			}
			m_ShownKey = key;
			UI::SetText(m_PointsText, std::to_string(state.Points) + (state.Points == 1 ? " talent point" : " talent points"));
			int owned[BranchCount] = {};
			int total[BranchCount] = {};
			bool open[BranchCount] = {};
			for (size_t i = 0; i < m_Cards.size() && i < verdicts.size(); i++)
			{
				const int branch = std::clamp((int)m_Talents[i].Branch, 0, BranchCount - 1);
				total[branch]++;
				owned[branch] += verdicts[i] == TalentRules::Verdict::Owned ? 1 : 0;
				open[branch] = open[branch] || verdicts[i] == TalentRules::Verdict::Buyable;
				PresentCard(i, verdicts[i]);
			}
			for (int b = 0; b < BranchCount; b++)
			{
				Tab& tab = m_Tabs[b];
				m_UI->Art(tab.Button, b == m_Branch ? MenuArt::Sprite::ButtonBlue : MenuArt::Sprite::ButtonDark);
				UI::SetText(tab.Count, std::to_string(owned[b]) + " / " + std::to_string(total[b]));
				UI::SetVisible(tab.Badge, open[b]);
			}
			for (Path& path : m_Paths)
			{
				const bool lit = path.From < verdicts.size() && path.To < verdicts.size()
					&& verdicts[path.From] == TalentRules::Verdict::Owned && verdicts[path.To] == TalentRules::Verdict::Owned;
				path.Line.GetComponent<UIImageComponent>().Color = lit ? glm::vec4(0.97f, 0.83f, 0.48f, 1.0f)
					: glm::vec4(0.85f, 0.70f, 0.35f, 0.3f);
			}
			if (m_Selected < m_Talents.size() && m_Selected < verdicts.size())
			{
				PresentDetail(state, m_Talents[m_Selected], m_Effects[m_Selected], verdicts[m_Selected]);
			}
		}

		// What a talent does in a line for the cards and the detail panel; tileName: the brush an UnlockTile talent gives, or
		// the building a Building talent lets them build.
		static std::string EffectLine(const TalentDefinition& talent, const std::string& tileName)
		{
			char text[96];
			const float amount = talent.Amount;
			const int percent = (int)(amount * 100.0f + (amount >= 0.0f ? 0.5f : -0.5f));
			switch (talent.Effect)
			{
			case TalentEffect::UnlockTile: return "Unlocks the " + tileName + " brush";
			case TalentEffect::MoistureReach: std::snprintf(text, sizeof(text), "Water wets +%g tiles further", amount); break;
			case TalentEffect::SpringHead: std::snprintf(text, sizeof(text), "Springs fill +%g tiles of channel", amount); break;
			case TalentEffect::SpringFrom: std::snprintf(text, sizeof(text), "Springs from %d%% weaker groundwater", percent); break;
			case TalentEffect::ClueClarity: std::snprintf(text, sizeof(text), "Groundwater clues +%d%% clearer", percent); break;
			case TalentEffect::ClueFrom: std::snprintf(text, sizeof(text), "Clues from %d%% weaker groundwater", percent); break;
			case TalentEffect::BrushSize: std::snprintf(text, sizeof(text), "Largest brush +%g tiles", amount); break;
			case TalentEffect::GrowthSpeed: std::snprintf(text, sizeof(text), "The land grows %d%% faster", percent); break;
			case TalentEffect::LifeSpeed: std::snprintf(text, sizeof(text), "Plants grow %d%% faster", percent); break;
			case TalentEffect::AnimalRoom: std::snprintf(text, sizeof(text), "Room for %d%% more animals", percent); break;
			case TalentEffect::AnimalSight: std::snprintf(text, sizeof(text), "%d%% more animals shown", percent); break;
			case TalentEffect::SoilRichness: std::snprintf(text, sizeof(text), "Soil grows %d%% richer under plants", percent); break;
			case TalentEffect::Settlers: std::snprintf(text, sizeof(text), "%s settle: a new group %g%% a day", PeopleWord(talent.TargetId),
				amount * 100.0f); break;
			case TalentEffect::FeudCalm: std::snprintf(text, sizeof(text), "Tension between peoples %d%% slower", percent); break;
			case TalentEffect::Evolve: std::snprintf(text, sizeof(text), "%s arise from their kin", PeopleWord(talent.TargetId)); break;
			case TalentEffect::Building: return std::string(PeopleWord(talent.TargetId)) + " build a " + tileName;
			case TalentEffect::ForestCreep: std::snprintf(text, sizeof(text), "Forests spread onto bare ground"); break;
			case TalentEffect::GroundwaterView: std::snprintf(text, sizeof(text), "See the groundwater (G)"); break;
			case TalentEffect::OldGrowth: std::snprintf(text, sizeof(text), "Old forests grow ancient trees"); break;
			default: return "-";
			}
			return text;
		}

		// The people a Settlers talent is for, by PeopleId (Peoples.hytable).
		static const char* PeopleWord(int peopleId)
		{
			const char* words[] = { "Settlers", "Humans", "Wood elves", "Dwarves", "Elves" };
			return peopleId >= 0 && peopleId < 5 ? words[peopleId] : "Settlers";
		}

	private:
		struct Tab
		{
			Entity Button;
			Entity Count;
			Entity Badge; // a blue diamond while the branch has a talent to learn
		};

		struct Card
		{
			Entity Button;
			Entity Badge;
			Entity Name;
			Entity State;
			float X = 0.0f;
			float Y = 0.0f;
			int Page = 0; // of its branch (CardsPerPage to a page)
		};

		struct Path
		{
			Entity Line;
			size_t From = 0; // talents (table order) at its ends: lit gold when both are learned
			size_t To = 0;
		};

		static constexpr float TabsTop = 176.0f;
		static constexpr float TabWidth = 256.0f;
		static constexpr float CardsTop = 270.0f;
		static constexpr float CardWidth = 400.0f;
		static constexpr float CardHeight = 200.0f;
		static constexpr float ColumnStep = (1920.0f - 2.0f * UIBuilder::PageMargin - UIBuilder::DetailWidth - 30.0f - CardWidth) / 2.0f;
		static constexpr float RowStep = CardHeight + 44.0f;
		static constexpr int CardsPerPage = 9; // three lines of three

		static const char* BranchName(TalentBranch branch)
		{
			const char* names[BranchCount] = { "Water", "Land", "Life", "Insight", "Peoples" };
			return names[std::clamp((int)branch, 0, BranchCount - 1)];
		}

		static glm::vec4 BranchColor(int branch)
		{
			const glm::vec4 colors[BranchCount] = { glm::vec4(0.56f, 0.83f, 1.0f, 1.0f), glm::vec4(0.93f, 0.76f, 0.50f, 1.0f),
				glm::vec4(0.60f, 0.90f, 0.50f, 1.0f), glm::vec4(0.80f, 0.66f, 1.0f, 1.0f), glm::vec4(1.0f, 0.64f, 0.56f, 1.0f) };
			return colors[std::clamp(branch, 0, BranchCount - 1)];
		}

		void MakeTab(Entity page, int branch)
		{
			const UIBuilder& ui = *m_UI;
			Tab& tab = m_Tabs[branch];
			const float x = UIBuilder::PageMargin + (float)branch * (TabWidth + 12.0f);
			tab.Button = ui.At(page, std::string("Tab ") + BranchName((TalentBranch)branch), x, TabsTop, TabWidth, 64.0f);
			ui.Art(tab.Button, MenuArt::Sprite::ButtonDark);
			UIBuilder::Button(tab.Button, "Branch " + std::to_string(branch));
			Entity icon = ui.At(tab.Button, "Icon", 16.0f, 14.0f, 36.0f, 36.0f);
			if (branch == 1)
			{
				ui.GetArt().ApplyNamed(icon.AddComponent<UIImageComponent>(), "Brush1", BranchColor(branch)); // the dirt picture
			}
			else
			{
				const MenuArt::Sprite icons[BranchCount] = { MenuArt::Sprite::IconDrop, MenuArt::Sprite::IconSprout,
					MenuArt::Sprite::IconLeaf, MenuArt::Sprite::DiamondBlue, MenuArt::Sprite::IconPeople };
				ui.Art(icon, icons[branch]);
			}
			ui.Label(tab.Button, "Name", 62.0f, 0.0f, 120.0f, 62.0f, BranchName((TalentBranch)branch), TextStyle::Heading, TextAlignH::Left,
				BranchColor(branch));
			tab.Count = ui.Label(tab.Button, "Count", TabWidth - 96.0f, 0.0f, 80.0f, 64.0f, "0 / 0", TextStyle::Body, TextAlignH::Right,
				MenuArt::Muted());
			tab.Badge = ui.At(tab.Button, "Badge", TabWidth - 18.0f, -10.0f, 28.0f, 28.0f);
			ui.Art(tab.Badge, MenuArt::Sprite::DiamondBlue);
			UI::SetVisible(tab.Badge, false);
		}

		// The branch's talents by Row, three to a line, the line after running back (a path that turns at the ends).
		void LayOutBranch(int branch)
		{
			const UIBuilder& ui = *m_UI;
			std::vector<size_t> order;
			for (size_t i = 0; i < m_Talents.size(); i++)
			{
				if ((int)m_Talents[i].Branch == branch)
				{
					order.push_back(i);
				}
			}
			std::stable_sort(order.begin(), order.end(), [this](size_t a, size_t b) { return m_Talents[a].Row < m_Talents[b].Row; });
			m_Cards.resize(m_Talents.size());
			const int pages = std::max(1, ((int)order.size() + CardsPerPage - 1) / CardsPerPage);
			for (int page = 0; page < pages; page++)
			{
				m_BranchPages[branch].push_back(ui.Fill(m_Branches[branch], "Page " + std::to_string(page + 1)));
			}
			if (pages > 1)
			{
				MakePager(branch);
			}
			for (size_t n = 0; n < order.size(); n++)
			{
				const int onPage = (int)n % CardsPerPage;
				const int line = onPage / 3;
				const int column = line % 2 == 0 ? onPage % 3 : 2 - onPage % 3;
				Card& card = m_Cards[order[n]];
				card.X = UIBuilder::PageMargin + (float)column * ColumnStep;
				card.Y = CardsTop + (float)line * RowStep;
				card.Page = (int)n / CardsPerPage;
				if (n > 0 && m_Cards[order[n - 1]].Page == card.Page)
				{
					const Card& previous = m_Cards[order[n - 1]];
					Path path;
					path.From = order[n - 1];
					path.To = order[n];
					if (previous.Y == card.Y)
					{
						const float left = std::min(previous.X, card.X) + CardWidth;
						path.Line = ui.At(m_BranchPages[branch][(size_t)card.Page], "Path", left - 4.0f, card.Y + CardHeight * 0.5f - 3.0f,
							std::max(previous.X, card.X) - left + 8.0f, 6.0f);
					}
					else
					{
						const float top = previous.Y + CardHeight - 4.0f;
						path.Line = ui.At(m_BranchPages[branch][(size_t)card.Page], "Path", card.X + CardWidth * 0.5f - 3.0f, top, 6.0f,
							card.Y + 4.0f - top);
					}
					UIBuilder::Flat(path.Line, glm::vec4(0.85f, 0.70f, 0.35f, 0.3f));
					m_Paths.push_back(path);
				}
			}
			for (size_t n = 0; n < order.size(); n++)
			{
				MakeCard(branch, order[n]);
			}
			ShowPage(branch, 0);
		}

		// "<" and ">" under a tab's cards with the page between (Events "Talents Prev" and "Talents Next").
		void MakePager(int branch)
		{
			const UIBuilder& ui = *m_UI;
			const float y = CardsTop + 3.0f * RowStep - 26.0f;
			for (int side = 0; side < 2; side++)
			{
				const float x = UIBuilder::PageMargin + (side == 0 ? 0.0f : 2.0f * ColumnStep + CardWidth - 120.0f);
				Entity button = ui.At(m_Branches[branch], side == 0 ? "Prev" : "Next", x, y, 120.0f, 56.0f);
				ui.Art(button, MenuArt::Sprite::ButtonDark);
				UIBuilder::Button(button, side == 0 ? "Talents Prev" : "Talents Next");
				ui.Text(ui.Fill(button, "Label"), side == 0 ? "<" : ">", TextStyle::Heading, TextAlignH::Center, MenuArt::Ink());
			}
			m_PagerText[branch] = ui.Label(m_Branches[branch], "Pager", UIBuilder::PageMargin + 140.0f, y,
				2.0f * ColumnStep + CardWidth - 280.0f, 56.0f, std::string(), TextStyle::Body, TextAlignH::Center, MenuArt::Muted());
		}

		void ShowPage(int branch, int page)
		{
			if (branch < 0 || branch >= BranchCount || m_BranchPages[branch].empty())
			{
				return;
			}
			const int count = (int)m_BranchPages[branch].size();
			m_PageOf[branch] = std::clamp(page, 0, count - 1);
			for (int p = 0; p < count; p++)
			{
				UI::SetVisible(m_BranchPages[branch][(size_t)p], p == m_PageOf[branch]);
			}
			if (count > 1)
			{
				UI::SetText(m_PagerText[branch], "Page " + std::to_string(m_PageOf[branch] + 1) + " / " + std::to_string(count));
			}
		}

		void MakeCard(int branch, size_t index)
		{
			const UIBuilder& ui = *m_UI;
			const TalentDefinition& talent = m_Talents[index];
			Card& card = m_Cards[index];
			card.Button = ui.At(m_BranchPages[branch][(size_t)card.Page], "Talent " + talent.Name, card.X, card.Y, CardWidth, CardHeight);
			ui.Art(card.Button, MenuArt::Sprite::Tile);
			UIBuilder::Button(card.Button, "Talent " + std::to_string(talent.TalentId));
			Entity medallion = ui.At(card.Button, "Emblem", 18.0f, 20.0f, 84.0f, 84.0f);
			ui.Art(medallion, MenuArt::Sprite::Medallion);
			const std::string letter(1, talent.Name.empty() ? '?' : talent.Name[0]);
			ui.Text(ui.Fill(medallion, "Letter", glm::vec4(0.0f, 4.0f, 0.0f, 0.0f)), letter, TextStyle::Heading, TextAlignH::Center,
				BranchColor(branch));
			card.Badge = ui.At(medallion, "Badge", 58.0f, -4.0f, 34.0f, 34.0f);
			ui.Art(card.Badge, MenuArt::Sprite::Lock);
			ui.Label(card.Button, "Tier", 18.0f, 112.0f, 84.0f, 36.0f, "Tier " + std::to_string(talent.Row), TextStyle::Body,
				TextAlignH::Center, MenuArt::Muted());
			card.Name = ui.Label(card.Button, "Name", 118.0f, 16.0f, CardWidth - 136.0f, 46.0f, talent.Name, TextStyle::Heading,
				TextAlignH::Left, MenuArt::Ink());
			Entity effect = ui.Label(card.Button, "Effect", 118.0f, 66.0f, CardWidth - 136.0f, 76.0f, m_Effects[index], TextStyle::Body,
				TextAlignH::Left, MenuArt::Sky(), true);
			effect.GetComponent<UITextComponent>().LineSpacing = 0.85f;
			card.State = ui.Label(card.Button, "State", 118.0f, 148.0f, CardWidth - 136.0f, 40.0f, std::string(), TextStyle::Body,
				TextAlignH::Left, MenuArt::Muted());
		}

		void PresentCard(size_t i, TalentRules::Verdict verdict)
		{
			const MenuArt& art = m_UI->GetArt();
			const TalentDefinition& talent = m_Talents[i];
			Card card = m_Cards[i]; // a copy: a const Entity gives const components
			const bool owned = verdict == TalentRules::Verdict::Owned;
			const bool ready = verdict == TalentRules::Verdict::Buyable;
			const MenuArt::Sprite frame = owned ? MenuArt::Sprite::NodeOwned
				: (ready ? MenuArt::Sprite::NodeOpen : MenuArt::Sprite::Tile);
			art.Apply(card.Button.GetComponent<UIImageComponent>(), frame);
			const bool locked = verdict == TalentRules::Verdict::NeedsTalent || verdict == TalentRules::Verdict::NeedsMilestone;
			UIBuilder::SetColor(card.Name, owned ? MenuArt::Gold() : (locked ? MenuArt::Faint() : MenuArt::Ink()));
			UI::SetVisible(card.Badge, owned || locked);
			art.Apply(card.Badge.GetComponent<UIImageComponent>(), owned ? MenuArt::Sprite::Check : MenuArt::Sprite::Lock);
			const std::string cost = std::to_string(talent.Cost) + (talent.Cost == 1 ? " point" : " points");
			std::string state = cost;
			glm::vec4 color = MenuArt::Muted();
			if (owned)
			{
				state = "Learned";
				color = MenuArt::Good();
			}
			else if (ready)
			{
				state = cost + "  -  ready";
				color = MenuArt::Sky();
			}
			else if (verdict == TalentRules::Verdict::NeedsTalent)
			{
				const TalentDefinition* first = TalentRules::Find(m_Talents, talent.RequiresTalentId);
				state = "Needs " + (first != nullptr ? first->Name : std::string("another talent"));
				color = MenuArt::Faint();
			}
			else if (verdict == TalentRules::Verdict::NeedsMilestone)
			{
				state = "Needs " + MilestoneName(talent.RequiresMilestoneId);
				color = MenuArt::Faint();
			}
			UI::SetText(card.State, state);
			UIBuilder::SetColor(card.State, color);
		}

		void PresentDetail(const TalentState& state, const TalentDefinition& talent, const std::string& effect,
			TalentRules::Verdict verdict)
		{
			UI::SetText(m_Letter, std::string(1, talent.Name.empty() ? '?' : talent.Name[0]));
			UI::SetText(m_Name, talent.Name);
			UI::SetText(m_BranchChip, BranchName(talent.Branch));
			UI::SetText(m_TierChip, "Tier " + std::to_string(talent.Row));
			UI::SetText(m_Description, talent.Description);
			UI::SetText(m_Effect, effect);
			UI::SetText(m_Cost, std::to_string(talent.Cost) + (talent.Cost == 1 ? " talent point" : " talent points") + "   (you have "
				+ std::to_string(state.Points) + ")");
			const TalentDefinition* first = TalentRules::Find(m_Talents, talent.RequiresTalentId);
			const bool milestone = talent.RequiresMilestoneId >= 0;
			const bool reached = milestone && TalentRules::Contains(state.Reached, talent.RequiresMilestoneId);
			const std::string goal = milestone ? MilestoneName(talent.RequiresMilestoneId) + (reached ? "   (reached)" : "   (not yet)")
				: std::string();
			if (first == nullptr && milestone == false)
			{
				UI::SetText(m_Requires, "None");
				UIBuilder::SetColor(m_Requires, MenuArt::DarkMuted());
			}
			else
			{
				const bool has = first == nullptr || TalentRules::Contains(state.Owned, first->TalentId);
				const std::string learned = first != nullptr ? first->Name + (has ? "   (learned)" : "   (not yet)") : std::string();
				UI::SetText(m_Requires, learned + (learned.empty() || goal.empty() ? "" : ",  ") + goal);
				UIBuilder::SetColor(m_Requires, has && (milestone == false || reached) ? MenuArt::DarkGood() : MenuArt::DarkBad());
			}

			const bool owned = verdict == TalentRules::Verdict::Owned;
			UI::SetVisible(m_Learn, owned == false);
			UI::SetInteractable(m_Learn, verdict == TalentRules::Verdict::Buyable);
			std::string status = m_Notice;
			glm::vec4 color = m_NoticeGood ? MenuArt::DarkGood() : MenuArt::DarkBad();
			if (status.empty())
			{
				color = MenuArt::DarkMuted();
				switch (verdict)
				{
				case TalentRules::Verdict::Owned: status = "Learned"; color = MenuArt::DarkGood(); break;
				case TalentRules::Verdict::Buyable: status = "Ready to learn"; break;
				case TalentRules::Verdict::NeedsTalent:
					status = "Learn " + (first != nullptr ? first->Name : std::string("its prerequisite")) + " first";
					break;
				case TalentRules::Verdict::NeedsMilestone: status = "Reach " + MilestoneName(talent.RequiresMilestoneId) + " first"; break;
				case TalentRules::Verdict::TooDear: status = "Reach more milestones for points"; break;
				default: break;
				}
			}
			UI::SetText(m_Status, status);
			UIBuilder::SetColor(m_Status, color);
		}

	public:
		// The milestones talents can require (RequiresMilestoneId), for "Needs First hamlet".
		void SetMilestones(const std::vector<MilestoneDefinition>& milestones)
		{
			m_MilestoneNames.clear();
			for (const MilestoneDefinition& milestone : milestones)
			{
				m_MilestoneNames.push_back({ milestone.MilestoneId, milestone.Name });
			}
		}

	private:
		std::string MilestoneName(int milestoneId) const
		{
			for (const std::pair<int, std::string>& milestone : m_MilestoneNames)
			{
				if (milestone.first == milestoneId)
				{
					return milestone.second;
				}
			}
			return "a milestone";
		}

		const UIBuilder* m_UI = nullptr; // the menu view's, which owns this page
		std::vector<TalentDefinition> m_Talents;
		std::vector<std::pair<int, std::string>> m_MilestoneNames; // MilestoneId, name
		std::vector<std::string> m_Effects;
		Tab m_Tabs[BranchCount];
		Entity m_Branches[BranchCount]; // a tab's cards and paths
		std::vector<Entity> m_BranchPages[BranchCount]; // a tab's pages of cards
		int m_PageOf[BranchCount] = {};                 // the page shown per tab
		Entity m_PagerText[BranchCount];                // "Page 1 / 2" (none for a tab of one page)
		std::vector<Card> m_Cards;      // per talent in table order
		std::vector<Path> m_Paths;
		Entity m_PointsText;
		Entity m_SelectRing;
		Entity m_Letter;
		Entity m_Name;
		Entity m_BranchChip;
		Entity m_TierChip;
		Entity m_Description;
		Entity m_Effect;
		Entity m_Cost;
		Entity m_Requires;
		Entity m_Learn;
		Entity m_Status;
		int m_Branch = -1;
		size_t m_Selected = 0;
		std::string m_Notice;
		bool m_NoticeGood = false;
		std::string m_ShownKey;
	};
}
