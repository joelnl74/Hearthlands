#pragma once

#include <cstdint>

namespace Hydro
{
	// The game's random numbers: a small seeded generator (xorshift64*), the same sequence on every compiler, so rules
	// given one are repeatable in tests. Pass it by reference into rules; never a static.
	class Random
	{
	public:
		explicit Random(uint64_t seed = 1)
			: m_State(seed != 0 ? seed : 0x9E3779B97F4A7C15ull)
		{
		}

		uint32_t Next()
		{
			m_State ^= m_State >> 12;
			m_State ^= m_State << 25;
			m_State ^= m_State >> 27;
			return (uint32_t)((m_State * 0x2545F4914F6CDD1Dull) >> 32);
		}

		// 0 to 1, never 1.
		float Float()
		{
			return (float)(Next() >> 8) / (float)(1u << 24);
		}

		// 0 to count - 1 (0 when count is 0).
		uint32_t Below(uint32_t count)
		{
			return count == 0 ? 0 : (uint32_t)(((uint64_t)Next() * count) >> 32);
		}

		// The dice's position, to save a run and go on with the same throws after loading it.
		uint64_t GetState() const
		{
			return m_State;
		}

		void SetState(uint64_t state)
		{
			m_State = state != 0 ? state : 0x9E3779B97F4A7C15ull;
		}

	private:
		uint64_t m_State;
	};
}
