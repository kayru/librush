#pragma once

#include "Rush.h"

namespace Rush
{
class Timer
{
public:
	Timer(void);
	~Timer(void);

	void reset();

	u64    microTime() const; // elapsed time in microseconds
	double time() const;      // elapsed time in seconds

	u64 ticks() const;          // elapsed std::chrono::steady_clock nanoseconds
	u64 ticksPerSecond() const;

	static u64 nowNs(); // std::chrono::steady_clock

	static const Timer global;

private:
	u64 m_start = 0;
};
}
