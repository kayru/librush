#pragma once

#include "MathTypes.h"

namespace Rush
{
	struct ColorRGBA;
	struct ColorRGBA8;
	
	// high-precision (32 bit floating point) color
	struct ColorRGBA
	{
		float r, g, b, a;

		ColorRGBA(){}

		ColorRGBA(float _r, float _g, float _b, float _a=1.0f)
			: r(_r)
			, g(_g)
			, b(_b)
			, a(_a)
		{
		}

		ColorRGBA(const Vec3& vec, float _a = 1.0f)
			: r(vec.x)
			, g(vec.y)
			, b(vec.z)
			, a(_a)
		{
		}
		
		ColorRGBA(const Vec4& vec)
		: r(vec.x)
		, g(vec.y)
		, b(vec.z)
		, a(vec.w)
		{
		}

		void RGBtoHSV();
		void HSVtoRGB();

		operator Vec4() const { return rgba(); }
		
		Vec3 rgb() const { return Vec3(r, g, b); }
		Vec4 rgba() const { return Vec4(r, g, b, a); }

		operator ColorRGBA8() const;

		//////////////////////////////////////////////////////////////////////////

		static ColorRGBA Black(float a = 1.0f)		{ return ColorRGBA(0.0f, 0.0f, 0.0f, a); }
		static ColorRGBA White(float a = 1.0f)		{ return ColorRGBA(1.0f, 1.0f, 1.0f, a); }
		static ColorRGBA Red(float a = 1.0f)		{ return ColorRGBA(1.0f, 0.0f, 0.0f, a); }
		static ColorRGBA Green(float a = 1.0f)		{ return ColorRGBA(0.0f, 1.0f, 0.0f, a); }
		static ColorRGBA Blue(float a = 1.0f)		{ return ColorRGBA(0.0f, 0.0f, 1.0f, a); }
		static ColorRGBA Cyan(float a = 1.0f)		{ return ColorRGBA(0.0f, 1.0f, 1.0f, a); }
		static ColorRGBA Magenta(float a = 1.0f)	{ return ColorRGBA(1.0f, 0.0f, 1.0f, a); }
		static ColorRGBA Yellow(float a = 1.0f)	{ return ColorRGBA(1.0f, 1.0f, 0.0f, a); }
		static ColorRGBA Orange(float a = 1.0f)	{ return ColorRGBA(1.0f, 0.5f, 0.0f, a); }
		static ColorRGBA Purple(float a = 1.0f)	{ return ColorRGBA(0.5f, 0.0f, 0.5f, a); }
	};

	template <>
	ColorRGBA lerp(const ColorRGBA& x, const ColorRGBA& y, float t);

	// low-precision (8 bit unsigned integer) color
	struct ColorRGBA8
	{
		u8 r, g, b, a;

		ColorRGBA8(){}

		explicit ColorRGBA8(u32 col)
		{
			*this = *reinterpret_cast<ColorRGBA8*>(&col);
		}

		ColorRGBA8(u8 _r, u8 _g, u8 _b, u8 _a=255)
			: r(_r)
			, g(_g)
			, b(_b)
			, a(_a)
		{
		}
		
		operator ColorRGBA() const
		{
			float rf = float(r) / 255.0f;
			float gf = float(g) / 255.0f;
			float bf = float(b) / 255.0f;
			float af = float(a) / 255.0f;

			return ColorRGBA(rf, gf, bf, af);
		}

		operator u32() const
		{
			return *reinterpret_cast<const u32*>(this);
		}

		//////////////////////////////////////////////////////////////////////////

		static ColorRGBA8 Black(u8 a=0xFF)		{ return ColorRGBA8(0x00, 0x00, 0x00, a); }
		static ColorRGBA8 White(u8 a = 0xFF)	{ return ColorRGBA8(0xFF, 0xFF, 0xFF, a); }
		static ColorRGBA8 Red(u8 a = 0xFF)		{ return ColorRGBA8(0xFF, 0x00, 0x00, a); }
		static ColorRGBA8 Green(u8 a = 0xFF)	{ return ColorRGBA8(0x00, 0xFF, 0x00, a); }
		static ColorRGBA8 Blue(u8 a = 0xFF)		{ return ColorRGBA8(0x00, 0x00, 0xFF, a); }

	};

	inline float linearToSRGB(float v)
	{
		v = min(1.0f, v);
		return v <= 0.0031308f ? v * 12.92f : 1.055f * powf(v, 1.0f / 2.4f) - 0.055f;
	}

	inline float SRGBToLinear(float sRGB)
	{
		float linearLo = sRGB / 12.92f;
		float linearHi = powf((sRGB + 0.055f) / 1.055f, 2.4f);
		float linear = (sRGB <= 0.04045f) ? linearLo : linearHi;
		return linear;
	}

	inline ColorRGBA linearToSRGB(ColorRGBA col)
	{
		col.r = linearToSRGB(col.r);
		col.g = linearToSRGB(col.g);
		col.b = linearToSRGB(col.b);

		return col;
	}
	inline Vec3 linearToSRGB(Vec3 col)
	{
		col.x = linearToSRGB(col.x);
		col.y = linearToSRGB(col.y);
		col.z = linearToSRGB(col.z);

		return col;
	}
}

