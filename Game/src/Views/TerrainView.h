#pragma once

#include "Data/TerrainTypeDefinition.h"
#include "State/TileMap.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// Draws the tile map on a Plane running tile_blend.frag: Albedo = the art atlas, else a palette made here; Emissive = the
	// map (R = id, G = priority) inside a ring of the outside tile, which the shader extends over the wider plane.
	class TerrainView
	{
	public:
		static constexpr int MaxTypes = 16; // tile_blend.frag's MaxTypes
		static constexpr int OutsideMargin = 1024; // tiles of outside around the map, past what the camera sees
		// Made textures are memory assets: call Release in OnDestroy.

		void Bind(Entity terrain)
		{
			m_Terrain = terrain;
		}

		// Uploads only when the map's revision or the outside tile moved, and then only the rectangle of changed tiles; a size
		// change makes a new tile texture. The types are read once. art: the atlas texture asset, 0 for the flat palette.
		void Present(const TileMap& map, const std::vector<TerrainTypeDefinition>& types, uint8_t outsideTileId, AssetHandle art)
		{
			MeshComponent* mesh = m_Terrain.IsValid() ? m_Terrain.TryGetComponent<MeshComponent>() : nullptr;
			if (mesh == nullptr || mesh->Materials.empty() || map.GetWidth() == 0 || map.GetHeight() == 0)
			{
				return;
			}
			if (m_Palette == 0)
			{
				m_Priorities.assign(256, 0);
				for (const TerrainTypeDefinition& type : types)
				{
					m_Priorities[Clamp(type.TileId, 0, 255)] = (uint8_t)Clamp(type.Priority, 0, 255);
				}
				m_Palette = AssetManager::AddMemoryAsset(MakePalette(types));
			}

			const uint32_t width = (uint32_t)map.GetWidth() + 2;   // the map and its ring of outside tiles
			const uint32_t height = (uint32_t)map.GetHeight() + 2;
			const bool sized = m_TileTexture != nullptr && m_TileTexture->GetWidth() == width && m_TileTexture->GetHeight() == height;
			if (sized && map.GetRevision() == m_Revision && outsideTileId == m_OutsideTileId)
			{
				return;
			}
			if (sized && outsideTileId == m_OutsideTileId && m_Shown.size() == map.GetTiles().size())
			{
				UploadChanges(map);
			}
			else if (sized)
			{
				FillPixels(map, outsideTileId);
				m_TileTexture->SetData(0, 0, width, height, m_Pixels.data());
			}
			else
			{
				RemoveTiles();
				FillPixels(map, outsideTileId);
				ImageSpecification specification;
				specification.Format = ImageFormat::RGBA; // ids, not colours
				specification.Width = width;
				specification.Height = height;
				m_TileTexture = CreateRef<Texture2D>(specification, m_Pixels.data(), NearestClamp());
				m_Tiles = AssetManager::AddMemoryAsset(m_TileTexture);
			}
			m_Revision = map.GetRevision();
			m_OutsideTileId = outsideTileId;
			m_Shown = map.GetTiles();

			MaterialData& material = mesh->Materials[0].Inline;
			material.Albedo = art != 0 ? art : m_Palette;
			material.Emissive = m_Tiles;
			mesh->Shape = MeshShape::Plane;
			const float margin = 2.0f * (float)OutsideMargin;
			mesh->Size = glm::vec3((float)map.GetWidth() + margin, (float)map.GetHeight() + margin, 1.0f); // 1 tile = 1 m
		}

		// Drops the textures and clears the slot, so nothing names a removed handle.
		void Release()
		{
			MeshComponent* mesh = m_Terrain.IsValid() ? m_Terrain.TryGetComponent<MeshComponent>() : nullptr;
			if (mesh != nullptr && mesh->Materials.empty() == false)
			{
				mesh->Materials[0].Inline.Albedo = 0;
				mesh->Materials[0].Inline.Emissive = 0;
			}
			RemoveTiles();
			if (m_Palette != 0)
			{
				AssetManager::RemoveMemoryAsset(m_Palette);
				m_Palette = 0;
			}
		}

	private:
		static int Clamp(int value, int low, int high)
		{
			return value < low ? low : (value > high ? high : value);
		}

		static uint8_t ToByte(float value)
		{
			const float clamped = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
			return (uint8_t)std::lround(clamped * 255.0f);
		}

		static TextureProperties NearestClamp()
		{
			TextureProperties properties;
			properties.SamplerFilter = TextureFilter::Nearest;
			properties.SamplerWrap = TextureWrap::Clamp;
			return properties;
		}

		// One texel per tile id up to the highest used (sRGB, the colours as picked); ids without a type are magenta.
		static Ref<Texture2D> MakePalette(const std::vector<TerrainTypeDefinition>& types)
		{
			int count = 1;
			for (const TerrainTypeDefinition& type : types)
			{
				count = type.TileId + 1 > count ? type.TileId + 1 : count;
			}
			count = Clamp(count, 1, MaxTypes);
			std::vector<uint8_t> pixels((size_t)count * 4, 255);
			for (int id = 0; id < count; id++)
			{
				pixels[(size_t)id * 4 + 1] = 0;
			}
			for (const TerrainTypeDefinition& type : types)
			{
				if (type.TileId >= 0 && type.TileId < count)
				{
					uint8_t* texel = &pixels[(size_t)type.TileId * 4];
					texel[0] = ToByte(type.Color.r);
					texel[1] = ToByte(type.Color.g);
					texel[2] = ToByte(type.Color.b);
				}
			}
			ImageSpecification specification;
			specification.Format = ImageFormat::SRGB;
			specification.Width = (uint32_t)count;
			specification.Height = 1;
			return CreateRef<Texture2D>(specification, pixels.data(), NearestClamp());
		}

		// The map's rows with a one-tile ring of the outside tile around them (texel (x + 1, y + 1) is tile (x, y)).
		void FillPixels(const TileMap& map, uint8_t outsideTileId)
		{
			const int width = map.GetWidth() + 2;
			const int height = map.GetHeight() + 2;
			const std::vector<uint8_t>& tiles = map.GetTiles();
			m_Pixels.resize((size_t)width * (size_t)height * 4);
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					const bool inside = x > 0 && y > 0 && x < width - 1 && y < height - 1;
					const uint8_t id = inside ? tiles[(size_t)(y - 1) * (size_t)map.GetWidth() + (size_t)(x - 1)] : outsideTileId;
					uint8_t* texel = &m_Pixels[((size_t)y * (size_t)width + (size_t)x) * 4];
					texel[0] = id;
					texel[1] = m_Priorities[id];
					texel[2] = 0;
					texel[3] = 255;
				}
			}
		}

		// Uploads the smallest rectangle holding every tile that differs from what was last uploaded (m_Shown).
		void UploadChanges(const TileMap& map)
		{
			const int width = map.GetWidth();
			const std::vector<uint8_t>& tiles = map.GetTiles();
			int minX = width;
			int minY = map.GetHeight();
			int maxX = -1;
			int maxY = -1;
			for (size_t i = 0; i < tiles.size(); i++)
			{
				if (tiles[i] != m_Shown[i])
				{
					const int x = (int)(i % (size_t)width);
					const int y = (int)(i / (size_t)width);
					minX = x < minX ? x : minX;
					minY = y < minY ? y : minY;
					maxX = x > maxX ? x : maxX;
					maxY = y > maxY ? y : maxY;
				}
			}
			if (maxX < 0)
			{
				return;
			}
			const int rectWidth = maxX - minX + 1;
			const int rectHeight = maxY - minY + 1;
			m_Pixels.resize((size_t)rectWidth * (size_t)rectHeight * 4);
			for (int y = 0; y < rectHeight; y++)
			{
				for (int x = 0; x < rectWidth; x++)
				{
					const uint8_t id = tiles[(size_t)(minY + y) * (size_t)width + (size_t)(minX + x)];
					uint8_t* texel = &m_Pixels[((size_t)y * (size_t)rectWidth + (size_t)x) * 4];
					texel[0] = id;
					texel[1] = m_Priorities[id];
					texel[2] = 0;
					texel[3] = 255;
				}
			}
			// Texel (x + 1, y + 1) is tile (x, y): the ring of outside tiles comes first.
			m_TileTexture->SetData((uint32_t)minX + 1, (uint32_t)minY + 1, (uint32_t)rectWidth, (uint32_t)rectHeight, m_Pixels.data());
		}

		void RemoveTiles()
		{
			if (m_Tiles != 0)
			{
				AssetManager::RemoveMemoryAsset(m_Tiles);
				m_Tiles = 0;
			}
			m_TileTexture = nullptr;
		}

		Entity m_Terrain;
		AssetHandle m_Palette = 0;
		AssetHandle m_Tiles = 0;
		Ref<Texture2D> m_TileTexture;
		uint64_t m_Revision = 0;
		uint8_t m_OutsideTileId = 0;
		std::vector<uint8_t> m_Priorities;
		std::vector<uint8_t> m_Shown; // the map's tiles as last uploaded
		std::vector<uint8_t> m_Pixels;
	};
}
