#pragma once

#include <cstdint>
#include <vector>

namespace Hydro
{
	// The land's height in whole levels per tile, row by row like TileMap's tiles: 0 is the ground the water lies on, raised
	// land 1 and up (the Raise brush, generated hills). Run state; every change moves the revision and is logged (TakeChanges).
	class HeightMap
	{
	public:
		HeightMap() = default;
		HeightMap(int width, int height, uint8_t fill = 0)
			: m_Width(width > 0 ? width : 0), m_Height(height > 0 ? height : 0)
		{
			m_Levels.assign((size_t)m_Width * (size_t)m_Height, fill);
		}

		// Copies are fine; assigning is not, because a view compares revisions: replace a held map with Replace.
		HeightMap(const HeightMap&) = default;
		HeightMap(HeightMap&&) = default;
		HeightMap& operator=(const HeightMap&) = delete;
		HeightMap& operator=(HeightMap&&) = delete;

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

		const std::vector<uint8_t>& GetLevels() const
		{
			return m_Levels;
		}

		bool InBounds(int x, int y) const
		{
			return x >= 0 && y >= 0 && x < m_Width && y < m_Height;
		}

		// 0 outside the map, so the sea around it is ground level.
		uint8_t Get(int x, int y) const
		{
			return InBounds(x, y) ? m_Levels[(size_t)y * (size_t)m_Width + (size_t)x] : (uint8_t)0;
		}

		// False when the tile is out of bounds or already at this level; the revision moves only on a real change.
		bool Set(int x, int y, uint8_t level)
		{
			if (InBounds(x, y) == false || m_Levels[(size_t)y * (size_t)m_Width + (size_t)x] == level)
			{
				return false;
			}
			m_Levels[(size_t)y * (size_t)m_Width + (size_t)x] = level;
			m_Changes.push_back((uint32_t)((size_t)y * (size_t)m_Width + (size_t)x));
			m_Revision++;
			return true;
		}

		// Takes another map's size and levels; the revision ends past both. The change log is cleared.
		void Replace(const HeightMap& other)
		{
			const uint64_t revision = (m_Revision > other.m_Revision ? m_Revision : other.m_Revision) + 1;
			m_Width = other.m_Width;
			m_Height = other.m_Height;
			m_Levels = other.m_Levels;
			m_Changes.clear();
			m_Revision = revision;
		}

		// The tiles (indices) changed since the last call, oldest first; may repeat a tile.
		std::vector<uint32_t> TakeChanges()
		{
			std::vector<uint32_t> changes;
			changes.swap(m_Changes);
			return changes;
		}

	private:
		int m_Width = 0;
		int m_Height = 0;
		std::vector<uint8_t> m_Levels;
		std::vector<uint32_t> m_Changes;
		uint64_t m_Revision = 0;
	};
}
