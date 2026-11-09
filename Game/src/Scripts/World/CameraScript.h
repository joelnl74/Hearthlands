#pragma once

#include "Hydro.h"
#include "Rules/CameraRules.h"
#include "State/HearthlandsSession.h"

namespace Hydro
{
	// The world camera, on the Camera entity: WASD or the arrow keys pan, Q / E or - / = zoom, holding the right mouse button
	// drags the ground. It always looks at a point over the terrain (CameraRules::ClampTarget), from a fixed tilt.
	HY_SCRIPT()
	class CameraScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Range(10, 80), Tooltip("Degrees from looking straight down"))
		float Pitch = 35.0f;
		HY_PROPERTY(Range(4, 400), Tooltip("Starting distance to the ground, in metres (tiles)"))
		float Distance = 32.0f;
		HY_PROPERTY(Range(2, 100))
		float MinDistance = 8.0f;
		HY_PROPERTY(Range(10, 400))
		float MaxDistance = 160.0f;
		HY_PROPERTY(Range(0.1, 5), Tooltip("Keyboard pan, in camera distances per second"))
		float PanSpeed = 1.0f;
		HY_PROPERTY(Range(0.5, 20), Tooltip("Zoom steps per second while a zoom key is held"))
		float ZoomSpeed = 4.0f;

	protected:
		void OnCreate() override
		{
			m_Distance = CameraRules::Zoom(Distance, 0.0f, MinDistance, MaxDistance);
			Place();
		}

		void OnUpdate(float deltaTime) override
		{
			const TileMap& map = HearthlandsSession::Get().Map;
			glm::vec2 move(0.0f);
			move.x += Held(Key::D, Key::Right) - Held(Key::A, Key::Left);
			move.y += Held(Key::W, Key::Up) - Held(Key::S, Key::Down);
			m_Target += move * PanSpeed * m_Distance * deltaTime;

			const float zoom = Held(Key::E, Key::Equal) - Held(Key::Q, Key::Minus);
			m_Distance = CameraRules::Zoom(m_Distance, zoom * ZoomSpeed * deltaTime, MinDistance, MaxDistance);

			Drag();
			if (map.GetWidth() > 0)
			{
				m_Target = CameraRules::ClampTarget(m_Target, map.GetWidth(), map.GetHeight());
			}
			Place();
		}

	private:
		static float Held(KeyCode first, KeyCode second)
		{
			return Input::IsKeyPressed(first) || Input::IsKeyPressed(second) ? 1.0f : 0.0f;
		}

		// Right mouse held over the game: the ground follows the pointer.
		void Drag()
		{
			Scene& scene = *GetEntity().GetScene();
			const auto [mouseX, mouseY] = Input::GetMousePosition();
			const glm::vec2 mouse(mouseX, mouseY);
			const bool dragging = Input::IsMouseButtonPressed(MouseButton::Right) && (m_Dragging || scene.GetUIPointer().Inside);
			if (dragging && m_Dragging)
			{
				const CameraComponent* camera = TryGetComponent<CameraComponent>();
				const float fov = camera != nullptr ? camera->Camera.PerspectiveFOV : 50.0f;
				const float scale = CameraRules::MetresPerPixel(m_Distance, fov, (float)scene.GetViewportHeight());
				const glm::vec2 delta = mouse - m_LastMouse; // window pixels, y down
				m_Target += glm::vec2(-delta.x, delta.y) * scale;
			}
			m_Dragging = dragging;
			m_LastMouse = mouse;
		}

		void Place()
		{
			const float pitch = glm::radians(Pitch);
			TransformComponent& transform = GetComponent<TransformComponent>();
			transform.Translation = CameraRules::Position(m_Target, m_Distance, pitch);
			transform.Rotation = glm::vec3(pitch, 0.0f, 0.0f);
		}

		glm::vec2 m_Target{ 0.0f }; // the ground point looked at; starts at the map's centre
		float m_Distance = 32.0f;
		bool m_Dragging = false;
		glm::vec2 m_LastMouse{ 0.0f };
	};
}
