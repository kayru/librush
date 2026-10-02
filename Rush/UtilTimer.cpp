#include "UtilTimer.h"

#include <chrono>

namespace Rush
{

const Timer Timer::global;

u64 Timer::nowNs()
{
	const auto now = std::chrono::steady_clock::now().time_since_epoch();
	return u64(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

Timer::Timer(void) { reset(); }

Timer::~Timer(void) {}

void Timer::reset() { m_start = nowNs(); }

u64 Timer::microTime() const { return ticks() / 1000; }

double Timer::time() const { return double(ticks()) / 1e9; }

u64 Timer::ticks() const { return nowNs() - m_start; }

u64 Timer::ticksPerSecond() const { return 1'000'000'000ull; }

}
