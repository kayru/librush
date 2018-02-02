#pragma once

#include "Rush.h"

namespace Rush
{
class Rand
{
public:
	Rand(u32 seed = 0) : m_seed(seed) {}
	u32 rand()
	{
		m_seed = 214013 * m_seed + 2531011;
		return (m_seed ^ m_seed >> 16);
	}
	u32   getSeed() const { return m_seed; }
	float getFloat(float min, float max)
	{
		m_seed = 214013 * m_seed + 2531011;
		return min + float(m_seed >> 16) * (1.0f / 65535.0f) * (max - min);
	}
	int getInt(int min, int max) { return min + rand() % (max - min + 1); }
	u32 getUint(u32 min, u32 max) { return min + rand() % (max - min + 1); }
	u8  getChar(u8 min, u8 max) { return u8(min + u8(rand() % (max - min + 1))); }

private:
	u32 m_seed;
};
}
