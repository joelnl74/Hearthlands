#pragma once

#include <glm/glm.hpp>

#include <cmath>

namespace Hydro
{
	// The world camera as maths: it looks at a target on the ground (z = 0) from a distance, tilted by a pitch (radians
	// from straight down, leaning towards +Y). The map is centred on the origin, 1 m tiles. Pure.
	class CameraRules
	{
	public:
		// Where the camera sits to look at the target from that distance and pitch.
		static glm::vec3 Position(const glm::vec2& target, float distance, float pitch)
		{
			return glm::vec3(target.x, target.y - std::sin(pitch) * distance, std::cos(pitch) * distance);
		}

		// Keeps the target over the map, so the camera stays on the terrain.
		static glm::vec2 ClampTarget(const glm::vec2& target, int width, int height)
		{
			const float halfX = (float)width * 0.5f;
			const float halfY = (float)height * 0.5f;
			return glm::vec2(Clamp(target.x, -halfX, halfX), Clamp(target.y, -halfY, halfY));
		}

		// Zooming by steps: each step in (positive) brings the camera 15% closer; clamped to the limits.
		static float Zoom(float distance, float steps, float minDistance, float maxDistance)
		{
			return Clamp(distance * std::pow(0.85f, steps), minDistance, maxDistance);
		}

		// About how many metres of ground one pixel covers at the target, so a drag moves the ground with the pointer.
		static float MetresPerPixel(float distance, float verticalFovDegrees, float viewportHeight)
		{
			const float halfFov = glm::radians(verticalFovDegrees) * 0.5f;
			return viewportHeight > 0.0f ? 2.0f * distance * std::tan(halfFov) / viewportHeight : 0.0f;
		}

	private:
		static float Clamp(float value, float low, float high)
		{
			return value < low ? low : (value > high ? high : value);
		}
	};
}
