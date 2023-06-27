#pragma once

#include <chrono>

class Timer {
public:
	Timer();

	void Tick();

	void StartMeasurement();
	void StopMeasurement();

	double GetTotalTime() const;
	double GetTickDeltaTime() const;
	
	double GetMeasuredTime() const;
	uint64_t GetMeasuredTicks() const;

private:
	std::chrono::high_resolution_clock clock;
	double m_TotalTime;
	double m_TickDeltaTime;
	std::chrono::steady_clock::time_point m_TickPrevTime;

	bool m_IsMeasuring;
	double m_MeasuredTime;
	uint64_t m_MeasuredTicks;
};