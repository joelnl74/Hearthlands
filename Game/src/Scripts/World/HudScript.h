#pragma once

#include "Hydro.h"
#include "Hydro/Renderer/Font.h"
#include "Hydro/Scene/UI/UI.h"
#include "Data/TerrainTypeDefinition.h"
#include "Rules/BrushRules.h"
#include "State/HearthlandsSession.h"
#include "Views/HudView.h"

#include <string>
#include <vector>

namespace Hydro
{
	// The world's controls, on the HUD entity (made the HUD canvas): brush, size, speed and the moisture overlay from the
	// HUD's buttons and keys (1-9, [ ], P, M) into the session; tells the brush when the pointer is over the HUD.
	HY_SCRIPT()
	class HudScript : public ScriptableEntity
	{
	public:
		static constexpr uint64_t DefaultFont = 7311820458392716001ull;         // Assets/Fonts/OpenSans.fnt
		static constexpr uint64_t DefaultTerrainTypes = 4392505292887402202ull; // Assets/Data/TerrainTypes.hytable

		HY_PROPERTY(Asset(Font), Tooltip("The HUD's text"))
		AssetHandle UIFont = DefaultFont;
		HY_PROPERTY(Asset(DataTable, TerrainTypeDefinition), Tooltip("The brushes' names and colours"))
		AssetHandle TerrainTypes = DefaultTerrainTypes;

	protected:
		void OnCreate() override
		{
			AssetManager::GetAsset<Font>(UIFont); // the renderer never loads
			const DataTableView<TerrainTypeDefinition> table = Data::GetTable<TerrainTypeDefinition>(TerrainTypes);
			for (const auto& [id, type] : table.Rows())
			{
				m_Types.push_back(type);
			}
			m_View.Bind(GetEntity(), UIFont);
		}

		void OnUpdate(float deltaTime) override
		{
			HearthlandsSession& session = HearthlandsSession::Get();
			if (session.UnlockedTiles != m_Unlocked)
			{
				AnnounceUnlocks(session.UnlockedTiles);
				m_Unlocked = session.UnlockedTiles;
				m_View.SetBrushes(Choices(m_Unlocked));
			}
			ReadKeys(session);
			session.Brush.TileId = BrushRules::BrushTile(session.Brush.TileId, session.UnlockedTiles);
			session.PointerOverHud = m_View.ContainsPointer(*GetEntity().GetScene());
			m_View.Present(session.Brush.TileId, session.Brush.Radius, session.Clock.Speed, session.Clock.GetDay(), deltaTime);
		}

		bool OnUIEvent(const UIEvent& event) override
		{
			if (event.Type != UIEventType::Submit)
			{
				return false;
			}
			HearthlandsSession& session = HearthlandsSession::Get();
			const std::string& name = event.Name;
			if (name.rfind("Brush ", 0) == 0)
			{
				session.Brush.TileId = (uint8_t)std::stoi(name.substr(6));
			}
			else if (name == "Size -" || name == "Size +")
			{
				session.Brush.Radius = BrushRules::StepRadius(session.Brush.Radius, name == "Size +" ? 1 : -1);
			}
			else if (name.rfind("Speed ", 0) == 0)
			{
				session.Clock.Speed = std::stoi(name.substr(6));
			}
			UI::ClearFocus(*GetEntity().GetScene()); // so Space and Enter stay with the game
			return true;
		}

	private:
		// A notice for each brush that joined the list since the last update (none for the starting brushes).
		void AnnounceUnlocks(const std::vector<uint8_t>& unlocked)
		{
			if (m_Unlocked.empty())
			{
				return;
			}
			for (size_t i = 0; i < unlocked.size(); i++)
			{
				bool known = false;
				for (uint8_t tileId : m_Unlocked)
				{
					known = known || tileId == unlocked[i];
				}
				if (known == false)
				{
					const std::string key = i < 9 ? " (key " + std::to_string(i + 1) + ")" : std::string();
					m_View.ShowNotice("New brush: " + Label(unlocked[i]) + key, 6.0f);
				}
			}
		}

		std::string Label(uint8_t tileId) const
		{
			for (const TerrainTypeDefinition& type : m_Types)
			{
				if (type.TileId == (int)tileId)
				{
					return type.BrushName.empty() ? type.Name : type.BrushName;
				}
			}
			return "Tile " + std::to_string((int)tileId);
		}

		// The unlocked types with their names and colours, in id order.
		std::vector<HudView::BrushChoice> Choices(const std::vector<uint8_t>& unlocked) const
		{
			std::vector<HudView::BrushChoice> choices;
			for (uint8_t tileId : unlocked)
			{
				HudView::BrushChoice choice;
				choice.TileId = tileId;
				choice.Name = Label(tileId);
				for (const TerrainTypeDefinition& type : m_Types)
				{
					if (type.TileId == (int)tileId)
					{
						choice.Color = glm::vec4(type.Color, 1.0f);
					}
				}
				choices.push_back(choice);
			}
			return choices;
		}

		void ReadKeys(HearthlandsSession& session)
		{
			if (UI::CapturesInput(*GetEntity().GetScene()))
			{
				return;
			}
			const KeyCode digits[9] = { Key::D1, Key::D2, Key::D3, Key::D4, Key::D5, Key::D6, Key::D7, Key::D8, Key::D9 };
			for (int i = 0; i < 9 && i < (int)session.UnlockedTiles.size(); i++)
			{
				if (Input::IsKeyPressed(digits[i]))
				{
					session.Brush.TileId = session.UnlockedTiles[(size_t)i];
				}
			}
			if (Pressed(Key::LeftBracket, m_SmallerHeld))
			{
				session.Brush.Radius = BrushRules::StepRadius(session.Brush.Radius, -1);
			}
			if (Pressed(Key::RightBracket, m_LargerHeld))
			{
				session.Brush.Radius = BrushRules::StepRadius(session.Brush.Radius, 1);
			}
			if (Pressed(Key::P, m_PauseHeld))
			{
				if (session.Clock.Speed == 0)
				{
					session.Clock.Speed = m_SpeedBeforePause;
				}
				else
				{
					m_SpeedBeforePause = session.Clock.Speed;
					session.Clock.Speed = 0;
				}
			}
			if (session.Clock.Speed > 0)
			{
				m_SpeedBeforePause = session.Clock.Speed;
			}
			if (Pressed(Key::M, m_MoistureHeld))
			{
				session.ShowMoisture = session.ShowMoisture == false;
			}
		}

		// True on the frame the key goes down.
		static bool Pressed(KeyCode key, bool& held)
		{
			const bool down = Input::IsKeyPressed(key);
			const bool pressed = down && held == false;
			held = down;
			return pressed;
		}

		HudView m_View;
		std::vector<TerrainTypeDefinition> m_Types;
		std::vector<uint8_t> m_Unlocked;
		int m_SpeedBeforePause = 1;
		bool m_SmallerHeld = false;
		bool m_LargerHeld = false;
		bool m_PauseHeld = false;
		bool m_MoistureHeld = false;
	};
}
