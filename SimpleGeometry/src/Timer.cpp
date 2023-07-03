#include <Timer.h>

Timer::Timer() :
	m_TotalTime(0.0),
	m_TickDeltaTime(0.0),
	m_IsMeasuring(false),
	m_MeasuredTime(0.0),
	m_MeasuredTicks(0)
{
	m_TickPrevTime = clock.now();
}

void Timer::Tick() {
	std::chrono::steady_clock::time_point curTime = clock.now();
	m_TickDeltaTime = (curTime - m_TickPrevTime).count() * 1e-9;
	m_TotalTime += m_TickDeltaTime;
	m_TickPrevTime = curTime;

	if (m_IsMeasuring) {
		m_MeasuredTime += m_TickDeltaTime;
		++m_MeasuredTicks;
	}
}

void Timer::StartMeasurement() {
	m_IsMeasuring = true;
	m_MeasuredTime = 0.0;
	m_MeasuredTicks = 0;
}

void Timer::StopMeasurement() {
	m_IsMeasuring = false;
}

double Timer::GetTotalTime() const {
	return m_TotalTime;
}

double Timer::GetTickDeltaTime() const {
	return m_TickDeltaTime;
}

double Timer::GetMeasuredTime() const {
	return m_MeasuredTime;
}

uint64_t Timer::GetMeasuredTicks() const {
	return m_MeasuredTicks;
}