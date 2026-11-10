#pragma once

#include "Hydro/Core/Assert.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// The world: width x height tile ids (TerrainTypeDefinition::TileId), row by row from the top-left; run state, never entities.
	// Every change moves the revision (the renderer) and is logged (TakeChanges, the simulation); tiles are counted per id.
	class TileMap
	{
	public:
		TileMap() = default;
		TileMap(int width, int height, uint8_t fill = 0)
			: m_Width(width > 0 ? width : 0), m_Height(height > 0 ? height : 0)
		{
			m_Tiles.assign((size_t)m_Width * (size_t)m_Height, fill);
			m_Counts.fill(0);
			m_Counts[fill] = (uint32_t)m_Tiles.size();
		}

		// Copies are fine; assigning is not, because a view compares revisions: replace a held map with Replace.
		TileMap(const TileMap&) = default;
		TileMap(TileMap&&) = default;
		TileMap& operator=(const TileMap&) = delete;
		TileMap& operator=(TileMap&&) = delete;

		int GetWidth() const
		{
			return m_Width;
		}

		int GetHeight() const
		{
			return m_Height;
		}

		uint64_t GetRevision() const
		{
			return m_Revision;
		}

		// One byte per tile, row by row: what the renderer uploads.
		const std::vector<uint8_t>& GetTiles() const
		{
			return m_Tiles;
		}

		// How many tiles hold this id.
		uint32_t GetCount(uint8_t tileId) const
		{
			return m_Counts[tileId];
		}

		bool InBounds(int x, int y) const
		{
			return x >= 0 && y >= 0 && x < m_Width && y < m_Height;
		}

		uint8_t Get(int x, int y) const
		{
			HY_CORE_ASSERT(InBounds(x, y), "TileMap::Get: tile out of bounds");
			return m_Tiles[Index(x, y)];
		}

		// False when the tile is out of bounds or already this id; the revision moves only on a real change.
		bool Set(int x, int y, uint8_t tileId)
		{
			if (InBounds(x, y) == false || m_Tiles[Index(x, y)] == tileId)
			{
				return false;
			}
			m_Counts[m_Tiles[Index(x, y)]]--;
			m_Counts[tileId]++;
			m_Tiles[Index(x, y)] = tileId;
			m_Changes.push_back((uint32_t)Index(x, y));
			m_Revision++;
			return true;
		}

		// Takes another map's size and tiles; the revision ends past both maps', so a view of either sees a change. The
		// change log is cleared: a whole new map is not a list of changes.
		void Replace(const TileMap& other)
		{
			const uint64_t revision = (m_Revision > other.m_Revision ? m_Revision : other.m_Revision) + 1;
			m_Width = other.m_Width;
			m_Height = other.m_Height;
			m_Tiles = other.m_Tiles;
			m_Counts = other.m_Counts;
			m_Changes.clear();
			m_Revision = revision;
		}

		// The tiles (indices, row by row) changed since the last call, oldest first; may repeat a tile.
		std::vector<uint32_t> TakeChanges()
		{
			std::vector<uint32_t> changes;
			changes.swap(m_Changes);
			return changes;
		}

	private:
		size_t Index(int x, int y) const
		{
			return (size_t)y * (size_t)m_Width + (size_t)x;
		}

		int m_Width = 0;
		int m_Height = 0;
		std::vector<uint8_t> m_Tiles;
		std::array<uint32_t, 256> m_Counts{};
		std::vector<uint32_t> m_Changes;
		uint64_t m_Revision = 0;
	};
}
