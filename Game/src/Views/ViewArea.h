#pragma once

#include "Rules/BrushRules.h"

#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"

#include <glm/glm.hpp>

namespace Hydro
{
	// What the primary camera sees of the map, for the views that only draw near it (objects, animals).
	class ViewArea
	{
	public:
		// The tiles (x0, y0, x1, y1) in view, `margin` tiles wider for tall sprites and hills (centre: the map's centre, its
		// ground at centre.z). Leaves rect alone (false) without a camera or when a corner's ray misses the ground.
		static bool VisibleTiles(Scene& scene, int width, int height, const glm::vec3& centre, int margin, glm::ivec4& rect)
		{
			Entity camera = scene.GetPrimaryCameraEntity();
			if ((bool)camera == false)
			{
				return false;
			}
			const glm::mat4 projection = camera.GetComponent<CameraComponent>().Camera.GetProjection(scene.GetViewportAspectRatio());
			const glm::mat4 inverse = glm::inverse(projection * glm::inverse(scene.GetWorldTransform(camera)));
			glm::vec2 low(1e9f);
			glm::vec2 high(-1e9f);
			const glm::vec2 corners[4] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 1.0f }, { 1.0f, 1.0f } };
			for (const glm::vec2& corner : corners)
			{
				glm::vec4 nearPoint = inverse * glm::vec4(corner, 0.0f, 1.0f);
				glm::vec4 farPoint = inverse * glm::vec4(corner, 1.0f, 1.0f);
				nearPoint /= nearPoint.w;
				farPoint /= farPoint.w;
				glm::vec3 hit(0.0f);
				if (BrushRules::RayToGround(glm::vec3(nearPoint), glm::vec3(farPoint - nearPoint), centre.z, hit) == false)
				{
					return false;
				}
				const glm::vec2 point = BrushRules::MapPoint(glm::vec2(hit.x - centre.x, hit.y - centre.y), width, height);
				low = glm::min(low, point);
				high = glm::max(high, point);
			}
			rect.x = Clamp((int)low.x - margin, 0, width - 1);
			rect.y = Clamp((int)low.y - margin, 0, height - 1);
			rect.z = Clamp((int)high.x + margin, 0, width - 1);
			rect.w = Clamp((int)high.y + margin, 0, height - 1);
			return true;
		}

		// Which way the screen's right and up run over the map, in map tiles (x right, y down the map), so a move on the map
		// can be turned into a facing. False without a camera (then right is +x and up -y).
		static bool ScreenAxes(Scene& scene, glm::vec2& right, glm::vec2& up)
		{
			right = glm::vec2(1.0f, 0.0f);
			up = glm::vec2(0.0f, -1.0f);
			Entity camera = scene.GetPrimaryCameraEntity();
			if ((bool)camera == false)
			{
				return false;
			}
			const glm::mat4 transform = scene.GetWorldTransform(camera);
			const glm::vec2 worldRight(transform[0].x, transform[0].y); // the world's y runs up the map: tiles' y is its minus
			const glm::vec2 worldUp(transform[1].x, transform[1].y);
			if (glm::length(worldRight) < 1e-4f || glm::length(worldUp) < 1e-4f)
			{
				return false;
			}
			right = glm::normalize(glm::vec2(worldRight.x, -worldRight.y));
			up = glm::normalize(glm::vec2(worldUp.x, -worldUp.y));
			return true;
		}

		static int Clamp(int value, int low, int high)
		{
			return value < low ? low : (value > high ? high : value);
		}
	};
}
