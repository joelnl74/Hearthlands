#pragma once

#include "Data/SeasonDefinition.h"
#include "State/SeasonState.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Hydro
{
	// The year: the seasons in Order, each lasting its Days and blending into the next over its last blendDays, so the
	// snowline, rivers and colours drift instead of jumping. Pure.
	class SeasonRules
	{
	public:
		// The seasons sorted by Order (ties: table order).
		static std::vector<SeasonDefinition> Sorted(std::vector<SeasonDefinition> seasons)
		{
			std::stable_sort(seasons.begin(), seasons.end(), [](const SeasonDefinition& a, const SeasonDefinition& b)
			{
				return a.Order < b.Order;
			});
			return seasons;
		}

		// The season `days` into the run (fractional days; 0 is the first season's start). seasons sorted (Sorted).
		static SeasonState At(const std::vector<SeasonDefinition>& seasons, double days, float blendDays)
		{
			SeasonState state;
			double year = 0.0;
			for (const SeasonDefinition& season : seasons)
			{
				year += season.Days > 0.0f ? (double)season.Days : 0.0;
			}
			if (seasons.empty() || year <= 0.0)
			{
				return state;
			}
			const double clamped = days > 0.0 ? days : 0.0;
			state.Year = 1 + (int)(clamped / year);
			double into = clamped - std::floor(clamped / year) * year;
			for (size_t i = 0; i < seasons.size(); i++)
			{
				const double length = seasons[i].Days > 0.0f ? (double)seasons[i].Days : 0.0;
				if (into < length || i + 1 == seasons.size())
				{
					state.Index = (int)i;
					state.Next = (int)((i + 1) % seasons.size());
					state.Day = 1 + (int)into;
					const double blend = std::min((double)blendDays, length);
					const double start = length - blend;
					const double t = blend > 0.0 && into > start ? (into - start) / blend : 0.0;
					state.Blend = Smooth((float)std::min(t, 1.0));
					break;
				}
				into -= length;
			}
			const SeasonDefinition& a = seasons[(size_t)state.Index];
			const SeasonDefinition& b = seasons[(size_t)state.Next];
			const float t = state.Blend;
			state.Name = a.Name;
			state.SnowShift = Mix(a.SnowShift, b.SnowShift, t);
			state.Flow = Mix(a.Flow, b.Flow, t);
			state.Melt = Mix(a.Melt, b.Melt, t);
			state.Growth = Mix(a.Growth, b.Growth, t);
			state.Life = Mix(a.Life, b.Life, t);
			state.Harvest = Mix(a.Harvest, b.Harvest, t);
			state.Sowing = a.Sowing;
			state.Frost = a.Frost;
			state.Migrants = Mix(a.Migrants, b.Migrants, t);
			state.Burrowed = Mix(a.Burrowed, b.Burrowed, t);
			state.GroundSnow = Mix(a.GroundSnow, b.GroundSnow, t);
			state.Ice = Mix(a.Ice, b.Ice, t);
			state.Fade = Mix(a.Fade, b.Fade, t);
			state.Tint = a.Tint + (b.Tint - a.Tint) * t;
			state.LeafTint = a.LeafTint + (b.LeafTint - a.LeafTint) * t;
			return state;
		}

		// A value rounded to steps of 1 / steps: changes the simulation and the materials follow only every step.
		static float Step(float value, float steps)
		{
			return std::round(value * steps) / steps;
		}

	private:
		static float Mix(float a, float b, float t)
		{
			return a + (b - a) * t;
		}

		static float Smooth(float t)
		{
			return t * t * (3.0f - 2.0f * t);
		}
	};
}
