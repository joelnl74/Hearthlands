#pragma once

#include "Data/ObjectDefinition.h"
#include "State/HeightMap.h"
#include "State/ObjectLayer.h"
#include "Views/ViewArea.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"

#include <cstdint>
#include <vector>

namespace Hydro
{
	// The object layer as upright sprites standing on their tiles, at their height. Only what the camera sees gets a sprite: a
	// pool under one "Objects" entity, reassigned when the view, layer or heights change (at most 10 times a second).
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
				Look& look = m_Looks[(size_t)object.ObjectId];
				look.CastsShadow = object.CastsShadow;
				look.Deciduous = object.Deciduous;
				const float pixelsPerTile = object.PixelsPerTile > 0.0f ? object.PixelsPerTile : 32.0f;
				AddVariant(look, object.Sprite, pixelsPerTile);
				for (AssetHandle variant : object.Variants)
				{
					AddVariant(look, variant, pixelsPerTile);
				}
			}
		}

		// centre: the map's centre in the world (the Terrain entity), its ground at centre.z; heights (nullptr: flat) raise
		// each sprite levelHeight metres a level.
		void Present(const ObjectLayer& layer, int width, int height, const glm::vec3& centre, float deltaTime,
			const HeightMap* heights = nullptr, float levelHeight = 0.0f, const glm::vec3& leafTint = glm::vec3(1.0f))
		{
			const glm::vec3 leaves = glm::round(leafTint * 32.0f) / 32.0f; // Deciduous sprites' colour, in steps
			if (m_Scene == nullptr || m_Root.IsValid() == false || width <= 0 || height <= 0)
			{
				return;
			}
			m_SinceRefresh += deltaTime;
			glm::ivec4 rect(0, 0, width - 1, height - 1);
			// Raised land shows tiles from further down.
			ViewArea::VisibleTiles(*m_Scene, width, height, centre, heights != nullptr ? 12 : 4, rect);
			const uint64_t heightRevision = heights != nullptr ? heights->GetRevision() : 0;
			const bool same = rect == m_Rect && layer.GetRevision() == m_Revision && heightRevision == m_HeightRevision
				&& leaves == m_Leaves;
			if (same || m_SinceRefresh < RefreshInterval)
			{
				return;
			}
			m_SinceRefresh = 0.0f;
			m_Rect = rect;
			m_Revision = layer.GetRevision();
			m_HeightRevision = heightRevision;
			m_Leaves = leaves;

			int used = 0;
			for (int y = rect.y; y <= rect.w && used < MaxSprites; y++)
			{
				for (int x = rect.x; x <= rect.z && used < MaxSprites; x++)
				{
					const size_t index = (size_t)y * (size_t)width + (size_t)x;
					const uint8_t id = layer.Get(index);
					if (id == 0 || m_Looks[id].Variants.empty())
					{
						continue;
					}
					Entity sprite = PoolSprite(used++);
					const uint32_t hash = Hash((uint32_t)index);
					const Look& look = m_Looks[id];
					const Variant& variant = look.Variants[(size_t)((hash >> 17) % (uint32_t)look.Variants.size())];
					const glm::vec2 jitter((float)(hash & 255u) / 255.0f - 0.5f, (float)((hash >> 8) & 255u) / 255.0f - 0.5f);
					TransformComponent& transform = sprite.GetComponent<TransformComponent>();
					// The quad is centred on its position and stands up about its bottom edge: half its height up in Y.
					transform.Translation = centre + glm::vec3((float)x + 0.5f - (float)width * 0.5f + jitter.x * 0.4f,
						(float)height * 0.5f - (float)y - 0.5f + jitter.y * 0.4f + variant.Size.y * 0.5f,
						heights != nullptr ? (float)heights->Get(x, y) * levelHeight : 0.0f);
					transform.Scale = glm::vec3(variant.Size, 1.0f);
					SpriteRendererComponent& renderer = sprite.GetComponent<SpriteRendererComponent>();
					renderer.Texture = variant.Sprite;
					renderer.CastShadows = look.CastsShadow;
					renderer.FlipX = ((hash >> 16) & 1u) != 0;
					renderer.Opacity = 1.0f;
					renderer.Color = look.Deciduous ? glm::vec4(leaves, 1.0f) : glm::vec4(1.0f);
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
		struct Variant
		{
			AssetHandle Sprite = 0;
			glm::vec2 Size{ 1.0f }; // in tiles
		};

		struct Look
		{
			std::vector<Variant> Variants; // a tile picks one by its hash, so it keeps its look
			bool CastsShadow = true;
			bool Deciduous = false;
		};

		static void AddVariant(Look& look, AssetHandle sprite, float pixelsPerTile)
		{
			if (sprite == 0)
			{
				return;
			}
			Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(sprite); // the renderer never loads
			Variant variant;
			variant.Sprite = sprite;
			variant.Size = texture != nullptr
				? glm::vec2((float)texture->GetWidth(), (float)texture->GetHeight()) / pixelsPerTile : glm::vec2(1.0f);
			look.Variants.push_back(variant);
		}

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

		Scene* m_Scene = nullptr;
		Entity m_Root;
		std::vector<Entity> m_Pool;
		std::vector<Look> m_Looks;
		glm::ivec4 m_Rect{ -1 };
		uint64_t m_Revision = ~0ull;
		uint64_t m_HeightRevision = ~0ull;
		glm::vec3 m_Leaves{ -1.0f };
		int m_Shown = 0;
		float m_SinceRefresh = 1.0f;
	};
}
