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

	u64 ticks() const;
	u64 ticksPerSecond() const;

	static const Timer global;

private:
	u64 m_start;
	u64 m_numer;
	u64 m_denom;
};
}
