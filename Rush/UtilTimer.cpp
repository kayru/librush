#include "UtilTimer.h"

#if defined(RUSH_PLATFORM_WINDOWS)
#include <windows.h>
#else
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#endif

namespace Rush
{

const Timer Timer::global;

Timer::Timer(void)
{
#if defined(RUSH_PLATFORM_WINDOWS)
	m_numer = 1000000;
	QueryPerformanceFrequency((LARGE_INTEGER*)&m_denom);
#else
	m_numer = 1;
	m_denom = 1;
#endif

	reset();
}

Timer::~Timer(void) {}

void Timer::reset()
{
#if defined(RUSH_PLATFORM_WINDOWS)
	QueryPerformanceCounter((LARGE_INTEGER*)&m_start);
#else
	timeval t;
	gettimeofday(&t, nullptr);
	m_start = u64(t.tv_sec) * 1000000ULL + u64(t.tv_usec);
#endif
}

u64 Timer::microTime() const
{
#if defined(RUSH_PLATFORM_WINDOWS)
	return ticks() * m_numer / m_denom;
#else
	return ticks();
#endif
}

double Timer::time() const { return double(microTime()) / 1e6; }

u64 Timer::ticks() const
{
#if defined(RUSH_PLATFORM_WINDOWS)
	u64 curtime;
	QueryPerformanceCounter((LARGE_INTEGER*)&curtime);
	return curtime - m_start;
#else
	timeval curtime;
	gettimeofday(&curtime, nullptr);
	u64 elapsed = (u64(curtime.tv_sec) * 1000000ULL + u64(curtime.tv_usec)) - m_start;
	return elapsed;
#endif
}

u64 Timer::ticksPerSecond() const
{
	return m_denom;
}

}
