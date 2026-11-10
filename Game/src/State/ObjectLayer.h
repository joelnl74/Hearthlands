#pragma once

#include <cstdint>
#include <vector>

namespace Hydro
{
	// What stands on each tile (ObjectDefinition::ObjectId, 0 for nothing), row by row like TileMap's tiles: run state on
	// the session. One object per tile. The revision moves on every change, so the view redraws only then.
	class ObjectLayer
	{
	public:
		void Reset(size_t tiles)
		{
			m_Ids.assign(tiles, 0);
			m_Count = 0;
			m_Revision++;
		}

		const std::vector<uint8_t>& GetIds() const
		{
			return m_Ids;
		}

		uint8_t Get(size_t index) const
		{
			return index < m_Ids.size() ? m_Ids[index] : (uint8_t)0;
		}

		// False when out of range or unchanged.
		bool Set(size_t index, uint8_t objectId)
		{
			if (index >= m_Ids.size() || m_Ids[index] == objectId)
			{
				return false;
			}
			m_Count += (m_Ids[index] == 0 ? 1 : 0) - (objectId == 0 ? 1 : 0);
			m_Ids[index] = objectId;
			m_Revision++;
			return true;
		}

		// How many tiles hold an object.
		uint32_t GetCount() const
		{
			return m_Count;
		}

		uint64_t GetRevision() const
		{
			return m_Revision;
		}

	private:
		std::vector<uint8_t> m_Ids;
		uint32_t m_Count = 0;
		uint64_t m_Revision = 0;
	};
}
