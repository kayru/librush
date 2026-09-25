#pragma once

#include "Rush.h"

#include <bit>
#include <float.h>
#include <math.h>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace Rush
{
static const float Pi    = 3.14159265358979323846f;
static const float TwoPi = 2 * 3.14159265358979323846f;

inline float toRadians(float degrees) { return degrees * 0.0174532925f; }
inline float toDegrees(float radians) { return radians * 57.2957795f; }

template <typename T> inline T min(const T& a, const T& b) { return (a < b) ? a : b; }
template <typename T> inline T max(const T& a, const T& b) { return (a < b) ? b : a; }
template <typename T> inline T min3(const T& a, const T& b, const T& c) { return min(a, min(b, c)); }
template <typename T> inline T max3(const T& a, const T& b, const T& c) { return max(a, max(b, c)); }
template <typename T> inline T sqr(T x) { return x * x; }
template <typename T> inline T abs(T i) { return (i < 0) ? -i : i; }
template <typename T> inline T lerp(const T& a, const T& b, float alpha) { return a * (1.0f - alpha) + b * alpha; }
template <typename T> inline T getSign(T v) { return v < T(0) ? T(-1) : T(1); }
template <typename T> inline T clamp(const T& val, const T& minVal, const T& maxVal)
{
	return min(max(val, minVal), maxVal);
}
inline float         saturate(float val) { return clamp(val, 0.0f, 1.0f); }
inline float         quantize(float val, float step) { return float(int(val / step)) * step; }
inline constexpr u32 alignCeiling(u32 x, u32 boundary) { return x + (~(x - 1) % boundary); }
inline constexpr u32 alignFloor(u32 x, u32 boundary) { return x - (x % boundary); }
inline constexpr u64 alignCeiling(u64 x, u64 boundary) { return x + (~(x - 1) % boundary); }
inline constexpr u64 alignFloor(u64 x, u64 boundary) { return x - (x % boundary); }

inline u32 nextPow2(u32 n)
{
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n++;
	return n;
};

inline u32 divUp(u32 n, u32 d) { return (n + d - 1) / d; }

inline u32 bitScanForward(u32 mask)
{
#ifdef _MSC_VER
	unsigned long count;
	_BitScanForward(&count, mask);
	return count;
#else
	return __builtin_ctz(mask);
#endif
}

inline u32 bitScanReverse(u32 mask)
{
#ifdef _MSC_VER
	unsigned long count;
	_BitScanReverse(&count, mask);
	return 31 - count;
#else
	return __builtin_clz(mask);
#endif
}

inline u32 bitCount(u32 mask)
{
#ifdef _MSC_VER
	return __popcnt(mask);
#else
	return __builtin_popcount(mask);
#endif
}

// IEEE 754 binary16, rounded to nearest even; beyond the largest half gives infinity
inline u16 floatToHalf(float value)
{
	const u32 f = std::bit_cast<u32>(value);
	const u32 sign = (f >> 16) & 0x8000u;
	const u32 absBits = f & 0x7FFFFFFFu;
	if (absBits >= 0x7F800000u)
	{
		return u16(sign | (absBits > 0x7F800000u ? 0x7E00u : 0x7C00u));
	}
	if (absBits >= 0x477FF000u) // 65520 and up round past 65504
	{
		return u16(sign | 0x7C00u);
	}
	const u32 exponent = absBits >> 23;
	const u32 mantissa = (absBits & 0x7FFFFFu) | 0x800000u;
	u32 shift = 13;
	u32 half = 0;
	if (exponent >= 113)
	{
		half = ((exponent - 112) << 10) | ((mantissa & 0x7FFFFFu) >> 13);
	}
	else if (exponent >= 102) // subnormal half
	{
		shift = 126 - exponent;
		half = mantissa >> shift;
	}
	else
	{
		return u16(sign); // below half the smallest subnormal
	}
	const u32 remainder = mantissa & ((1u << shift) - 1u);
	const u32 halfway = 1u << (shift - 1u);
	if (remainder > halfway || (remainder == halfway && (half & 1u)))
	{
		++half; // a carry into the exponent is the correctly rounded result
	}
	return u16(sign | half);
}

inline float halfToFloat(u16 h)
{
	const u32 sign = u32(h & 0x8000u) << 16;
	const u32 exponent = (h >> 10) & 0x1Fu;
	const u32 mantissa = h & 0x3FFu;
	if (exponent == 0)
	{
		// Zero or subnormal: mantissa * 2^-24, exact in float
		const float magnitude = float(mantissa) * (1.0f / 16777216.0f);
		return sign ? -magnitude : magnitude;
	}
	if (exponent == 31)
	{
		return std::bit_cast<float>(sign | 0x7F800000u | (mantissa << 13));
	}
	return std::bit_cast<float>(sign | ((exponent + 112) << 23) | (mantissa << 13));
}
}
