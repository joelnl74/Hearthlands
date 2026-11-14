#pragma once

#include "Hydro.h"
#include "Hydro/Scene/UI/UI.h"
#include "Rules/CameraRules.h"
#include "State/HearthlandsSession.h"

#include <glm/gtc/quaternion.hpp>

namespace Hydro
{
	// The world camera, on the Camera entity: orthographic and isometric (Yaw, Elevation). WASD or the arrows pan, Q / E or
	// - / = zoom in whole pixel steps, the right mouse drags the ground (none while a modal UI is open); it looks over the map.
	HY_SCRIPT()
	class CameraScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Range(0, 360), Tooltip("Degrees the view is turned about the vertical; 45 is isometric"))
		float Yaw = 45.0f;
		HY_PROPERTY(Range(10, 90), Tooltip("Degrees above the ground; 30 is 2:1 isometric, 90 straight down"))
		float Elevation = 30.0f;
		HY_PROPERTY(Range(0, 5), Tooltip("Starting zoom: index into 1/4, 1/2, 1, 2, 3, 4 screen pixels per art pixel"))
		int Zoom = 3;
		HY_PROPERTY(Range(8, 64), Tooltip("Art pixels per tile in the object sprites (sets the pixel scale)"))
		float PixelsPerTile = 32.0f;
		HY_PROPERTY(Range(0.1, 5), Tooltip("Keyboard pan, in screen heights per second"))
		float PanSpeed = 0.8f;

	protected:
		void OnCreate() override
		{
			const int saved = HearthlandsSession::Get().CameraZoom; // a loaded save's, else the property's
			m_Zoom = CameraRules::StepZoom(saved >= 0 ? saved : Zoom, 0);
			CameraComponent* camera = TryGetComponent<CameraComponent>();
			if (camera != nullptr)
			{
				camera->Camera.Projection = ProjectionType::Orthographic;
				camera->Camera.OrthographicNear = 1.0f;
				camera->Camera.OrthographicFar = 3000.0f;
			}
			Place();
		}

		void OnUpdate(float deltaTime) override
		{
			if (HearthlandsSession::Get().Loading)
			{
				return; // a save was loaded: the new scene's camera looks where the save did
			}
			Scene& scene = *GetEntity().GetScene();
			const float viewportHeight = (float)(scene.GetViewportHeight() > 0 ? scene.GetViewportHeight() : 1080);
			const float size = CameraRules::OrthographicSize(viewportHeight, CameraRules::PixelScales[m_Zoom], PixelsPerTile);
			const float pixelsPerUnit = viewportHeight / size;

			const bool free = UI::CapturesInput(scene) == false; // the keys move the menu's focus instead
			glm::vec2 move(0.0f);
			move.x += Held(Key::D, Key::Right) - Held(Key::A, Key::Left);
			move.y += Held(Key::S, Key::Down) - Held(Key::W, Key::Up); // screen y points down
			if (free)
			{
				m_Target += CameraRules::GroundDelta(move * PanSpeed * viewportHeight * deltaTime, pixelsPerUnit, Yaw, Elevation);
			}

			const bool zoomIn = Pressed(Key::E, m_InHeld) || Pressed(Key::Equal, m_InHeld2);
			const bool zoomOut = Pressed(Key::Q, m_OutHeld) || Pressed(Key::Minus, m_OutHeld2);
			if (free && zoomIn)
			{
				m_Zoom = CameraRules::StepZoom(m_Zoom, 1);
			}
			if (free && zoomOut)
			{
				m_Zoom = CameraRules::StepZoom(m_Zoom, -1);
			}

			Drag(scene, pixelsPerUnit, free);
			HearthlandsSession& session = HearthlandsSession::Get();
			const TileMap& map = session.Map;
			if (session.FocusRequested && map.GetWidth() > 0)
			{
				// A tile to look at (a settlement chosen on the HUD): map tiles to the map-centred ground point.
				m_Target = glm::vec2(session.FocusTile.x + 0.5f - (float)map.GetWidth() * 0.5f,
					(float)map.GetHeight() * 0.5f - session.FocusTile.y - 0.5f);
				session.FocusRequested = false;
			}
			if (map.GetWidth() > 0)
			{
				m_Target = CameraRules::ClampTarget(m_Target, map.GetWidth(), map.GetHeight());
				// Where it looks, in map tiles (the focus above the other way round), for saving.
				session.CameraTile = glm::vec2(m_Target.x + (float)map.GetWidth() * 0.5f - 0.5f,
					(float)map.GetHeight() * 0.5f - m_Target.y - 0.5f);
			}
			session.CameraZoom = m_Zoom;
			Place();
		}

	private:
		static constexpr float Distance = 1000.0f; // orthographic: only room for the depth range

		static float Held(KeyCode first, KeyCode second)
		{
			return Input::IsKeyPressed(first) || Input::IsKeyPressed(second) ? 1.0f : 0.0f;
		}

		// True on the frame the key goes down.
		static bool Pressed(KeyCode key, bool& held)
		{
			const bool down = Input::IsKeyPressed(key);
			const bool pressed = down && held == false;
			held = down;
			return pressed;
		}

		// Right mouse held over the game: the ground follows the pointer (not while `free` is false).
		void Drag(Scene& scene, float pixelsPerUnit, bool free)
		{
			const auto [mouseX, mouseY] = Input::GetMousePosition();
			const glm::vec2 mouse(mouseX, mouseY);
			const bool dragging = free && Input::IsMouseButtonPressed(MouseButton::Right) && (m_Dragging || scene.GetUIPointer().Inside);
			if (dragging && m_Dragging)
			{
				m_Target -= CameraRules::GroundDelta(mouse - m_LastMouse, pixelsPerUnit, Yaw, Elevation);
			}
			m_Dragging = dragging;
			m_LastMouse = mouse;
		}

		void Place()
		{
			const glm::quat rotation = CameraRules::Rotation(Yaw, Elevation);
			TransformComponent& transform = GetComponent<TransformComponent>();
			transform.Translation = CameraRules::Position(m_Target, rotation, Distance);
			transform.Rotation = glm::eulerAngles(rotation);
			CameraComponent* camera = TryGetComponent<CameraComponent>();
			Scene& scene = *GetEntity().GetScene();
			if (camera != nullptr)
			{
				const float viewportHeight = (float)(scene.GetViewportHeight() > 0 ? scene.GetViewportHeight() : 1080);
				camera->Camera.OrthographicSize = CameraRules::OrthographicSize(viewportHeight, CameraRules::PixelScales[m_Zoom],
					PixelsPerTile);
			}
		}

		glm::vec2 m_Target{ 0.0f }; // the ground point looked at; starts at the map's centre
		int m_Zoom = 3;
		bool m_InHeld = false;
		bool m_InHeld2 = false;
		bool m_OutHeld = false;
		bool m_OutHeld2 = false;
		bool m_Dragging = false;
		glm::vec2 m_LastMouse{ 0.0f };
	};
}
