#include "MathCommon.h"

#include <bit>

namespace Rush
{

u16 floatToHalf(float value)
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

float halfToFloat(u16 h)
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

} // namespace Rush
