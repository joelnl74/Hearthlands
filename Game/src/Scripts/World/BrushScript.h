#pragma once

#include "Hydro.h"
#include "Hydro/Scene/UI/UI.h"
#include "Data/TerrainTypeDefinition.h"
#include "Rules/BrushRules.h"
#include "State/HearthlandsSession.h"

#include <string>
#include <vector>

namespace Hydro
{
	// The player's terrain brush, on the Terrain entity next to WorldScript: keys 1 to 9 pick one of the run's unlocked
	// tile types, holding the left mouse button over the land paints a round brush, joined along the drag (never the sea).
	HY_SCRIPT()
	class BrushScript : public ScriptableEntity
	{
	public:
		static constexpr uint64_t DefaultTerrainTypes = 4392505292887402202ull; // Assets/Data/TerrainTypes.hytable

		HY_PROPERTY(Asset(DataTable, TerrainTypeDefinition), Tooltip("Where the brushes' names come from"))
		AssetHandle TerrainTypes = DefaultTerrainTypes;
		HY_PROPERTY(Range(0.5, 8), Tooltip("The brush's radius in tiles, from the point under the pointer"))
		float Radius = 1.5f;

	protected:
		void OnCreate() override
		{
			const DataTableView<TerrainTypeDefinition> table = Data::GetTable<TerrainTypeDefinition>(TerrainTypes);
			for (const auto& [id, type] : table.Rows())
			{
				if (type.TileId >= 0 && type.TileId <= 255)
				{
					m_Names.resize(256);
					m_Names[(size_t)type.TileId] = type.Name;
				}
			}
		}

		void OnUpdate([[maybe_unused]] float deltaTime) override
		{
			HearthlandsSession& session = HearthlandsSession::Get();
			if (session.UnlockedTiles.empty())
			{
				return;
			}
			PickBrush(session.UnlockedTiles);

			glm::vec2 point(0.0f);
			const Scene::UIPointer& pointer = GetEntity().GetScene()->GetUIPointer();
			const bool painting = pointer.Down && pointer.Inside && UI::CapturesInput(*GetEntity().GetScene()) == false;
			if (painting == false || PointUnderPointer(session.Map, point) == false)
			{
				m_Stroking = false;
				return;
			}

			const uint8_t tileId = session.UnlockedTiles[(size_t)m_Brush];
			const glm::vec2 from = m_Stroking ? m_LastPoint : point;
			for (const glm::vec2& step : BrushRules::Stroke(from, point, 0.5f))
			{
				for (const glm::ivec2& at : BrushRules::Stamp(step, Radius, session.Map.GetWidth(), session.Map.GetHeight(), session.Land))
				{
					session.Map.Set(at.x, at.y, tileId);
				}
			}
			m_LastPoint = point;
			m_Stroking = true;
		}

	private:
		// Keys 1 to 9 pick the first nine unlocked types; the pick stays valid when the list changes.
		void PickBrush(const std::vector<uint8_t>& unlocked)
		{
			const KeyCode keys[9] = { Key::D1, Key::D2, Key::D3, Key::D4, Key::D5, Key::D6, Key::D7, Key::D8, Key::D9 };
			int picked = m_Brush < (int)unlocked.size() ? m_Brush : 0;
			for (int i = 0; i < 9 && i < (int)unlocked.size(); i++)
			{
				if (Input::IsKeyPressed(keys[i]))
				{
					picked = i;
				}
			}
			if (picked != m_Brush || m_Announced == false)
			{
				const uint8_t tileId = unlocked[(size_t)picked];
				const std::string name = tileId < m_Names.size() && m_Names[tileId].empty() == false ? m_Names[tileId] : "tile";
				HY_INFO("Brush: {} (tile {}, key {})", name, tileId, picked + 1);
				m_Announced = true;
			}
			m_Brush = picked;
		}

		// The map point under the pointer (BrushRules::MapPoint): the primary camera's ray met with the ground (this z).
		bool PointUnderPointer(const TileMap& map, glm::vec2& point)
		{
			Scene& scene = *GetEntity().GetScene();
			Entity camera = scene.GetPrimaryCameraEntity();
			if ((bool)camera == false || scene.GetViewportWidth() == 0 || scene.GetViewportHeight() == 0)
			{
				return false;
			}

			// The pointer is in the view's pixels with Y up; the projection's Y flip puts the image's top at -1.
			const glm::vec2 position = scene.GetUIPointer().Position;
			const glm::vec2 ndc(position.x / (float)scene.GetViewportWidth() * 2.0f - 1.0f,
				1.0f - position.y / (float)scene.GetViewportHeight() * 2.0f);
			const glm::mat4 cameraTransform = scene.GetWorldTransform(camera);
			const glm::mat4 projection = camera.GetComponent<CameraComponent>().Camera.GetProjection(scene.GetViewportAspectRatio());
			const glm::mat4 inverse = glm::inverse(projection * glm::inverse(cameraTransform));
			glm::vec4 nearPoint = inverse * glm::vec4(ndc, 0.0f, 1.0f);
			glm::vec4 farPoint = inverse * glm::vec4(ndc, 1.0f, 1.0f);
			nearPoint /= nearPoint.w;
			farPoint /= farPoint.w;

			const glm::vec3 centre = GetWorldPosition();
			glm::vec3 hit(0.0f);
			if (BrushRules::RayToGround(glm::vec3(nearPoint), glm::vec3(farPoint - nearPoint), centre.z, hit) == false)
			{
				return false;
			}
			point = BrushRules::MapPoint(glm::vec2(hit.x - centre.x, hit.y - centre.y), map.GetWidth(), map.GetHeight());
			return BrushRules::OnMap(point, map.GetWidth(), map.GetHeight());
		}

		std::vector<std::string> m_Names; // by tile id
		int m_Brush = 0;                  // index into the session's UnlockedTiles
		bool m_Announced = false;
		bool m_Stroking = false;
		glm::vec2 m_LastPoint{ 0.0f };
	};
}
