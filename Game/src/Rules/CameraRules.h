#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

namespace Hydro
{
	// The world camera as maths: orthographic, looking at a target on the ground (z = 0) from a fixed angle (yaw about Z,
	// elevation above the ground; 45 and 30 degrees is 2:1 isometric). The map is centred on the origin, 1 m tiles. Pure.
	class CameraRules
	{
	public:
		// Screen pixels per art pixel the zoom steps through (sprites are PixelsPerTile art pixels a tile).
		static constexpr float PixelScales[6] = { 0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f };

		// The camera's rotation: turned by yaw about Z, tilted down to elevation above the ground (90 = straight down).
		static glm::quat Rotation(float yawDegrees, float elevationDegrees)
		{
			const glm::quat yaw = glm::angleAxis(glm::radians(yawDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
			const glm::quat tilt = glm::angleAxis(glm::radians(90.0f - elevationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
			return yaw * tilt;
		}

		// Where the camera sits: back from the target along its view, `distance` away (orthographic: only depth room).
		static glm::vec3 Position(const glm::vec2& target, const glm::quat& rotation, float distance)
		{
			return glm::vec3(target, 0.0f) + rotation * glm::vec3(0.0f, 0.0f, 1.0f) * distance;
		}

		// The orthographic size (world units shown vertically) that draws an art pixel as pixelScale screen pixels.
		static float OrthographicSize(float viewportHeight, float pixelScale, float pixelsPerTile)
		{
			const float perUnit = pixelScale * pixelsPerTile;
			return perUnit > 0.0f && viewportHeight > 0.0f ? viewportHeight / perUnit : 1.0f;
		}

		// The zoom step after `steps` more (in, positive) or fewer, within PixelScales.
		static int StepZoom(int index, int steps)
		{
			const int next = index + steps;
			return next < 0 ? 0 : (next > 5 ? 5 : next);
		}

		// The ground movement for a movement on screen (pixels, x right, y down), so the ground follows a drag: screen right
		// is the camera's right, screen up its forward on the ground, stretched by 1 / sin(elevation).
		static glm::vec2 GroundDelta(const glm::vec2& screenDelta, float pixelsPerUnit, float yawDegrees, float elevationDegrees)
		{
			if (pixelsPerUnit <= 0.0f)
			{
				return glm::vec2(0.0f);
			}
			const float yaw = glm::radians(yawDegrees);
			const glm::vec2 right(std::cos(yaw), std::sin(yaw));
			const glm::vec2 forward(-std::sin(yaw), std::cos(yaw));
			const float stretch = 1.0f / std::sin(glm::radians(elevationDegrees > 1.0f ? elevationDegrees : 1.0f));
			return (right * screenDelta.x - forward * screenDelta.y * stretch) / pixelsPerUnit;
		}

		// Keeps the target over the map, so the camera stays on the terrain.
		static glm::vec2 ClampTarget(const glm::vec2& target, int width, int height)
		{
			const float halfX = (float)width * 0.5f;
			const float halfY = (float)height * 0.5f;
			return glm::vec2(Clamp(target.x, -halfX, halfX), Clamp(target.y, -halfY, halfY));
		}

	private:
		static float Clamp(float value, float low, float high)
		{
			return value < low ? low : (value > high ? high : value);
		}
	};
}
