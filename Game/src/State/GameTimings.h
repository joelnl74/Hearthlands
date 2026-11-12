#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace Hydro
{
	// A part of the game's frame that is timed (GameTimings).
	enum class TimedPart : uint8_t
	{
		Water,
		Growth,
		Objects,
		Soil,
		Wildlife,
		Settlements,
		Habitat,
		Milestones,
		Unlocks,
		TerrainView,
		ReliefView,
		ObjectsView,
		AnimalsView,
		SettlersView,
		Hud,
		Count
	};

	// Where the game's frame time goes: scripts time their parts (TimedScope) and WorldScript logs the averages every few
	// seconds (Frame) beside the whole frame, so what is left is the engine's (particles, transforms, UI, drawing).
	struct GameTimings
	{
		static constexpr int PartCount = (int)TimedPart::Count;
		static constexpr double ReportSeconds = 5.0;

		double Ms[PartCount] = {};
		double FrameMs = 0.0;
		int Frames = 0;

		static const char* Name(int part)
		{
			const char* names[PartCount] = { "water", "land growth", "plants", "soil", "wildlife", "settlements", "habitats",
				"milestones", "unlocks", "terrain view", "relief view", "objects view", "animals view", "settlers view", "hud" };
			return part >= 0 && part < PartCount ? names[part] : "?";
		}

		// After a frame of `seconds`: every ReportSeconds the averages per frame as one line (largest first), else empty.
		std::string Frame(float seconds)
		{
			FrameMs += (double)seconds * 1000.0;
			Frames++;
			if (FrameMs < ReportSeconds * 1000.0)
			{
				return std::string();
			}
			std::vector<std::pair<double, int>> parts;
			double game = 0.0;
			for (int part = 0; part < PartCount; part++)
			{
				parts.push_back({ Ms[part] / (double)Frames, part });
				game += Ms[part] / (double)Frames;
			}
			std::sort(parts.begin(), parts.end(), [](const std::pair<double, int>& a, const std::pair<double, int>& b)
				{
					return a.first > b.first;
				});
			char text[128];
			const double frame = FrameMs / (double)Frames;
			std::snprintf(text, sizeof(text), "Frame %.1f ms (%d frames): game parts %.1f ms, the rest (engine) %.1f ms |", frame,
				Frames, game, frame - game);
			std::string line = text;
			for (const std::pair<double, int>& part : parts)
			{
				if (part.first >= 0.05)
				{
					std::snprintf(text, sizeof(text), " %s %.1f", Name(part.second), part.first);
					line += text;
				}
			}
			*this = GameTimings{};
			return line;
		}
	};

	// Adds the time from its making to its end to a part of the frame.
	class TimedScope
	{
	public:
		TimedScope(GameTimings& timings, TimedPart part)
			: m_Timings(timings), m_Part(part), m_Start(std::chrono::steady_clock::now())
		{
		}

		~TimedScope()
		{
			const std::chrono::duration<double, std::milli> spent = std::chrono::steady_clock::now() - m_Start;
			m_Timings.Ms[(int)m_Part] += spent.count();
		}

		TimedScope(const TimedScope&) = delete;
		TimedScope& operator=(const TimedScope&) = delete;

	private:
		GameTimings& m_Timings;
		TimedPart m_Part;
		std::chrono::steady_clock::time_point m_Start;
	};
}
