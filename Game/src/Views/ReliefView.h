#pragma once

#include "Data/TerrainTypeDefinition.h"
#include "State/HeightMap.h"
#include "State/TileMap.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Mesh.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Scene.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Hydro
{
	// The raised land as stepped blocks: a mesh (memory asset) per chunk of tops in the Terrain plane's material (UV = tile, so
	// they blend like the ground) and walls in the types' CliffColor; changed chunks are rebuilt a few at a time.
	class ReliefView
	{
	public:
		static constexpr int ChunkTiles = 32;
		static constexpr float RefreshInterval = 0.1f;
		static constexpr int MaxChunksPerRefresh = 16;

		// Made by Bind, removed by Release (call it in OnDestroy). terrain: the entity whose first material slot the tops copy.
		void Bind(Scene& scene, Entity terrain, const std::vector<TerrainTypeDefinition>& types)
		{
			m_Scene = &scene;
			m_Terrain = terrain;
			m_Root = scene.CreateEntity("Relief"); // only a tag and an ID
			m_Root.AddComponent<TransformComponent>();
			m_Cliffs.assign(256, glm::vec3(0.45f, 0.36f, 0.27f));
			for (const TerrainTypeDefinition& type : types)
			{
				if (type.TileId >= 0 && type.TileId <= 255)
				{
					m_Cliffs[(size_t)type.TileId] = glm::pow(glm::clamp(type.CliffColor, 0.0f, 1.0f), glm::vec3(2.2f)); // picked in sRGB
				}
			}
		}

		// centre: the map's centre in the world (the Terrain entity); levelHeight: metres a level stands.
		void Present(const HeightMap& heights, const TileMap& map, const glm::vec3& centre, float levelHeight, float deltaTime)
		{
			if (m_Scene == nullptr || m_Root.IsValid() == false || map.GetWidth() == 0
				|| heights.GetLevels().size() != map.GetTiles().size())
			{
				return;
			}
			m_Root.GetComponent<TransformComponent>().Translation = centre;
			SyncTopMaterial();
			m_SinceRefresh += deltaTime;
			const bool moved = heights.GetRevision() != m_HeightRevision || map.GetRevision() != m_MapRevision
				|| levelHeight != m_LevelHeight;
			if ((moved == false && m_Pending == 0) || m_SinceRefresh < RefreshInterval)
			{
				return;
			}
			m_SinceRefresh = 0.0f;
			if (moved)
			{
				MarkChanges(heights, map, levelHeight);
			}

			int built = 0;
			for (size_t i = 0; i < m_Dirty.size() && built < MaxChunksPerRefresh; i++)
			{
				if (m_Dirty[i] != 0)
				{
					m_Dirty[i] = 0;
					m_Pending--;
					Build((int)(i % (size_t)m_ChunksX), (int)(i / (size_t)m_ChunksX), heights, map, levelHeight);
					built++;
				}
			}
		}

		void Release()
		{
			for (Chunk& chunk : m_Chunks)
			{
				if (chunk.Model != 0)
				{
					AssetManager::RemoveMemoryAsset(chunk.Model);
				}
			}
			if (m_Scene != nullptr && m_Root.IsValid())
			{
				m_Scene->DestroyEntity(m_Root); // and the chunks under it
			}
			m_Root = {};
			m_Chunks.clear();
			m_Dirty.clear();
			m_Pending = 0;
		}

	private:
		struct Chunk
		{
			Entity Holder;        // made on the chunk's first raised tile
			AssetHandle Model = 0; // 0 while the chunk has no raised tile
		};

		// Marks the chunks whose tiles or heights differ from the last look (and, for heights at a chunk's edge, the chunk
		// next to it, whose walls face them). A new map size or level height marks every chunk.
		void MarkChanges(const HeightMap& heights, const TileMap& map, float levelHeight)
		{
			const int width = map.GetWidth();
			const int height = map.GetHeight();
			const int chunksX = (width + ChunkTiles - 1) / ChunkTiles;
			const int chunksY = (height + ChunkTiles - 1) / ChunkTiles;
			const bool resized = chunksX != m_ChunksX || chunksY != m_ChunksY || m_ShownLevels.size() != heights.GetLevels().size();
			if (resized)
			{
				Release();
				m_Root = m_Scene->CreateEntity("Relief");
				m_Root.AddComponent<TransformComponent>();
				m_ChunksX = chunksX;
				m_ChunksY = chunksY;
				m_Chunks.assign((size_t)chunksX * (size_t)chunksY, Chunk{});
				m_Dirty.assign(m_Chunks.size(), 0);
				m_ShownLevels.assign(heights.GetLevels().size(), 0);
				m_ShownTiles.assign(map.GetTiles().size(), 0);
			}
			const bool all = resized || levelHeight != m_LevelHeight;
			const std::vector<uint8_t>& levels = heights.GetLevels();
			const std::vector<uint8_t>& tiles = map.GetTiles();
			for (size_t i = 0; i < levels.size(); i++)
			{
				const bool raised = levels[i] != m_ShownLevels[i];
				const bool painted = tiles[i] != m_ShownTiles[i] && levels[i] > 0; // a wall's colour is its tile's type
				if (all == false && raised == false && painted == false)
				{
					continue;
				}
				const int x = (int)(i % (size_t)width);
				const int y = (int)(i / (size_t)width);
				if (all && levels[i] == 0 && m_ShownLevels[i] == 0)
				{
					continue; // flat before and after: nothing to draw for it
				}
				Mark(x / ChunkTiles, y / ChunkTiles);
				if (raised || all)
				{
					Mark((x - 1) / ChunkTiles, y / ChunkTiles);
					Mark((x + 1) / ChunkTiles, y / ChunkTiles);
					Mark(x / ChunkTiles, (y - 1) / ChunkTiles);
					Mark(x / ChunkTiles, (y + 1) / ChunkTiles);
				}
			}
			m_ShownLevels = levels;
			m_ShownTiles = tiles;
			m_HeightRevision = heights.GetRevision();
			m_MapRevision = map.GetRevision();
			m_LevelHeight = levelHeight;
		}

		void Mark(int cx, int cy)
		{
			if (cx < 0 || cy < 0 || cx >= m_ChunksX || cy >= m_ChunksY)
			{
				return;
			}
			uint8_t& dirty = m_Dirty[(size_t)cy * (size_t)m_ChunksX + (size_t)cx];
			m_Pending += dirty == 0 ? 1 : 0;
			dirty = 1;
		}

		// The chunk's mesh again from the current heights: tops (slot 0) and walls (slot 1); none when nothing is raised.
		void Build(int cx, int cy, const HeightMap& heights, const TileMap& map, float levelHeight)
		{
			Chunk& chunk = m_Chunks[(size_t)cy * (size_t)m_ChunksX + (size_t)cx];
			MeshData tops;
			MeshData walls;
			const int width = map.GetWidth();
			const int height = map.GetHeight();
			const int x0 = cx * ChunkTiles;
			const int y0 = cy * ChunkTiles;
			const int x1 = x0 + ChunkTiles < width ? x0 + ChunkTiles : width;
			const int y1 = y0 + ChunkTiles < height ? y0 + ChunkTiles : height;
			for (int y = y0; y < y1; y++)
			{
				int x = x0;
				while (x < x1)
				{
					const int level = heights.Get(x, y);
					int end = x + 1;
					while (end < x1 && heights.Get(end, y) == level)
					{
						end++;
					}
					if (level > 0)
					{
						AddTop(tops, x, end, y, (float)level * levelHeight, width, height); // a run of one level: one quad
					}
					x = end;
				}
				for (int wx = x0; wx < x1; wx++)
				{
					const int level = heights.Get(wx, y);
					if (level > 0)
					{
						AddWalls(walls, heights, wx, y, level, m_Cliffs[map.Get(wx, y)], levelHeight, width, height);
					}
				}
			}

			if (chunk.Model != 0)
			{
				AssetManager::RemoveMemoryAsset(chunk.Model); // its buffers go through the renderer's deletion queue
				chunk.Model = 0;
			}
			if (tops.Indices.empty())
			{
				if (chunk.Holder.IsValid())
				{
					m_Scene->DestroyEntity(chunk.Holder); // Model 0 would draw the built-in box
				}
				chunk.Holder = {};
				return;
			}

			MeshData data = tops;
			SubMesh topPart;
			topPart.IndexCount = (uint32_t)tops.Indices.size();
			data.SubMeshes.push_back(topPart);
			if (walls.Indices.empty() == false)
			{
				const uint32_t base = (uint32_t)data.Vertices.size();
				SubMesh wallPart;
				wallPart.FirstIndex = (uint32_t)data.Indices.size();
				wallPart.IndexCount = (uint32_t)walls.Indices.size();
				wallPart.MaterialSlot = 1;
				data.Vertices.insert(data.Vertices.end(), walls.Vertices.begin(), walls.Vertices.end());
				for (uint32_t index : walls.Indices)
				{
					data.Indices.push_back(base + index);
				}
				data.SubMeshes.push_back(wallPart);
			}
			chunk.Model = AssetManager::AddMemoryAsset(CreateRef<Mesh>(data, "Relief " + std::to_string(cx) + "," + std::to_string(cy)));

			if (chunk.Holder.IsValid() == false)
			{
				chunk.Holder = m_Scene->CreateEntity("ReliefChunk");
				chunk.Holder.AddComponent<TransformComponent>();
				MeshComponent& mesh = chunk.Holder.AddComponent<MeshComponent>();
				MeshMaterialSlot wallSlot;
				wallSlot.Inline.Filter = TextureFilter::Nearest; // Standard shader, white: the walls' vertex colours
				mesh.Materials = { MeshMaterialSlot{}, wallSlot };
				mesh.Materials[0].Inline = m_TopMaterial;
				mesh.CastShadows = true;
				m_Scene->SetParent(chunk.Holder, m_Root, false);
			}
			chunk.Holder.GetComponent<MeshComponent>().Model = chunk.Model;
		}

		// Tiles [x, end) of row y at height z: the map's space is centred, x right, y up (row 0 at the top), 1 m tiles; the
		// UV is (x, -y) like the Terrain plane's, so tile_blend.frag finds the same tile.
		static void AddTop(MeshData& mesh, int x, int end, int y, float z, int width, int height)
		{
			const float left = (float)x - (float)width * 0.5f;
			const float right = (float)end - (float)width * 0.5f;
			const float top = (float)height * 0.5f - (float)y;
			const float bottom = top - 1.0f;
			const glm::vec3 corners[4] = { { left, bottom, z }, { right, bottom, z }, { right, top, z }, { left, top, z } };
			const uint32_t first = (uint32_t)mesh.Vertices.size();
			for (const glm::vec3& corner : corners)
			{
				MeshVertex vertex;
				vertex.Position = corner;
				vertex.Normal = glm::vec3(0.0f, 0.0f, 1.0f);
				vertex.UV = glm::vec2(corner.x, -corner.y);
				mesh.Vertices.push_back(vertex);
			}
			AddQuadIndices(mesh, first);
		}

		// The tile's cliff faces down to each lower side neighbour (off the map counts as 0), a band per level: alternate
		// bands a shade darker, the lowest darker still, so the steps read.
		static void AddWalls(MeshData& mesh, const HeightMap& heights, int x, int y, int level, const glm::vec3& cliff,
			float levelHeight, int width, int height)
		{
			const float left = (float)x - (float)width * 0.5f;
			const float right = left + 1.0f;
			const float top = (float)height * 0.5f - (float)y;
			const float bottom = top - 1.0f;
			const int below[4] = { heights.Get(x, y + 1), heights.Get(x, y - 1), heights.Get(x + 1, y), heights.Get(x - 1, y) };
			// Each side's bottom edge, left to right seen from outside (so its quad winds counter-clockwise), and its normal:
			// front (-Y), back (+Y), right (+X), left (-X), like MeshFactory's blocks.
			const glm::vec2 from[4] = { { left, bottom }, { right, top }, { right, bottom }, { left, top } };
			const glm::vec2 to[4] = { { right, bottom }, { left, top }, { right, top }, { left, bottom } };
			const glm::vec3 normals[4] = { { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } };
			for (int side = 0; side < 4; side++)
			{
				for (int band = below[side]; band < level; band++)
				{
					const float shade = (band % 2 == 0 ? 1.0f : 0.88f) * (band == below[side] ? 0.85f : 1.0f);
					AddWall(mesh, from[side], to[side], (float)band * levelHeight, (float)(band + 1) * levelHeight, normals[side],
						glm::vec4(cliff * shade, 1.0f));
				}
			}
		}

		static void AddWall(MeshData& mesh, const glm::vec2& from, const glm::vec2& to, float z0, float z1, const glm::vec3& normal,
			const glm::vec4& colour)
		{
			const glm::vec3 corners[4] = { { from, z0 }, { to, z0 }, { to, z1 }, { from, z1 } };
			const uint32_t first = (uint32_t)mesh.Vertices.size();
			for (const glm::vec3& corner : corners)
			{
				MeshVertex vertex;
				vertex.Position = corner;
				vertex.Normal = normal;
				vertex.Color = colour;
				mesh.Vertices.push_back(vertex);
			}
			AddQuadIndices(mesh, first);
		}

		static void AddQuadIndices(MeshData& mesh, uint32_t first)
		{
			const uint32_t quad[6] = { 0, 1, 2, 2, 3, 0 };
			for (uint32_t index : quad)
			{
				mesh.Indices.push_back(first + index);
			}
		}

		// The tops draw with the Terrain plane's material (its atlas, tile texture and overlay switch): copied when it changes.
		void SyncTopMaterial()
		{
			const MeshComponent* plane = m_Terrain.IsValid() ? m_Terrain.TryGetComponent<MeshComponent>() : nullptr;
			if (plane == nullptr || plane->Materials.empty() || plane->Materials[0].Inline == m_TopMaterial)
			{
				return;
			}
			m_TopMaterial = plane->Materials[0].Inline;
			for (Chunk& chunk : m_Chunks)
			{
				if (chunk.Holder.IsValid())
				{
					chunk.Holder.GetComponent<MeshComponent>().Materials[0].Inline = m_TopMaterial;
				}
			}
		}

		Scene* m_Scene = nullptr;
		Entity m_Terrain;
		Entity m_Root;
		std::vector<Chunk> m_Chunks;
		std::vector<uint8_t> m_Dirty;
		int m_Pending = 0;
		int m_ChunksX = 0;
		int m_ChunksY = 0;
		std::vector<glm::vec3> m_Cliffs; // per tile id, linear
		std::vector<uint8_t> m_ShownLevels;
		std::vector<uint8_t> m_ShownTiles;
		MaterialData m_TopMaterial;
		uint64_t m_HeightRevision = ~0ull;
		uint64_t m_MapRevision = ~0ull;
		float m_LevelHeight = -1.0f;
		float m_SinceRefresh = 1.0f;
	};
}
