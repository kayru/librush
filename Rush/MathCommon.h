#pragma once

#include "Rush.h"

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
}
