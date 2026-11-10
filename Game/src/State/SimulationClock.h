#pragma once

#include <cstdint>

namespace Hydro
{
	// How fast the world runs and how long it has run: run state on the session. Speed 0 is paused. The HUD sets the speed;
	// SimulationScript advances the time and runs the ticks due (GrowthRules::TicksDue).
	struct SimulationClock
	{
		static constexpr double SecondsPerDay = 60.0; // world seconds at 1x

		int Speed = 1;        // world seconds per real second: 0 (paused), 1, 3 or 10
		double Seconds = 0.0; // world time since the run started
		uint64_t Ticks = 0;   // simulation ticks run (SimulationScript; dropped ticks count as run)

		// Day 1 while the first SecondsPerDay run.
		int GetDay() const
		{
			return 1 + (int)(Seconds / SecondsPerDay);
		}
	};
}
