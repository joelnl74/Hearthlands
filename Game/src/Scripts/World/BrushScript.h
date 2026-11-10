#pragma once

#include "Hydro.h"
#include "Hydro/Scene/UI/UI.h"
#include "Rules/BrushRules.h"
#include "State/HearthlandsSession.h"
#include "Views/BrushCursorView.h"

#include <vector>

namespace Hydro
{
	// The player's terrain brush, on the Terrain entity next to WorldScript: holding the left mouse button over the land
	// paints the session's brush (HudScript picks it) in a round stamp, joined along the drag; a ring shows where.
	HY_SCRIPT()
	class BrushScript : public ScriptableEntity
	{
	protected:
		void OnCreate() override
		{
			m_Cursor.Bind(*GetEntity().GetScene());
		}

		void OnUpdate([[maybe_unused]] float deltaTime) override
		{
			HearthlandsSession& session = HearthlandsSession::Get();
			const Scene::UIPointer& pointer = GetEntity().GetScene()->GetUIPointer();
			glm::vec2 point(0.0f);
			const bool overLand = pointer.Inside && session.PointerOverHud == false && PointUnderPointer(session.Map, point)
				&& IsLand(session, point);
			const float radius = session.Brush.Radius;
			m_Cursor.Present(overLand, GroundPoint(session.Map, point), radius);

			const bool painting = overLand && pointer.Down && UI::CapturesInput(*GetEntity().GetScene()) == false;
			if (painting == false || session.UnlockedTiles.empty())
			{
				m_Stroking = false;
				return;
			}

			const uint8_t tileId = BrushRules::BrushTile(session.Brush.TileId, session.UnlockedTiles);
			const glm::vec2 from = m_Stroking ? m_LastPoint : point;
			for (const glm::vec2& step : BrushRules::Stroke(from, point, 0.5f))
			{
				for (const glm::ivec2& at : BrushRules::Stamp(step, radius, session.Map.GetWidth(), session.Map.GetHeight(), session.Land))
				{
					if (BrushRules::CanPaint(session.Map.Get(at.x, at.y), tileId, session.Water.WaterTileId, session.Water.BedTileId))
					{
						session.Map.Set(at.x, at.y, tileId);
					}
				}
			}
			m_LastPoint = point;
			m_Stroking = true;
		}

		void OnDestroy() override
		{
			m_Cursor.Release();
		}

	private:
		static bool IsLand(const HearthlandsSession& session, const glm::vec2& point)
		{
			const size_t index = (size_t)point.y * (size_t)session.Map.GetWidth() + (size_t)point.x;
			return session.Land.empty() || (index < session.Land.size() && session.Land[index] != 0);
		}

		// A map point back in the world, on this entity's ground.
		glm::vec3 GroundPoint(const TileMap& map, const glm::vec2& point)
		{
			const glm::vec3 centre = GetWorldPosition();
			const glm::vec2 offset(point.x - (float)map.GetWidth() * 0.5f, (float)map.GetHeight() * 0.5f - point.y);
			return glm::vec3(centre.x + offset.x, centre.y + offset.y, centre.z);
		}

		// The map point under the pointer (BrushRules::MapPoint): the primary camera's ray met with the ground (this z).
		bool PointUnderPointer(const TileMap& map, glm::vec2& point)
		{
			Scene& scene = *GetEntity().GetScene();
			Entity camera = scene.GetPrimaryCameraEntity();
			if ((bool)camera == false || scene.GetViewportWidth() == 0 || scene.GetViewportHeight() == 0 || map.GetWidth() == 0)
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

		BrushCursorView m_Cursor;
		bool m_Stroking = false;
		glm::vec2 m_LastPoint{ 0.0f };
	};
}
