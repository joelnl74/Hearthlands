#pragma once

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// The brush's outline on the ground: a flat overlay sprite with a ring texture made here (a memory asset), moved and
	// scaled to the brush. Made by Bind, removed by Release (call it in OnDestroy).
	class BrushCursorView
	{
	public:
		void Bind(Scene& scene)
		{
			m_Scene = &scene;
			m_Ring = AssetManager::AddMemoryAsset(MakeRing());
			m_Cursor = scene.CreateEntity("Brush Cursor"); // only a tag and an ID: the transform is added here
			m_Cursor.AddComponent<TransformComponent>();
			SpriteRendererComponent& sprite = m_Cursor.AddComponent<SpriteRendererComponent>();
			sprite.Texture = m_Ring;
			sprite.Facing = SpriteFacing::Flat;
			sprite.Overlay = true;       // on top, unlit
			sprite.CastShadows = false;
			sprite.Opacity = 0.0f;
		}

		// centre: the brush's centre in the world; radius in tiles; hidden when not shown.
		void Present(bool shown, const glm::vec3& centre, float radius)
		{
			SpriteRendererComponent* sprite = m_Cursor.IsValid() ? m_Cursor.TryGetComponent<SpriteRendererComponent>() : nullptr;
			TransformComponent* transform = m_Cursor.IsValid() ? m_Cursor.TryGetComponent<TransformComponent>() : nullptr;
			if (sprite == nullptr || transform == nullptr)
			{
				return;
			}
			sprite->Opacity = shown ? 1.0f : 0.0f;
			transform->Translation = centre + glm::vec3(0.0f, 0.0f, 0.02f);
			transform->Scale = glm::vec3(radius * 2.0f, radius * 2.0f, 1.0f);
		}

		void Release()
		{
			if (m_Scene != nullptr && m_Cursor.IsValid())
			{
				m_Scene->DestroyEntity(m_Cursor);
			}
			m_Cursor = {};
			if (m_Ring != 0)
			{
				AssetManager::RemoveMemoryAsset(m_Ring);
				m_Ring = 0;
			}
		}

	private:
		// A white pixel ring with a dark outline on both sides, 64 x 64.
		static Ref<Texture2D> MakeRing()
		{
			const int size = 64;
			std::vector<uint8_t> pixels((size_t)size * size * 4, 0);
			for (int y = 0; y < size; y++)
			{
				for (int x = 0; x < size; x++)
				{
					const float dx = (float)x + 0.5f - (float)size * 0.5f;
					const float dy = (float)y + 0.5f - (float)size * 0.5f;
					const float distance = std::sqrt(dx * dx + dy * dy);
					uint8_t* texel = &pixels[((size_t)y * size + (size_t)x) * 4];
					if (distance <= 31.5f && distance >= 28.0f)
					{
						const bool edge = distance > 30.5f || distance < 29.0f;
						const uint8_t value = edge ? (uint8_t)40 : (uint8_t)250;
						texel[0] = value;
						texel[1] = value;
						texel[2] = value;
						texel[3] = 255;
					}
				}
			}
			ImageSpecification specification;
			specification.Format = ImageFormat::SRGB;
			specification.Width = (uint32_t)size;
			specification.Height = (uint32_t)size;
			TextureProperties properties;
			properties.SamplerFilter = TextureFilter::Nearest;
			properties.SamplerWrap = TextureWrap::Clamp;
			return CreateRef<Texture2D>(specification, pixels.data(), properties);
		}

		Scene* m_Scene = nullptr;
		Entity m_Cursor;
		AssetHandle m_Ring = 0;
	};
}
