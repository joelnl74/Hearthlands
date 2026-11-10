#pragma once

#include "Data/ObjectDefinition.h"
#include "Rules/BrushRules.h"
#include "State/ObjectLayer.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"

#include <cstdint>
#include <vector>

namespace Hydro
{
	// Draws the object layer as upright sprites standing on their tiles. Only what the primary camera can see gets a sprite:
	// a pool of entities under one "Objects" entity, reassigned when the view or the layer changes (at most 10 times a second).
	class ObjectsView
	{
	public:
		static constexpr int MaxSprites = 6000;
		static constexpr float RefreshInterval = 0.1f;

		// Made by Bind, removed by Release (call it in OnDestroy).
		void Bind(Scene& scene, const std::vector<ObjectDefinition>& objects)
		{
			m_Scene = &scene;
			m_Root = scene.CreateEntity("Objects"); // only a tag and an ID
			m_Root.AddComponent<TransformComponent>();
			m_Looks.assign(256, Look{});
			for (const ObjectDefinition& object : objects)
			{
				if (object.ObjectId <= 0 || object.ObjectId > 255 || object.Sprite == 0)
				{
					continue;
				}
				Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(object.Sprite); // the renderer never loads
				Look& look = m_Looks[(size_t)object.ObjectId];
				look.Sprite = object.Sprite;
				const float pixelsPerTile = object.PixelsPerTile > 0.0f ? object.PixelsPerTile : 24.0f;
				look.Size = texture != nullptr
					? glm::vec2((float)texture->GetWidth(), (float)texture->GetHeight()) / pixelsPerTile : glm::vec2(1.0f);
				look.CastsShadow = object.CastsShadow;
			}
		}

		// centre: the map's centre in the world (the Terrain entity), its ground at centre.z.
		void Present(const ObjectLayer& layer, int width, int height, const glm::vec3& centre, float deltaTime)
		{
			if (m_Scene == nullptr || m_Root.IsValid() == false || width <= 0 || height <= 0)
			{
				return;
			}
			m_SinceRefresh += deltaTime;
			glm::ivec4 rect(0, 0, width - 1, height - 1);
			VisibleTiles(width, height, centre, rect);
			const bool same = rect == m_Rect && layer.GetRevision() == m_Revision;
			if (same || m_SinceRefresh < RefreshInterval)
			{
				return;
			}
			m_SinceRefresh = 0.0f;
			m_Rect = rect;
			m_Revision = layer.GetRevision();

			int used = 0;
			for (int y = rect.y; y <= rect.w && used < MaxSprites; y++)
			{
				for (int x = rect.x; x <= rect.z && used < MaxSprites; x++)
				{
					const size_t index = (size_t)y * (size_t)width + (size_t)x;
					const uint8_t id = layer.Get(index);
					if (id == 0 || m_Looks[id].Sprite == 0)
					{
						continue;
					}
					Entity sprite = PoolSprite(used++);
					const Look& look = m_Looks[id];
					const uint32_t hash = Hash((uint32_t)index);
					const glm::vec2 jitter((float)(hash & 255u) / 255.0f - 0.5f, (float)((hash >> 8) & 255u) / 255.0f - 0.5f);
					TransformComponent& transform = sprite.GetComponent<TransformComponent>();
					// The quad is centred on its position and stands up about its bottom edge: half its height up in Y.
					transform.Translation = centre + glm::vec3((float)x + 0.5f - (float)width * 0.5f + jitter.x * 0.4f,
						(float)height * 0.5f - (float)y - 0.5f + jitter.y * 0.4f + look.Size.y * 0.5f, 0.0f);
					transform.Scale = glm::vec3(look.Size, 1.0f);
					SpriteRendererComponent& renderer = sprite.GetComponent<SpriteRendererComponent>();
					renderer.Texture = look.Sprite;
					renderer.CastShadows = look.CastsShadow;
					renderer.FlipX = ((hash >> 16) & 1u) != 0;
					renderer.Opacity = 1.0f;
				}
			}
			for (int i = used; i < m_Shown; i++)
			{
				SpriteRendererComponent& renderer = m_Pool[(size_t)i].GetComponent<SpriteRendererComponent>();
				renderer.Opacity = 0.0f;
				renderer.CastShadows = false; // a hidden cut-out sprite keeps its shadow otherwise
			}
			m_Shown = used;
		}

		void Release()
		{
			if (m_Scene != nullptr && m_Root.IsValid())
			{
				m_Scene->DestroyEntity(m_Root); // and the pooled sprites under it
			}
			m_Root = {};
			m_Pool.clear();
			m_Shown = 0;
		}

	private:
		struct Look
		{
			AssetHandle Sprite = 0;
			glm::vec2 Size{ 1.0f }; // in tiles
			bool CastsShadow = true;
		};

		static uint32_t Hash(uint32_t value)
		{
			value ^= value >> 16;
			value *= 0x7FEB352Du;
			value ^= value >> 15;
			value *= 0x846CA68Bu;
			value ^= value >> 16;
			return value;
		}

		Entity PoolSprite(int index)
		{
			while ((int)m_Pool.size() <= index)
			{
				Entity sprite = m_Scene->CreateEntity("Object");
				sprite.AddComponent<TransformComponent>();
				SpriteRendererComponent& renderer = sprite.AddComponent<SpriteRendererComponent>();
				renderer.Facing = SpriteFacing::Upright;
				m_Scene->SetParent(sprite, m_Root, false);
				m_Pool.push_back(sprite);
			}
			return m_Pool[(size_t)index];
		}

		// The tiles (x0, y0, x1, y1) under the primary camera's view, a few tiles wider for tall sprites; the whole map
		// when there is no camera or a corner's ray misses the ground.
		void VisibleTiles(int width, int height, const glm::vec3& centre, glm::ivec4& rect) const
		{
			Entity camera = m_Scene->GetPrimaryCameraEntity();
			if ((bool)camera == false)
			{
				return;
			}
			const glm::mat4 projection = camera.GetComponent<CameraComponent>().Camera.GetProjection(m_Scene->GetViewportAspectRatio());
			const glm::mat4 inverse = glm::inverse(projection * glm::inverse(m_Scene->GetWorldTransform(camera)));
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
					return;
				}
				const glm::vec2 point = BrushRules::MapPoint(glm::vec2(hit.x - centre.x, hit.y - centre.y), width, height);
				low = glm::min(low, point);
				high = glm::max(high, point);
			}
			const int margin = 4;
			rect.x = Clamp((int)low.x - margin, 0, width - 1);
			rect.y = Clamp((int)low.y - margin, 0, height - 1);
			rect.z = Clamp((int)high.x + margin, 0, width - 1);
			rect.w = Clamp((int)high.y + margin, 0, height - 1);
		}

		static int Clamp(int value, int low, int high)
		{
			return value < low ? low : (value > high ? high : value);
		}

		Scene* m_Scene = nullptr;
		Entity m_Root;
		std::vector<Entity> m_Pool;
		std::vector<Look> m_Looks;
		glm::ivec4 m_Rect{ -1 };
		uint64_t m_Revision = ~0ull;
		int m_Shown = 0;
		float m_SinceRefresh = 1.0f;
	};
}
