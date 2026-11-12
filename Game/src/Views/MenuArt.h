#pragma once

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/Components/Components.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>

namespace Hydro
{
	// The game UI's look: the sprites of Assets/Textures/UI/MenuArt.png (scripts/art/make_ui_art.py) with their nine-slice
	// borders, icons, brush pictures (Brush<TileId>) and the palette. Without the atlas the sprites fall back to flat colours.
	class MenuArt
	{
	public:
		enum class Sprite : uint8_t
		{
			Panel, Tile, TileGreen, Select, Parchment, ParchmentHeader, ButtonPurple, ButtonDark, ButtonBlue, NodeLocked, NodeOpen,
			NodeOwned, NodeSelect, Ribbon, BarTrack, BarFill, BarFillGold, Ornament, Diamond, DiamondBlue, ArtSlot, Medallion, Check,
			Lock, IconPeople, IconTent, IconFood, IconWood, IconHouse, IconHourglass, IconFire, IconTrophy, IconLeaf, IconDrop,
			IconSprout, Count
		};

		void Load(AssetHandle atlas)
		{
			m_Atlas = 0;
			const Ref<Texture2D> texture = atlas != 0 ? AssetManager::GetAsset<Texture2D>(atlas) : nullptr; // the renderer never loads
			m_Texture = texture;
			if (texture == nullptr)
			{
				return;
			}
			m_Atlas = atlas;
			for (int i = 0; i < (int)Sprite::Count; i++)
			{
				m_Index[i] = texture->FindSprite(Info((Sprite)i).Name);
			}
		}

		// The sprite on `image`, tinted by `tint`; a flat colour when the atlas or the sprite is missing.
		void Apply(UIImageComponent& image, Sprite sprite, const glm::vec4& tint = glm::vec4(1.0f)) const
		{
			const SpriteInfo info = Info(sprite);
			const int32_t index = m_Index[(int)sprite];
			if (m_Atlas == 0 || index < 0)
			{
				image.Texture = 0;
				image.NineSlice = glm::vec4(0.0f);
				image.Color = info.Fallback * tint;
				return;
			}
			image.Texture = m_Atlas;
			image.SpriteIndex = (uint32_t)index;
			image.NineSlice = info.Border;
			image.Color = tint;
		}

		// A sprite looked up by name (a brush picture: "Brush<TileId>"); `fallback` as a flat colour when it isn't there.
		void ApplyNamed(UIImageComponent& image, const std::string& name, const glm::vec4& fallback) const
		{
			const int32_t index = m_Texture != nullptr ? m_Texture->FindSprite(name) : -1;
			image.NineSlice = glm::vec4(0.0f);
			if (index < 0)
			{
				image.Texture = 0;
				image.Color = fallback;
				return;
			}
			image.Texture = m_Atlas;
			image.SpriteIndex = (uint32_t)index;
			image.Color = glm::vec4(1.0f);
		}

		// The palette (linear colours as the UI draws them).
		static glm::vec4 Gold()
		{
			return glm::vec4(0.97f, 0.83f, 0.48f, 1.0f);
		}

		static glm::vec4 Ink()
		{
			return glm::vec4(0.96f, 0.93f, 0.85f, 1.0f);
		}

		static glm::vec4 Muted()
		{
			return glm::vec4(0.68f, 0.71f, 0.80f, 1.0f);
		}

		static glm::vec4 Faint()
		{
			return glm::vec4(0.45f, 0.47f, 0.55f, 1.0f);
		}

		static glm::vec4 DarkInk()
		{
			return glm::vec4(0.23f, 0.16f, 0.08f, 1.0f);
		}

		static glm::vec4 DarkMuted()
		{
			return glm::vec4(0.42f, 0.33f, 0.21f, 1.0f);
		}

		static glm::vec4 Good()
		{
			return glm::vec4(0.50f, 0.86f, 0.44f, 1.0f);
		}

		static glm::vec4 DarkGood()
		{
			return glm::vec4(0.16f, 0.45f, 0.14f, 1.0f);
		}

		static glm::vec4 DarkBad()
		{
			return glm::vec4(0.62f, 0.20f, 0.14f, 1.0f);
		}

		static glm::vec4 Sky()
		{
			return glm::vec4(0.56f, 0.83f, 1.0f, 1.0f);
		}

		static glm::vec4 Backdrop()
		{
			return glm::vec4(0.02f, 0.03f, 0.07f, 0.72f);
		}

		static glm::vec4 Band()
		{
			return glm::vec4(0.0f, 0.0f, 0.0f, 0.28f);
		}

	private:
		struct SpriteInfo
		{
			const char* Name = "";
			glm::vec4 Border{ 0.0f }; // left, bottom, right, top in atlas pixels (make_ui_art.py's radii and rims)
			glm::vec4 Fallback{ 1.0f };
		};

		static SpriteInfo Info(Sprite sprite)
		{
			const glm::vec4 navy(0.08f, 0.10f, 0.20f, 0.95f);
			switch (sprite)
			{
			case Sprite::Panel: return { "Panel", glm::vec4(16.0f), navy };
			case Sprite::Tile: return { "Tile", glm::vec4(18.0f), navy };
			case Sprite::TileGreen: return { "TileGreen", glm::vec4(18.0f), glm::vec4(0.08f, 0.20f, 0.10f, 0.95f) };
			case Sprite::Select: return { "Select", glm::vec4(26.0f), glm::vec4(0.47f, 0.84f, 1.0f, 0.25f) };
			case Sprite::Parchment: return { "Parchment", glm::vec4(22.0f), glm::vec4(0.91f, 0.85f, 0.70f, 1.0f) };
			case Sprite::ParchmentHeader: return { "ParchmentHeader", glm::vec4(10.0f), glm::vec4(0.76f, 0.68f, 0.52f, 1.0f) };
			case Sprite::ButtonPurple: return { "ButtonPurple", glm::vec4(16.0f), glm::vec4(0.35f, 0.22f, 0.70f, 1.0f) };
			case Sprite::ButtonDark: return { "ButtonDark", glm::vec4(12.0f), glm::vec4(0.12f, 0.14f, 0.24f, 1.0f) };
			case Sprite::ButtonBlue: return { "ButtonBlue", glm::vec4(12.0f), glm::vec4(0.18f, 0.40f, 0.80f, 1.0f) };
			case Sprite::NodeLocked: return { "NodeLocked", glm::vec4(32.0f, 31.0f, 32.0f, 31.0f), glm::vec4(0.11f, 0.12f, 0.16f, 1.0f) };
			case Sprite::NodeOpen: return { "NodeOpen", glm::vec4(32.0f, 31.0f, 32.0f, 31.0f), glm::vec4(0.12f, 0.20f, 0.36f, 1.0f) };
			case Sprite::NodeOwned: return { "NodeOwned", glm::vec4(32.0f, 31.0f, 32.0f, 31.0f), glm::vec4(0.36f, 0.27f, 0.11f, 1.0f) };
			case Sprite::NodeSelect: return { "NodeSelect", glm::vec4(32.0f, 31.0f, 32.0f, 31.0f), glm::vec4(0.47f, 0.84f, 1.0f, 0.25f) };
			case Sprite::Ribbon: return { "Ribbon", glm::vec4(10.0f), glm::vec4(0.35f, 0.22f, 0.70f, 1.0f) };
			case Sprite::BarTrack: return { "BarTrack", glm::vec4(8.0f, 7.0f, 8.0f, 7.0f), glm::vec4(0.04f, 0.05f, 0.10f, 1.0f) };
			case Sprite::BarFill: return { "BarFill", glm::vec4(8.0f, 7.0f, 8.0f, 7.0f), glm::vec4(0.35f, 0.70f, 1.0f, 1.0f) };
			case Sprite::BarFillGold: return { "BarFillGold", glm::vec4(8.0f, 7.0f, 8.0f, 7.0f), glm::vec4(0.85f, 0.70f, 0.35f, 1.0f) };
			case Sprite::Ornament: return { "Ornament", glm::vec4(0.0f), glm::vec4(0.85f, 0.70f, 0.35f, 0.6f) };
			case Sprite::Diamond: return { "Diamond", glm::vec4(0.0f), glm::vec4(0.85f, 0.70f, 0.35f, 1.0f) };
			case Sprite::DiamondBlue: return { "DiamondBlue", glm::vec4(0.0f), glm::vec4(0.47f, 0.84f, 1.0f, 1.0f) };
			case Sprite::ArtSlot: return { "ArtSlot", glm::vec4(0.0f), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f) };
			case Sprite::Medallion: return { "Medallion", glm::vec4(0.0f), glm::vec4(0.85f, 0.70f, 0.35f, 1.0f) };
			case Sprite::Check: return { "Check", glm::vec4(0.0f), glm::vec4(0.30f, 0.66f, 0.28f, 1.0f) };
			case Sprite::Lock: return { "Lock", glm::vec4(0.0f), glm::vec4(0.85f, 0.70f, 0.35f, 1.0f) };
			case Sprite::IconPeople: return { "IconPeople", glm::vec4(0.0f), glm::vec4(0.85f, 0.70f, 0.35f, 1.0f) };
			case Sprite::IconTent: return { "IconTent", glm::vec4(0.0f), glm::vec4(0.85f, 0.70f, 0.35f, 1.0f) };
			case Sprite::IconFood: return { "IconFood", glm::vec4(0.0f), glm::vec4(0.78f, 0.20f, 0.17f, 1.0f) };
			case Sprite::IconWood: return { "IconWood", glm::vec4(0.0f), glm::vec4(0.55f, 0.36f, 0.20f, 1.0f) };
			case Sprite::IconHouse: return { "IconHouse", glm::vec4(0.0f), glm::vec4(0.85f, 0.70f, 0.35f, 1.0f) };
			case Sprite::IconHourglass: return { "IconHourglass", glm::vec4(0.0f), glm::vec4(0.85f, 0.70f, 0.35f, 1.0f) };
			case Sprite::IconFire: return { "IconFire", glm::vec4(0.0f), glm::vec4(0.95f, 0.55f, 0.15f, 1.0f) };
			case Sprite::IconTrophy: return { "IconTrophy", glm::vec4(0.0f), glm::vec4(0.85f, 0.70f, 0.35f, 1.0f) };
			case Sprite::IconLeaf: return { "IconLeaf", glm::vec4(0.0f), glm::vec4(0.43f, 0.77f, 0.31f, 1.0f) };
			case Sprite::IconDrop: return { "IconDrop", glm::vec4(0.0f), glm::vec4(0.27f, 0.59f, 0.92f, 1.0f) };
			case Sprite::IconSprout: return { "IconSprout", glm::vec4(0.0f), glm::vec4(0.43f, 0.78f, 0.31f, 1.0f) };
			default: return {};
			}
		}

		AssetHandle m_Atlas = 0;
		Ref<Texture2D> m_Texture; // for sprites looked up by name
		int32_t m_Index[(int)Sprite::Count] = {};
	};
}
