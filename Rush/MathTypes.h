#pragma once

#include "MathCommon.h"

namespace Rush
{

struct Vec2
{
	float x, y;

	Vec2(){};
	Vec2(const float _x, const float _y) : x(_x), y(_y){};
	Vec2(const float s) : x(s), y(s){};
	Vec2(const float* arr) : x(arr[0]), y(arr[1]){};

	Vec2& operator=(const float s)
	{
		x = s;
		y = s;
		return *this;
	}

	Vec2& operator+=(const Vec2& rhs)
	{
		x += rhs.x;
		y += rhs.y;
		return *this;
	}
	Vec2& operator+=(const float s)
	{
		x += s;
		y += s;
		return *this;
	}
	Vec2& operator-=(const Vec2& rhs)
	{
		x -= rhs.x;
		y -= rhs.y;
		return *this;
	}
	Vec2& operator-=(const float s)
	{
		x -= s;
		y -= s;
		return *this;
	}
	Vec2& operator*=(const Vec2& rhs)
	{
		x *= rhs.x;
		y *= rhs.y;
		return *this;
	}
	Vec2& operator*=(const float s)
	{
		x *= s;
		y *= s;
		return *this;
	}
	Vec2& operator/=(const Vec2& rhs)
	{
		x /= rhs.x;
		y /= rhs.y;
		return *this;
	}
	Vec2& operator/=(const float s)
	{
		x /= s;
		y /= s;
		return *this;
	}

	Vec2 operator+(const Vec2& rhs) const { return Vec2(x + rhs.x, y + rhs.y); }
	Vec2 operator-(const Vec2& rhs) const { return Vec2(x - rhs.x, y - rhs.y); }
	Vec2 operator*(const Vec2& rhs) const { return Vec2(x * rhs.x, y * rhs.y); }
	Vec2 operator*(const float s) const { return Vec2(x * s, y * s); }
	Vec2 operator/(const Vec2& rhs) const { return Vec2(x / rhs.x, y / rhs.y); }
	Vec2 operator/(const float s) const { return Vec2(x / s, y / s); }
	Vec2 operator+() const { return *this; }
	Vec2 operator-() const { return Vec2(-x, -y); }

	bool operator==(const Vec2& rhs) const { return (x == rhs.x && y == rhs.y); }
	bool operator!=(const Vec2& rhs) const { return (x != rhs.x || y != rhs.y); }
	bool operator<(const Vec2& rhs) const { return (x < rhs.x && y < rhs.y); }
	bool operator<=(const Vec2& rhs) const { return (x <= rhs.x && y <= rhs.y); }
	bool operator>(const Vec2& rhs) const { return (x > rhs.x && y > rhs.y); }
	bool operator>=(const Vec2& rhs) const { return (x >= rhs.x && y >= rhs.y); }

	float*        begin() { return &x; }
	float*        end() { return &y + 1; }
	const float*  begin() const { return &x; }
	const float*  end() const { return &y + 1; }
	static size_t size() { return 2; }

	const float& operator[](const size_t i) const { return *(&x + i); }
	float&       operator[](const size_t i) { return *(&x + i); }

	const float& elem(const size_t i) const { return *(&x + i); }
	float&       elem(const size_t i) { return *(&x + i); }

	float reduceAdd() const { return x + y; }
	float reduceMul() const { return x * y; }
	float reduceMin() const { return min(x, y); }
	float reduceMax() const { return max(x, y); }

	float lengthSquared() const { return x * x + y * y; }
	float length() const { return sqrtf(lengthSquared()); }

	float dot(const Vec2& rhs) const { return x * rhs.x + y * rhs.y; }
	float pseudoCross(const Vec2& rhs) const { return x * rhs.y - y * rhs.x; }
	Vec2  perpendicularCCW(void) const { return Vec2(-y, x); }
	Vec2  perpendicularCW(void) const { return Vec2(y, -x); }
	void  normalize()
	{
		float len = length();
		if (len != 0)
			(*this) /= len;
	}
	Vec2 reflect(const Vec2& normal) { return (*this) - Vec2(2) * normal * Vec2(dot(normal)); }
	Vec2 reciprocal() const { return Vec2(1.0f / x, 1.0f / y); }
};

struct Vec3
{
	float x, y, z;

	Vec3(){};

	Vec3(const float _x, const float _y, const float _z) : x(_x), y(_y), z(_z){};
	Vec3(const float s) : x(s), y(s), z(s){};
	Vec3(const float* arr) : x(arr[0]), y(arr[1]), z(arr[2]){};

	Vec3& operator=(const float s)
	{
		x = s;
		y = s;
		z = s;
		return *this;
	}

	Vec3& operator+=(const Vec3& rhs)
	{
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		return *this;
	}
	Vec3& operator+=(const float s)
	{
		x += s;
		y += s;
		z += s;
		return *this;
	}
	Vec3& operator-=(const Vec3& rhs)
	{
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
		return *this;
	}
	Vec3& operator-=(const float s)
	{
		x -= s;
		y -= s;
		z -= s;
		return *this;
	}
	Vec3& operator*=(const Vec3& rhs)
	{
		x *= rhs.x;
		y *= rhs.y;
		z *= rhs.z;
		return *this;
	}
	Vec3& operator*=(const float s)
	{
		x *= s;
		y *= s;
		z *= s;
		return *this;
	}
	Vec3& operator/=(const Vec3& rhs)
	{
		x /= rhs.x;
		y /= rhs.y;
		z /= rhs.z;
		return *this;
	}
	Vec3& operator/=(const float s)
	{
		x /= s;
		y /= s;
		z /= s;
		return *this;
	}

	Vec3 operator+(const Vec3& rhs) const { return Vec3(x + rhs.x, y + rhs.y, z + rhs.z); }
	Vec3 operator-(const Vec3& rhs) const { return Vec3(x - rhs.x, y - rhs.y, z - rhs.z); }
	Vec3 operator*(const Vec3& rhs) const { return Vec3(x * rhs.x, y * rhs.y, z * rhs.z); }
	Vec3 operator*(const float s) const { return Vec3(x * s, y * s, z * s); }
	Vec3 operator/(const Vec3& rhs) const { return Vec3(x / rhs.x, y / rhs.y, z / rhs.z); }
	Vec3 operator/(const float s) const { return Vec3(x / s, y / s, z / s); }
	Vec3 operator+() const { return *this; }
	Vec3 operator-() const { return Vec3(-x, -y, -z); }

	bool operator==(const Vec3& rhs) const { return (x == rhs.x && y == rhs.y && z == rhs.z); }
	bool operator!=(const Vec3& rhs) const { return !(*this == rhs); }
	bool operator<(const Vec3& rhs) const { return (x < rhs.x && y < rhs.y && z < rhs.z); }
	bool operator<=(const Vec3& rhs) const { return (x <= rhs.x && y <= rhs.y && z <= rhs.z); }
	bool operator>(const Vec3& rhs) const { return (x > rhs.x && y > rhs.y && z > rhs.z); }
	bool operator>=(const Vec3& rhs) const { return (x >= rhs.x && y >= rhs.y && z >= rhs.z); }

	float*        begin() { return &x; }
	float*        end() { return &z + 1; }
	const float*  begin() const { return &x; }
	const float*  end() const { return &z + 1; }
	static size_t size() { return 3; }

	const float& operator[](const size_t i) const { return *(&x + i); }
	float&       operator[](const size_t i) { return *(&x + i); }

	const float& elem(const size_t i) const { return *(&x + i); }
	float&       elem(const size_t i) { return *(&x + i); }

	float lengthSquared() const { return x * x + y * y + z * z; }
	float length() const { return sqrtf(lengthSquared()); }

	float reduceAdd() const { return x + y + z; }
	float reduceMul() const { return x * y * z; }
	float reduceMin() const { return min3(x, y, z); }
	float reduceMax() const { return max3(x, y, z); }

	float dot(const Vec3& rhs) const { return x * rhs.x + y * rhs.y + z * rhs.z; }
	Vec3  cross(const Vec3& rhs) const
	{
		return Vec3(y * rhs.z - z * rhs.y, z * rhs.x - x * rhs.z, x * rhs.y - y * rhs.x);
	}
	Vec3 left() const { return Vec3(-z, y, x); }  // left perpendicular
	Vec3 right() const { return Vec3(z, y, -x); } // right perpendicular
	void normalize() { (*this) /= length(); }
	Vec3 reciprocal() const { return Vec3(1.0f / x, 1.0f / y, 1.0f / z); }

	Vec2 xy() const { return Vec2(x, y); }
	Vec2 xz() const { return Vec2(x, z); }

	float swizzle(size_t a) const { return elem(a); }
	Vec2  swizzle(size_t a, size_t b) const { return Vec2(elem(a), elem(b)); }
	Vec3  swizzle(size_t a, size_t b, size_t c) const { return Vec3(elem(a), elem(b), elem(c)); }
};

struct Vec4
{
	float x, y, z, w;

	Vec4(){};

	Vec4(const float _x, const float _y, const float _z, const float _w) : x(_x), y(_y), z(_z), w(_w){};
	Vec4(const float s) : x(s), y(s), z(s), w(s){};
	Vec4(const float* arr) : x(arr[0]), y(arr[1]), z(arr[2]), w(arr[3]){};
	Vec4(const Vec2& rhs, float _z = 0.0f, float _w = 0.0f) : x(rhs.x), y(rhs.y), z(_z), w(_w){};
	explicit Vec4(const Vec3& rhs, float _w = 0.0f) : x(rhs.x), y(rhs.y), z(rhs.z), w(_w){};

	Vec4& operator=(const float s)
	{
		x = s;
		y = s;
		z = s;
		w = s;
		return *this;
	}

	Vec4& operator+=(const Vec4& rhs)
	{
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		w += rhs.w;
		return *this;
	}
	Vec4& operator+=(const float s)
	{
		x += s;
		y += s;
		z += s;
		w += s;
		return *this;
	}
	Vec4& operator-=(const Vec4& rhs)
	{
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
		w -= rhs.w;
		return *this;
	}
	Vec4& operator-=(const float s)
	{
		x -= s;
		y -= s;
		z -= s;
		w -= s;
		return *this;
	}
	Vec4& operator*=(const Vec4& rhs)
	{
		x *= rhs.x;
		y *= rhs.y;
		z *= rhs.z;
		w *= rhs.w;
		return *this;
	}
	Vec4& operator*=(const float s)
	{
		x *= s;
		y *= s;
		z *= s;
		w *= s;
		return *this;
	}
	Vec4& operator/=(const Vec4& rhs)
	{
		x /= rhs.x;
		y /= rhs.y;
		z /= rhs.z;
		w /= rhs.w;
		return *this;
	}
	Vec4& operator/=(const float s)
	{
		x /= s;
		y /= s;
		z /= s;
		w /= s;
		return *this;
	}

	Vec4 operator+(const Vec4& rhs) const { return Vec4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w); }
	Vec4 operator-(const Vec4& rhs) const { return Vec4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w); }
	Vec4 operator*(const Vec4& rhs) const { return Vec4(x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w); }
	Vec4 operator*(const float s) const { return Vec4(x * s, y * s, z * s, w * s); }
	Vec4 operator/(const Vec4& rhs) const { return Vec4(x / rhs.x, y / rhs.y, z / rhs.z, w / rhs.w); }
	Vec4 operator/(const float s) const { return Vec4(x / s, y / s, z / s, w / s); }
	Vec4 operator+() const { return *this; }
	Vec4 operator-() const { return Vec4(-x, -y, -z, -w); }

	bool operator==(const Vec4& rhs) const { return (x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w); }
	bool operator!=(const Vec4& rhs) const { return !(*this == rhs); }
	bool operator<(const Vec4& rhs) const { return (x < rhs.x && y < rhs.y && z < rhs.z && w < rhs.w); }
	bool operator<=(const Vec4& rhs) const { return (x <= rhs.x && y <= rhs.y && z <= rhs.z && w <= rhs.w); }
	bool operator>(const Vec4& rhs) const { return (x > rhs.x && y > rhs.y && z > rhs.z && w > rhs.w); }
	bool operator>=(const Vec4& rhs) const { return (x >= rhs.x && y >= rhs.y && z >= rhs.z && w >= rhs.w); }

	float*        begin() { return &x; }
	float*        end() { return &w + 1; }
	const float*  begin() const { return &x; }
	const float*  end() const { return &w + 1; }
	static size_t size() { return 4; }

	const float& operator[](const size_t i) const { return *(&x + i); }
	float&       operator[](const size_t i) { return *(&x + i); }

	const float& elem(const size_t i) const { return *(&x + i); }
	float&       elem(const size_t i) { return *(&x + i); }

	float reduceAdd() const { return x + y + z + w; }
	float reduceMul() const { return x * y * z * w; }
	float reduceMin() const { return min(min3(x, y, z), w); }
	float reduceMax() const { return max(max3(x, y, z), w); }

	float lengthSquared() const { return x * x + y * y + z * z + w * w; }
	float length() const { return sqrtf(lengthSquared()); }

	float dot(const Vec4& rhs) const { return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w; }
	void  normalize() { (*this) /= length(); }
	Vec4  reciprocal() const { return Vec4(1.0f / x, 1.0f / y, 1.0f / z, 1.0f / w); }

	Vec2 xy() const { return Vec2(x, y); }
	Vec2 xz() const { return Vec2(x, z); }

	const Vec3& xyz() const { return *((Vec3*)(this)); }
	Vec3&       xyz() { return *((Vec3*)(this)); }

	float swizzle(size_t a) const { return elem(a); }
	Vec2  swizzle(size_t a, size_t b) const { return Vec2(elem(a), elem(b)); }
	Vec3  swizzle(size_t a, size_t b, size_t c) const { return Vec3(elem(a), elem(b), elem(c)); }
	Vec4  swizzle(size_t a, size_t b, size_t c, size_t d) const { return Vec4(elem(a), elem(b), elem(c), elem(d)); }
};

inline float length(const Vec2& v) { return v.length(); }
inline float length(const Vec3& v) { return v.length(); }
inline float length(const Vec4& v) { return v.length(); }

inline float lengthSquared(const Vec2& v) { return v.lengthSquared(); }
inline float lengthSquared(const Vec3& v) { return v.lengthSquared(); }
inline float lengthSquared(const Vec4& v) { return v.lengthSquared(); }

inline Vec2 normalize(const Vec2& v) { return v / v.length(); }
inline Vec3 normalize(const Vec3& v) { return v / v.length(); }
inline Vec4 normalize(const Vec4& v) { return v / v.length(); }

inline float cross(const Vec2& a, const Vec2& b) { return a.pseudoCross(b); }
inline Vec3  cross(const Vec3& a, const Vec3& b) { return a.cross(b); }
inline float dot(const Vec2& a, const Vec2& b) { return a.dot(b); }
inline float dot(const Vec3& a, const Vec3& b) { return a.dot(b); }
inline float dot(const Vec4& a, const Vec4& b) { return a.dot(b); }
inline Vec2  operator*(float a, const Vec2& b) { return b * a; }
inline Vec3  operator*(float a, const Vec3& b) { return b * a; }
inline Vec4  operator*(float a, const Vec4& b) { return b * a; }

inline Vec2 reflect(const Vec2& i, const Vec2& n) { return i - 2.0f * n * dot(i, n); }
inline Vec3 reflect(const Vec3& i, const Vec3& n) { return i - 2.0f * n * dot(i, n); }
inline Vec4 reflect(const Vec4& i, const Vec4& n) { return i - 2.0f * n * dot(i, n); }

inline Vec2 min(const Vec2& a, const Vec2& b) { return Vec2(min(a.x, b.x), min(a.y, b.y)); }
inline Vec3 min(const Vec3& a, const Vec3& b) { return Vec3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)); }
inline Vec4 min(const Vec4& a, const Vec4& b)
{
	return Vec4(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w));
}

inline Vec2 max(const Vec2& a, const Vec2& b) { return Vec2(max(a.x, b.x), max(a.y, b.y)); }
inline Vec3 max(const Vec3& a, const Vec3& b) { return Vec3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)); }
inline Vec4 max(const Vec4& a, const Vec4& b)
{
	return Vec4(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w));
}

inline Vec2 abs(const Vec2& a) { return Vec2(abs(a.x), abs(a.y)); }
inline Vec3 abs(const Vec3& a) { return Vec3(abs(a.x), abs(a.y), abs(a.z)); }
inline Vec4 abs(const Vec4& a) { return Vec4(abs(a.x), abs(a.y), abs(a.z), abs(a.w)); }

inline Vec2 saturate(const Vec2& val) { return min(Vec2(1.0f), max(Vec2(0.0f), val)); }
inline Vec3 saturate(const Vec3& val) { return min(Vec3(1.0f), max(Vec3(0.0f), val)); }
inline Vec4 saturate(const Vec4& val) { return min(Vec4(1.0f), max(Vec4(0.0f), val)); }

class Box3;

struct Mat2
{
	Vec2 rows[2];

	Vec2 operator*(const Vec2& v) const
	{
		return {
		    v.x * rows[0][0] + v.y * rows[1][0],
		    v.x * rows[0][1] + v.y * rows[1][1],
		};
	}
};

struct Mat3
{
	Vec3 rows[3];

	Vec3 operator*(const Vec3& v) const
	{
		return {
		    v.x * rows[0][0] + v.y * rows[1][0] + v.z * rows[2][0],
		    v.x * rows[0][1] + v.y * rows[1][1] + v.z * rows[2][0],
		    v.x * rows[0][1] + v.y * rows[1][1] + v.z * rows[2][0],
		};
	}
};

inline Mat3 transpose(const Mat3& m)
{
	return {{Vec3{m.rows[0].x, m.rows[1].x, m.rows[2].x}, Vec3{m.rows[0].y, m.rows[1].y, m.rows[2].y},
	    Vec3{m.rows[0].z, m.rows[1].z, m.rows[2].z}}};
}

enum class ProjectionFlags : u32
{
	// Top-Left-Near XYZ: -1, +1, 0
	// Bottom-Right-Far XYZ: +1, -1, +1
	Default = 0,

	FlipVertical = 1 << 0,
};

RUSH_IMPLEMENT_FLAG_OPERATORS(ProjectionFlags, u32);

struct Mat4
{
	Vec4 rows[4];

	Mat4(){};

	Mat4(float m00, float m10, float m20, float m30, float m01, float m11, float m21, float m31, float m02, float m12,
	    float m22, float m32, float m03, float m13, float m23, float m33);

	Mat4(float m00, float m10, float m20, float m01, float m11, float m21, float m02, float m12, float m22);

	Mat4(const Mat3& m);

	Mat4(float* arr);

	Vec4 operator*(const Vec4& v) const;
	Vec3 operator*(const Vec3& v) const;

	Mat4  operator*(const Mat4& rhs) const;
	Mat4& operator*=(const Mat4& rhs);

	float&       operator()(size_t r, size_t c) { return rows[r].elem(c); }
	const float& operator()(size_t r, size_t c) const { return rows[r].elem(c); }

	Vec4 column(size_t c) const;

	Vec4&       row(size_t r) { return rows[r]; }
	const Vec4& row(size_t r) const { return rows[r]; };

	void setRow(size_t r, const Vec4& v) { rows[r] = v; }

	float&       m(size_t r, size_t c) { return rows[r].elem(c); }
	const float& m(size_t r, size_t c) const { return rows[r].elem(c); };

	Mat4 transposed() const;
	void transpose();

	float determinant() const;

	Mat4 inverse() const;

	Mat4 multiply33(const Mat4& mat);

	static Mat4 zero();
	static Mat4 identity();

	static Mat4 translation(const Vec3& v);
	static Mat4 translation(float x, float y, float z) { return translation(Vec3(x, y, z)); }

	static Mat4 scale(const Vec3& v);
	static Mat4 scale(float x, float y, float z);
	static Mat4 scaleTranslate(const Vec3& s, const Vec3& t);
	static Mat4 scaleTranslate(float sx, float sy, float sz, float tx, float ty, float tz);
	static Mat4 rotationX(float angle);
	static Mat4 rotationY(float angle);
	static Mat4 rotationZ(float angle);
	static Mat4 rotationZ(const Vec3& dir);
	static Mat4 rotationAxis(const Vec3& axis, float angle);
	static Mat4 orthographic(float w, float h, float zn = 1, float zf = -1);
	static Mat4 orthographicOffCenter(float l, float r, float b, float t, float zn, float zf);
	static Mat4 orthographicOffCenter(const Box3& bounds);
	static Mat4 perspective(
	    float aspect, float fov, float zn, float zf, ProjectionFlags flags = ProjectionFlags::Default);
	static Mat4 lookAt(const Vec3& position, const Vec3& target, const Vec3& up = Vec3(0, 1, 0));
};

inline Mat4 operator*(float a, const Mat4& b)
{
	Mat4 res = b;
	res.rows[0] *= a;
	res.rows[1] *= a;
	res.rows[2] *= a;
	res.rows[3] *= a;
	return res;
}

struct Quat
{
	float x, y, z, w;

	Quat operator+(const Quat& q) const { return {x + q.x, y + q.y, z + q.z, w + q.w}; }
	Quat operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
	Quat operator/(float s) const { return {x / s, y / s, z / s, w / s}; }
};

inline float dot(const Quat& a, const Quat& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

Quat  makeQuat(const Mat3& m);
Mat3  makeMat3(const Quat& q);
Quat  slerp(const Quat& x, const Quat& y, float a);
Quat  normalize(const Quat& q);
float length(const Quat& q);

template <typename VECTOR_TYPE> class BoxT
{
public:
	typedef VECTOR_TYPE Vector;

	enum ClampType
	{
		ClampType_None = 0,
		ClampType_MinX = 1 << 0,
		ClampType_MinY = 1 << 1,
		ClampType_MinZ = 1 << 2,
		ClampType_MaxX = 1 << 3,
		ClampType_MaxY = 1 << 4,
		ClampType_MaxZ = 1 << 5
	};

	BoxT() {}

	BoxT(const VECTOR_TYPE& min, const VECTOR_TYPE& max) : m_min(min), m_max(max) {}

	BoxT(const VECTOR_TYPE& origin, float radius)
	: m_min(origin - VECTOR_TYPE(radius)), m_max(origin + VECTOR_TYPE(radius))
	{
	}

	const VECTOR_TYPE center() const { return (m_min + m_max) * 0.5f; }
	bool              contains(const VECTOR_TYPE& v) const { return m_min <= v && v <= m_max; }
	bool              contains(const BoxT& b) const { return contains(b.m_min) && contains(b.m_max); }
	bool              intersects(const VECTOR_TYPE& v) const { return contains(v); }
	bool              intersects(const BoxT& b) const
	{
		for (u32 i = 0; i < m_min.size(); ++i)
		{
			if (m_max[i] < b.m_min[i] || m_min[i] > b.m_max[i])
				return false;
		}
		return true;
	}

	VECTOR_TYPE dimensions() const { return m_max - m_min; }

	void translate(const VECTOR_TYPE& t)
	{
		m_min += t;
		m_max += t;
	}

	void expandInit()
	{
		m_min = VECTOR_TYPE(FLT_MAX);
		m_max = VECTOR_TYPE(-FLT_MAX);
	}

	VECTOR_TYPE m_min;
	VECTOR_TYPE m_max;
};

class Box2 : public BoxT<Vec2>
{
	typedef BoxT<Vec2> BASE_TYPE;

public:
	Box2() {}

	Box2(const Vec2& min, const Vec2& max) : BASE_TYPE(min, max) {}
	Box2(const Vec2& origin, float radius) : BASE_TYPE(origin, radius) {}

	Box2(float minX, float minY, float maxX, float maxY) : BASE_TYPE(Vec2(minX, minY), Vec2(maxX, maxY)) {}

	/// Top Left
	Vec2 tl() const { return Vec2(m_min.x, m_max.y); }

	/// Top Right
	Vec2 tr() const { return Vec2(m_max.x, m_max.y); }

	/// Bottom Left
	Vec2 bl() const { return Vec2(m_min.x, m_min.y); }

	/// Bottom Right
	Vec2 br() const { return Vec2(m_max.x, m_min.y); }

	float width() const { return m_max.x - m_min.x; }
	float height() const { return m_max.y - m_min.y; }

	/// Expands this bounding box to contain the box
	void expand(const Box2& rhs)
	{
		m_min.x = min(m_min.x, rhs.m_min.x);
		m_min.y = min(m_min.y, rhs.m_min.y);
		m_max.x = max(m_max.x, rhs.m_max.x);
		m_max.y = max(m_max.y, rhs.m_max.y);
	}

	/// Expands this bounding box to contain the vector
	void expand(const Vec2& rhs)
	{
		m_min.x = min(m_min.x, rhs.x);
		m_min.y = min(m_min.y, rhs.y);
		m_max.x = max(m_max.x, rhs.x);
		m_max.y = max(m_max.y, rhs.y);
	}

	/// Constrain this bounding box by another, so that all it is fully inside it
	void clip(const Box2& clip_by)
	{
		m_min.x = max(m_min.x, clip_by.m_min.x);
		m_min.y = max(m_min.y, clip_by.m_min.y);
		m_max.x = min(m_max.x, clip_by.m_max.x);
		m_max.y = min(m_max.y, clip_by.m_max.y);
	}
};

class Box3 : public BoxT<Vec3>
{
	typedef BoxT<Vec3> BASE_TYPE;

public:
	Box3() {}

	Box3(const Vec3& min, const Vec3& max) : BASE_TYPE(min, max) {}

	Box3(const Vec3& origin, float radius) : BASE_TYPE(origin, radius) {}

	/// Expands this bounding box to contain the box
	void expand(const Box3& rhs)
	{
		m_min.x = min(m_min.x, rhs.m_min.x);
		m_min.y = min(m_min.y, rhs.m_min.y);
		m_min.z = min(m_min.z, rhs.m_min.z);

		m_max.x = max(m_max.x, rhs.m_max.x);
		m_max.y = max(m_max.y, rhs.m_max.y);
		m_max.z = max(m_max.z, rhs.m_max.z);
	}

	/// Expands this bounding box to contain the vector
	void expand(const Vec3& rhs)
	{
		m_min.x = min(m_min.x, rhs.x);
		m_min.y = min(m_min.y, rhs.y);
		m_min.z = min(m_min.z, rhs.z);

		m_max.x = max(m_max.x, rhs.x);
		m_max.y = max(m_max.y, rhs.y);
		m_max.z = max(m_max.z, rhs.z);
	}

	float width() const { return m_max.x - m_min.x; }
	float height() const { return m_max.y - m_min.y; }
	float depth() const { return m_max.z - m_min.z; }
};

class Line2
{
public:
	Line2() {}

	Line2(const Vec2& a, const Vec2& b) : start(a), end(b) {}

	Line2(float x1, float y1, float x2, float y2) : start(x1, y1), end(x2, y2) {}

	float length() const
	{
		const Vec2 c = start - end;
		return c.length();
	}

	float lengthSquared() const
	{
		const Vec2 c = start - end;
		return c.lengthSquared();
	}

	int intersect(const Line2& rhs, Vec2* pos = 0) const;

	float distance(const Vec2& pos) const;

public:
	Vec2 start;
	Vec2 end;
};

class Line3
{
public:
	Line3() {}

	Line3(const Vec3& a, const Vec3& b) : start(a), end(b) {}

	Line3(float x1, float y1, float z1, float x2, float y2, float z2) : start(x1, y1, z1), end(x2, y2, z2) {}

public:
	Vec3 start;
	Vec3 end;
};

struct Plane
{
	Vec3  n;
	float d;

	Plane() {}
	Plane(const Vec3& _n, float _d) : n(_n), d(_d) {}
	Plane(float _nx, float _ny, float _nz, float _d) : n(_nx, _ny, _nz), d(_d) {}

	Plane(const Vec3& pos, const Vec3& normal)
	{
		n = normal;
		d = -(normal.dot(pos));
	}

	float distance(const Vec3& pos) const { return dot(n, pos) + d; }

	void normalize()
	{
		float lenSquared = dot(n, n);
		float oneOverLen = 1.0f / sqrtf(lenSquared);
		n *= oneOverLen;
		d *= oneOverLen;
	}

	Vec3 intersectInfiniteLine(const Vec3& p0, const Vec3& p1);

	static bool intersect3(const Plane& a, const Plane& b, const Plane& c, Vec3& out_point);
};

enum class FrustumPlane
{
	Left   = 0,
	Right  = 1,
	Top    = 2,
	Bottom = 3,
	Near   = 4,
	Far    = 5
};

class Frustum
{
public:
	Frustum();
	Frustum(const Mat4& viewproj);
	~Frustum(){};

	const Plane& plane(FrustumPlane p) const { return m_planes[(size_t)p]; }
	void         setPlane(FrustumPlane fp, const Plane& plane);

	bool intersectSphereConservative(const Vec3& pos, float radius) const
	{
		float d;

		d = m_planes[(size_t)FrustumPlane::Left].distance(pos);
		if (d + radius < 0)
			return false;

		d = m_planes[(size_t)FrustumPlane::Right].distance(pos);
		if (d + radius < 0)
			return false;

		d = m_planes[(size_t)FrustumPlane::Top].distance(pos);
		if (d + radius < 0)
			return false;

		d = m_planes[(size_t)FrustumPlane::Bottom].distance(pos);
		if (d + radius < 0)
			return false;

		d = m_planes[(size_t)FrustumPlane::Near].distance(pos);
		if (d + radius < 0)
			return false;

		d = m_planes[(size_t)FrustumPlane::Far].distance(pos);
		if (d + radius < 0)
			return false;

		return true;
	}

	void getDepthSlicePoints(float distance, Vec3* out_points) const;
	void getNearPlanePoints(Vec3* out_points) const;
	void getFarPlanePoints(Vec3* out_points) const;

private:
	Plane m_planes[6]; // Order: left, right, top, bottom, near, far
};

class Triangle
{
public:
	Vec3 a, b, c;

	Triangle() {}
	Triangle(const Vec3& _a, const Vec3& _b, const Vec3& _c) : a(_a), b(_b), c(_c) {}

	Vec3         calculateNormal() const { return calculateNormal(a, b, c); }
	float        calculateArea() const { return calculateArea(a, b, c); }
	static float calculateArea(Vec3 a, Vec3 b, Vec3 c) { return cross(c - a, b - a).length() * 0.5f; }
	static float calculateArea(Vec2 a, Vec2 b, Vec2 c) { return fabsf(cross(c - a, b - a)) * 0.5f; }
	static Vec3  calculateNormal(Vec3 a, Vec3 b, Vec3 c) { return normalize(cross(c - a, b - a)); }
};
}
