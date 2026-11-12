#pragma once

#include "Data/TerrainTypeDefinition.h"
#include "State/HeightMap.h"
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
	// map (R id, G priority | level << 4, B role | clue << 2, A moisture) inside a ring of the outside tile, over the plane.
	class TerrainView
	{
	public:
		static constexpr int MaxTypes = 16; // tile_blend.frag's MaxTypes
		static constexpr int OutsideMargin = 1024; // tiles of outside around the map, past what the camera sees
		static constexpr float UploadInterval = 1.0f / 15.0f; // SetData waits for the GPU: at most this often
		// Made textures are memory assets: call Release in OnDestroy.

		void Bind(Entity terrain)
		{
			m_Terrain = terrain;
		}

		// Uploads changes at most every UploadInterval. art: the atlas (0: palette); moisture (nullptr: dry; the soil's quality
		// with showSoil, the groundwater with showGroundwater), heights (flat) and clues per tile 0 to 63 are optional.
		void Present(const TileMap& map, const std::vector<TerrainTypeDefinition>& types, uint8_t outsideTileId, AssetHandle art,
			const std::vector<uint8_t>* moisture, bool showMoisture, float deltaTime, const HeightMap* heights = nullptr,
			const std::vector<uint8_t>* clues = nullptr, uint64_t cluesRevision = 0, bool showSoil = false, bool showGroundwater = false)
		{
			MeshComponent* mesh = m_Terrain.IsValid() ? m_Terrain.TryGetComponent<MeshComponent>() : nullptr;
			if (mesh == nullptr || mesh->Materials.empty() || map.GetWidth() == 0 || map.GetHeight() == 0)
			{
				return;
			}
			if (m_Palette == 0)
			{
				m_Priorities.assign(256, 0);
				m_Roles.assign(256, 0);
				for (const TerrainTypeDefinition& type : types)
				{
					const int id = Clamp(type.TileId, 0, 255);
					m_Priorities[(size_t)id] = (uint8_t)Clamp(type.Priority, 0, 15); // 4 bits: the level takes the other 4
					const bool bed = type.Role == TerrainRole::Riverbed;
					m_Roles[(size_t)id] = type.Role == TerrainRole::Water ? (uint8_t)2 : (bed ? (uint8_t)1 : (uint8_t)0);
				}
				m_Palette = AssetManager::AddMemoryAsset(MakePalette(types));
			}
			if (moisture != nullptr && moisture->size() != map.GetTiles().size())
			{
				moisture = nullptr;
			}
			const std::vector<uint8_t>* levels = heights != nullptr && heights->GetLevels().size() == map.GetTiles().size()
				? &heights->GetLevels() : nullptr;
			const uint64_t heightRevision = levels != nullptr ? heights->GetRevision() : 0;
			clues = clues != nullptr && clues->size() == map.GetTiles().size() ? clues : nullptr;
			cluesRevision = clues != nullptr ? cluesRevision : 0;

			m_SinceUpload += deltaTime;
			const uint32_t width = (uint32_t)map.GetWidth() + 2; // the map and its ring of outside tiles
			const uint32_t height = (uint32_t)map.GetHeight() + 2;
			const bool sized = m_TileTexture != nullptr && m_TileTexture->GetWidth() == width && m_TileTexture->GetHeight() == height;
			const bool wet = moisture != nullptr;
			const bool same = sized && outsideTileId == m_OutsideTileId && wet == m_Wet;
			// Moisture has no revision: compared on upload.
			const bool moved = map.GetRevision() != m_Revision || heightRevision != m_HeightRevision || cluesRevision != m_CluesRevision
				|| wet;
			if (same && (moved == false || m_SinceUpload < UploadInterval))
			{
				return;
			}

			if (same && m_Shown.size() == map.GetTiles().size())
			{
				UploadChanges(map, moisture, levels, clues);
			}
			else if (sized)
			{
				FillPixels(map, outsideTileId, moisture, levels, clues);
				m_TileTexture->SetData(0, 0, width, height, m_Pixels.data());
			}
			else
			{
				RemoveTiles();
				FillPixels(map, outsideTileId, moisture, levels, clues);
				ImageSpecification specification;
				specification.Format = ImageFormat::RGBA; // ids, not colours
				specification.Width = width;
				specification.Height = height;
				m_TileTexture = CreateRef<Texture2D>(specification, m_Pixels.data(), NearestClamp());
				m_Tiles = AssetManager::AddMemoryAsset(m_TileTexture);
			}
			m_Revision = map.GetRevision();
			m_HeightRevision = heightRevision;
			m_ShownLevels = levels != nullptr ? *levels : std::vector<uint8_t>();
			m_CluesRevision = cluesRevision;
			m_ShownClues = clues != nullptr ? *clues : std::vector<uint8_t>();
			m_OutsideTileId = outsideTileId;
			m_Wet = wet;
			m_Shown = map.GetTiles();
			m_ShownMoisture = wet ? *moisture : std::vector<uint8_t>();
			m_SinceUpload = 0.0f;

			MaterialData& material = mesh->Materials[0].Inline;
			material.Albedo = art != 0 ? art : m_Palette;
			material.Emissive = m_Tiles;
			// tile_blend.frag: alpha below 0.5 shows an overlay of the map's alpha: 0 moisture, 0.25 soil, 0.375 groundwater
			// (those given as moisture).
			material.Tint.a = showGroundwater ? 0.375f : (showSoil ? 0.25f : (showMoisture ? 0.0f : 1.0f));
			mesh->Shape = MeshShape::Plane;
			const float margin = 2.0f * (float)OutsideMargin;
			mesh->Size = glm::vec3((float)map.GetWidth() + margin, (float)map.GetHeight() + margin, 1.0f); // 1 tile = 1 m
		}

		// The season's look on the material: the land's colour (Tint); snow on the ground, ice on the water and green fading to
		// straw (tile_blend.frag reads them from the emission colour, never added). In 1/32 steps: a new GPU material per step.
		void SetSeason(const glm::vec3& tint, float groundSnow, float ice, float fade)
		{
			MeshComponent* mesh = m_Terrain.IsValid() ? m_Terrain.TryGetComponent<MeshComponent>() : nullptr;
			if (mesh == nullptr || mesh->Materials.empty())
			{
				return;
			}
			MaterialData& material = mesh->Materials[0].Inline;
			const glm::vec3 season = glm::round(glm::vec3(tint) * 32.0f) / 32.0f;
			const glm::vec3 weather = glm::round(glm::vec3(groundSnow, ice, fade) * 32.0f) / 32.0f;
			if (glm::vec3(material.Tint) != season || material.EmissionColor != weather || material.EmissionIntensity != 1.0f)
			{
				material.Tint = glm::vec4(season, material.Tint.a);
				material.EmissionColor = weather;
				material.EmissionIntensity = 1.0f;
			}
		}

		// Drops the textures and clears the slot, so nothing names a removed handle.
		void Release()
		{
			MeshComponent* mesh = m_Terrain.IsValid() ? m_Terrain.TryGetComponent<MeshComponent>() : nullptr;
			if (mesh != nullptr && mesh->Materials.empty() == false)
			{
				mesh->Materials[0].Inline.Albedo = 0;
				mesh->Materials[0].Inline.Emissive = 0;
				mesh->Materials[0].Inline.Tint = glm::vec4(1.0f);
				mesh->Materials[0].Inline.EmissionColor = glm::vec3(0.0f);
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

		void WriteTexel(uint8_t* texel, uint8_t id, uint8_t moisture, uint8_t level, uint8_t clue) const
		{
			texel[0] = id;
			texel[1] = (uint8_t)(m_Priorities[id] | ((level < 15 ? level : 15) << 4));
			texel[2] = (uint8_t)(m_Roles[id] | ((clue < 63 ? clue : 63) << 2));
			texel[3] = moisture;
		}

		// The map's rows with a one-tile ring of the outside tile around them (texel (x + 1, y + 1) is tile (x, y)).
		void FillPixels(const TileMap& map, uint8_t outsideTileId, const std::vector<uint8_t>* moisture,
			const std::vector<uint8_t>* levels, const std::vector<uint8_t>* clues)
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
					const size_t tile = inside ? (size_t)(y - 1) * (size_t)map.GetWidth() + (size_t)(x - 1) : 0;
					const uint8_t id = inside ? tiles[tile] : outsideTileId;
					const uint8_t wet = inside && moisture != nullptr ? (*moisture)[tile] : (uint8_t)0;
					const uint8_t level = inside && levels != nullptr ? (*levels)[tile] : (uint8_t)0;
					const uint8_t clue = inside && clues != nullptr ? (*clues)[tile] : (uint8_t)0;
					WriteTexel(&m_Pixels[((size_t)y * (size_t)width + (size_t)x) * 4], id, wet, level, clue);
				}
			}
		}

		// Uploads the smallest rectangle holding every tile (moisture, level, clue) that differs from the last upload.
		void UploadChanges(const TileMap& map, const std::vector<uint8_t>* moisture, const std::vector<uint8_t>* levels,
			const std::vector<uint8_t>* clues)
		{
			const int width = map.GetWidth();
			const std::vector<uint8_t>& tiles = map.GetTiles();
			const bool wet = moisture != nullptr && m_ShownMoisture.size() == tiles.size();
			const bool shownLevels = m_ShownLevels.size() == tiles.size(); // else everything was uploaded flat
			const bool shownClues = m_ShownClues.size() == tiles.size();
			int minX = width;
			int minY = map.GetHeight();
			int maxX = -1;
			int maxY = -1;
			for (size_t i = 0; i < tiles.size(); i++)
			{
				const uint8_t level = levels != nullptr ? (*levels)[i] : (uint8_t)0;
				const bool levelMoved = level != (shownLevels ? m_ShownLevels[i] : (uint8_t)0);
				const uint8_t clue = clues != nullptr ? (*clues)[i] : (uint8_t)0;
				const bool clueMoved = clue != (shownClues ? m_ShownClues[i] : (uint8_t)0);
				if (tiles[i] != m_Shown[i] || (wet && (*moisture)[i] != m_ShownMoisture[i]) || levelMoved || clueMoved)
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
					const size_t tile = (size_t)(minY + y) * (size_t)width + (size_t)(minX + x);
					const uint8_t moistureHere = moisture != nullptr ? (*moisture)[tile] : (uint8_t)0;
					const uint8_t level = levels != nullptr ? (*levels)[tile] : (uint8_t)0;
					const uint8_t clue = clues != nullptr ? (*clues)[tile] : (uint8_t)0;
					WriteTexel(&m_Pixels[((size_t)y * (size_t)rectWidth + (size_t)x) * 4], tiles[tile], moistureHere, level, clue);
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
		uint64_t m_HeightRevision = 0;
		uint64_t m_CluesRevision = 0;
		uint8_t m_OutsideTileId = 0;
		bool m_Wet = false;
		float m_SinceUpload = 0.0f;
		std::vector<uint8_t> m_Priorities;
		std::vector<uint8_t> m_Roles;         // per id: 2 water, 1 riverbed, 0 land (the texture's B, low 2 bits)
		std::vector<uint8_t> m_Shown;         // the map's tiles as last uploaded
		std::vector<uint8_t> m_ShownMoisture; // the moisture as last uploaded
		std::vector<uint8_t> m_ShownLevels;   // the heights as last uploaded (empty: flat)
		std::vector<uint8_t> m_ShownClues;    // the groundwater clues as last uploaded (empty: none)
		std::vector<uint8_t> m_Pixels;
	};
}
