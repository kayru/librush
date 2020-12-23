#include "MathTypes.h"

namespace Rush
{

namespace
{
void mul33(Mat4& res, const Mat4& a, const Mat4& b)
{
	res.m(0, 0) = a.m(0, 0) * b.m(0, 0) + a.m(0, 1) * b.m(1, 0) + a.m(0, 2) * b.m(2, 0);
	res.m(0, 1) = a.m(0, 0) * b.m(0, 1) + a.m(0, 1) * b.m(1, 1) + a.m(0, 2) * b.m(2, 1);
	res.m(0, 2) = a.m(0, 0) * b.m(0, 2) + a.m(0, 1) * b.m(1, 2) + a.m(0, 2) * b.m(2, 2);

	res.m(1, 0) = a.m(1, 0) * b.m(0, 0) + a.m(1, 1) * b.m(1, 0) + a.m(1, 2) * b.m(2, 0);
	res.m(1, 1) = a.m(1, 0) * b.m(0, 1) + a.m(1, 1) * b.m(1, 1) + a.m(1, 2) * b.m(2, 1);
	res.m(1, 2) = a.m(1, 0) * b.m(0, 2) + a.m(1, 1) * b.m(1, 2) + a.m(1, 2) * b.m(2, 2);

	res.m(2, 0) = a.m(2, 0) * b.m(0, 0) + a.m(2, 1) * b.m(1, 0) + a.m(2, 2) * b.m(2, 0);
	res.m(2, 1) = a.m(2, 0) * b.m(0, 1) + a.m(2, 1) * b.m(1, 1) + a.m(2, 2) * b.m(2, 1);
	res.m(2, 2) = a.m(2, 0) * b.m(0, 2) + a.m(2, 1) * b.m(1, 2) + a.m(2, 2) * b.m(2, 2);
}

void mul(Mat4& res, const Mat4& a, const Mat4& b)
{
	res.m(0, 0) = a.m(0, 0) * b.m(0, 0) + a.m(0, 1) * b.m(1, 0) + a.m(0, 2) * b.m(2, 0) + a.m(0, 3) * b.m(3, 0);
	res.m(0, 1) = a.m(0, 0) * b.m(0, 1) + a.m(0, 1) * b.m(1, 1) + a.m(0, 2) * b.m(2, 1) + a.m(0, 3) * b.m(3, 1);
	res.m(0, 2) = a.m(0, 0) * b.m(0, 2) + a.m(0, 1) * b.m(1, 2) + a.m(0, 2) * b.m(2, 2) + a.m(0, 3) * b.m(3, 2);
	res.m(0, 3) = a.m(0, 0) * b.m(0, 3) + a.m(0, 1) * b.m(1, 3) + a.m(0, 2) * b.m(2, 3) + a.m(0, 3) * b.m(3, 3);

	res.m(1, 0) = a.m(1, 0) * b.m(0, 0) + a.m(1, 1) * b.m(1, 0) + a.m(1, 2) * b.m(2, 0) + a.m(1, 3) * b.m(3, 0);
	res.m(1, 1) = a.m(1, 0) * b.m(0, 1) + a.m(1, 1) * b.m(1, 1) + a.m(1, 2) * b.m(2, 1) + a.m(1, 3) * b.m(3, 1);
	res.m(1, 2) = a.m(1, 0) * b.m(0, 2) + a.m(1, 1) * b.m(1, 2) + a.m(1, 2) * b.m(2, 2) + a.m(1, 3) * b.m(3, 2);
	res.m(1, 3) = a.m(1, 0) * b.m(0, 3) + a.m(1, 1) * b.m(1, 3) + a.m(1, 2) * b.m(2, 3) + a.m(1, 3) * b.m(3, 3);

	res.m(2, 0) = a.m(2, 0) * b.m(0, 0) + a.m(2, 1) * b.m(1, 0) + a.m(2, 2) * b.m(2, 0) + a.m(2, 3) * b.m(3, 0);
	res.m(2, 1) = a.m(2, 0) * b.m(0, 1) + a.m(2, 1) * b.m(1, 1) + a.m(2, 2) * b.m(2, 1) + a.m(2, 3) * b.m(3, 1);
	res.m(2, 2) = a.m(2, 0) * b.m(0, 2) + a.m(2, 1) * b.m(1, 2) + a.m(2, 2) * b.m(2, 2) + a.m(2, 3) * b.m(3, 2);
	res.m(2, 3) = a.m(2, 0) * b.m(0, 3) + a.m(2, 1) * b.m(1, 3) + a.m(2, 2) * b.m(2, 3) + a.m(2, 3) * b.m(3, 3);

	res.m(3, 0) = a.m(3, 0) * b.m(0, 0) + a.m(3, 1) * b.m(1, 0) + a.m(3, 2) * b.m(2, 0) + a.m(3, 3) * b.m(3, 0);
	res.m(3, 1) = a.m(3, 0) * b.m(0, 1) + a.m(3, 1) * b.m(1, 1) + a.m(3, 2) * b.m(2, 1) + a.m(3, 3) * b.m(3, 1);
	res.m(3, 2) = a.m(3, 0) * b.m(0, 2) + a.m(3, 1) * b.m(1, 2) + a.m(3, 2) * b.m(2, 2) + a.m(3, 3) * b.m(3, 2);
	res.m(3, 3) = a.m(3, 0) * b.m(0, 3) + a.m(3, 1) * b.m(1, 3) + a.m(3, 2) * b.m(2, 3) + a.m(3, 3) * b.m(3, 3);
}
}

Mat4 Mat4::inverse() const
{
	float m00 = m(0, 0), m01 = m(0, 1), m02 = m(0, 2), m03 = m(0, 3);
	float m10 = m(1, 0), m11 = m(1, 1), m12 = m(1, 2), m13 = m(1, 3);
	float m20 = m(2, 0), m21 = m(2, 1), m22 = m(2, 2), m23 = m(2, 3);
	float m30 = m(3, 0), m31 = m(3, 1), m32 = m(3, 2), m33 = m(3, 3);

	float v0 = m20 * m31 - m21 * m30;
	float v1 = m20 * m32 - m22 * m30;
	float v2 = m20 * m33 - m23 * m30;
	float v3 = m21 * m32 - m22 * m31;
	float v4 = m21 * m33 - m23 * m31;
	float v5 = m22 * m33 - m23 * m32;

	float t00 = +(v5 * m11 - v4 * m12 + v3 * m13);
	float t10 = -(v5 * m10 - v2 * m12 + v1 * m13);
	float t20 = +(v4 * m10 - v2 * m11 + v0 * m13);
	float t30 = -(v3 * m10 - v1 * m11 + v0 * m12);

	float det  = (t00 * m00 + t10 * m01 + t20 * m02 + t30 * m03);
	float rdet = 1.0f / det;

	float d00 = t00 * rdet;
	float d10 = t10 * rdet;
	float d20 = t20 * rdet;
	float d30 = t30 * rdet;

	float d01 = -(v5 * m01 - v4 * m02 + v3 * m03) * rdet;
	float d11 = +(v5 * m00 - v2 * m02 + v1 * m03) * rdet;
	float d21 = -(v4 * m00 - v2 * m01 + v0 * m03) * rdet;
	float d31 = +(v3 * m00 - v1 * m01 + v0 * m02) * rdet;

	v0 = m10 * m31 - m11 * m30;
	v1 = m10 * m32 - m12 * m30;
	v2 = m10 * m33 - m13 * m30;
	v3 = m11 * m32 - m12 * m31;
	v4 = m11 * m33 - m13 * m31;
	v5 = m12 * m33 - m13 * m32;

	float d02 = +(v5 * m01 - v4 * m02 + v3 * m03) * rdet;
	float d12 = -(v5 * m00 - v2 * m02 + v1 * m03) * rdet;
	float d22 = +(v4 * m00 - v2 * m01 + v0 * m03) * rdet;
	float d32 = -(v3 * m00 - v1 * m01 + v0 * m02) * rdet;

	v0 = m21 * m10 - m20 * m11;
	v1 = m22 * m10 - m20 * m12;
	v2 = m23 * m10 - m20 * m13;
	v3 = m22 * m11 - m21 * m12;
	v4 = m23 * m11 - m21 * m13;
	v5 = m23 * m12 - m22 * m13;

	float d03 = -(v5 * m01 - v4 * m02 + v3 * m03) * rdet;
	float d13 = +(v5 * m00 - v2 * m02 + v1 * m03) * rdet;
	float d23 = -(v4 * m00 - v2 * m01 + v0 * m03) * rdet;
	float d33 = +(v3 * m00 - v1 * m01 + v0 * m02) * rdet;

	return Mat4(d00, d01, d02, d03, d10, d11, d12, d13, d20, d21, d22, d23, d30, d31, d32, d33);
}

Mat4 Mat4::operator*(const Mat4& rhs) const
{
	Mat4 res;
	mul(res, *this, rhs);
	return res;
}

Mat4& Mat4::operator*=(const Mat4& rhs)
{
	Mat4 tmp(*this);
	mul(*this, tmp, rhs);
	return *this;
}

float Mat4::determinant() const
{
	float res;

	float a = m(1, 1) * (m(2, 2) * m(3, 3) - m(2, 3) * m(3, 2)) - m(2, 1) * (m(1, 2) * m(3, 3) - m(1, 3) * m(3, 2)) +
	          m(3, 1) * (m(1, 2) * m(2, 3) - m(1, 3) * m(2, 2));

	float b = m(0, 1) * (m(2, 2) * m(3, 3) - m(2, 3) * m(3, 2)) - m(2, 1) * (m(0, 2) * m(3, 3) - m(0, 3) * m(3, 2)) +
	          m(3, 1) * (m(0, 2) * m(2, 3) - m(0, 3) * m(2, 2));

	float c = m(0, 1) * (m(1, 2) * m(3, 3) - m(1, 3) * m(3, 2)) - m(1, 1) * (m(0, 2) * m(3, 3) - m(0, 3) * m(3, 2)) +
	          m(3, 1) * (m(0, 2) * m(1, 3) - m(0, 3) * m(1, 2));

	float d = m(0, 1) * (m(1, 2) * m(2, 3) - m(1, 3) * m(2, 2)) - m(1, 1) * (m(0, 2) * m(2, 3) - m(0, 3) * m(2, 2)) +
	          m(2, 1) * (m(0, 2) * m(1, 3) - m(0, 3) * m(1, 2));

	res = m(0, 0) * a + m(1, 0) * b + m(2, 0) * c + m(3, 0) * d;

	return res;
}

Mat4 Mat4::multiply33(const Mat4& mat)
{
	Mat4 res;

	mul33(res, *this, mat);

	return res;
}

Mat4 Mat4::translation(const Vec3& v) { return Mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, v.x, v.y, v.z, 1); }

Mat4 Mat4::scale(const Vec3& v) { return Mat4(v.x, 0, 0, 0, 0, v.y, 0, 0, 0, 0, v.z, 0, 0, 0, 0, 1); }

Mat4 Mat4::orthographic(float w, float h, float zn, float zf)
{
	return Mat4(2.0f / w, 0, 0, 0, 0, 2.0f / h, 0, 0, 0, 0, 1.0f / (zf - zn), 0, 0, 0, zn / (zn - zf), 1.0f);
}

Mat4 Mat4::orthographicOffCenter(float l, float r, float b, float t, float zn, float zf)
{
	return Mat4(2.0f / (r - l), 0, 0, 0, 0, 2.0f / (t - b), 0, 0, 0, 0, 1.0f / (zf - zn), 0, (l + r) / (l - r),
	    (t + b) / (b - t), zn / (zn - zf), 1);
}

Mat4 Mat4::orthographicOffCenter(const Box3& bounds)
{
	return orthographicOffCenter(
	    bounds.m_min.x, bounds.m_max.x, bounds.m_min.y, bounds.m_max.y, bounds.m_min.z, bounds.m_max.z);
}

Mat4 Mat4::perspective(float aspect, float fov, float zn, float zf)
{
	float sy = 1.0f / tanf(fov * 0.5f);
	float sx = sy / aspect;

	float a = zf / (zf - zn);
	float b = -zn * a;

	Mat4 res(sx, 0, 0, 0, 0, sy, 0, 0, 0, 0, a, 1, 0, 0, b, 0);

	return res;
}

Mat4 Mat4::scaleTranslate(const Vec3& s, const Vec3& t)
{
	return Mat4(s.x, 0, 0, 0, 0, s.y, 0, 0, 0, 0, s.z, 0, t.x, t.y, t.z, 1);
}

Mat4 Mat4::rotationX(float angle)
{
	float sa = sinf(angle);
	float ca = cosf(angle);

	Mat4 res(1, 0, 0, 0, 0, ca, sa, 0, 0, -sa, ca, 0, 0, 0, 0, 1);

	return res;
}

Mat4 Mat4::rotationY(float angle)
{
	float sa = sinf(angle);
	float ca = cosf(angle);

	Mat4 res(ca, 0, -sa, 0, 0, 1, 0, 0, sa, 0, ca, 0, 0, 0, 0, 1);

	return res;
}

Mat4 Mat4::rotationZ(float angle)
{
	float sa = sinf(angle);
	float ca = cosf(angle);

	Mat4 res(ca, sa, 0, 0, -sa, ca, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

	return res;
}

Mat4 Mat4::rotationZ(const Vec3& dir)
{
	Mat4 res(dir.z, 0, -dir.x, 0, 0, 1, 0.0f, 0, dir.x, 0, dir.z, 0, 0, 0, 0, 1);

	return res;
}

Mat4 Mat4::rotationAxis(const Vec3& axis, float angle)
{
	Mat4 res;

	float sa = sinf(angle);
	float ca = cosf(angle);

	float omc = 1.0f - ca;

	float xomc = axis.x * omc;
	float yomc = axis.y * omc;
	float zomc = axis.z * omc;

	float xxomc = axis.x * xomc;
	float xyomc = axis.x * yomc;
	float xzomc = axis.x * zomc;
	float yyomc = axis.y * yomc;
	float yzomc = axis.y * zomc;
	float zzomc = axis.z * zomc;

	float xs = axis.x * sa;
	float ys = axis.y * sa;
	float zs = axis.z * sa;

	res.m(0, 0) = xxomc + ca;
	res.m(0, 1) = xyomc + zs;
	res.m(0, 2) = xzomc - ys;
	res.m(0, 3) = 0;

	res.m(1, 0) = xyomc - zs;
	res.m(1, 1) = yyomc + ca;
	res.m(1, 2) = yzomc + xs;
	res.m(1, 3) = 0;

	res.m(2, 0) = xzomc + ys;
	res.m(2, 1) = yzomc - xs;
	res.m(2, 2) = zzomc + ca;
	res.m(2, 3) = 0; //-V525

	res.m(3, 0) = 0;
	res.m(3, 1) = 0;
	res.m(3, 2) = 0;
	res.m(3, 3) = 1;

	return res;
}

Mat4 Mat4::lookAt(const Vec3& position, const Vec3& target, const Vec3& up)
{
	Vec3 x, y, z;

	z = target - position;

	x = up.cross(z);
	y = z.cross(x);
	x = y.cross(z);

	x.normalize();
	y.normalize();
	z.normalize();

	float px = -x.dot(position);
	float py = -y.dot(position);
	float pz = -z.dot(position);

	Mat4 res(x.x, y.x, z.x, 0, x.y, y.y, z.y, 0, x.z, y.z, z.z, 0, px, py, pz, 1);

	return res;
}

Frustum::Frustum(const Mat4& viewProj)
{
	m_planes[(size_t)FrustumPlane::Left].n.x = viewProj(0, 3) + viewProj(0, 0);
	m_planes[(size_t)FrustumPlane::Left].n.y = viewProj(1, 3) + viewProj(1, 0);
	m_planes[(size_t)FrustumPlane::Left].n.z = viewProj(2, 3) + viewProj(2, 0);
	m_planes[(size_t)FrustumPlane::Left].d   = viewProj(3, 3) + viewProj(3, 0);

	m_planes[(size_t)FrustumPlane::Right].n.x = viewProj(0, 3) - viewProj(0, 0);
	m_planes[(size_t)FrustumPlane::Right].n.y = viewProj(1, 3) - viewProj(1, 0);
	m_planes[(size_t)FrustumPlane::Right].n.z = viewProj(2, 3) - viewProj(2, 0);
	m_planes[(size_t)FrustumPlane::Right].d   = viewProj(3, 3) - viewProj(3, 0);

	m_planes[(size_t)FrustumPlane::Top].n.x = viewProj(0, 3) - viewProj(0, 1);
	m_planes[(size_t)FrustumPlane::Top].n.y = viewProj(1, 3) - viewProj(1, 1);
	m_planes[(size_t)FrustumPlane::Top].n.z = viewProj(2, 3) - viewProj(2, 1);
	m_planes[(size_t)FrustumPlane::Top].d   = viewProj(3, 3) - viewProj(3, 1);

	m_planes[(size_t)FrustumPlane::Bottom].n.x = viewProj(0, 3) + viewProj(0, 1);
	m_planes[(size_t)FrustumPlane::Bottom].n.y = viewProj(1, 3) + viewProj(1, 1);
	m_planes[(size_t)FrustumPlane::Bottom].n.z = viewProj(2, 3) + viewProj(2, 1);
	m_planes[(size_t)FrustumPlane::Bottom].d   = viewProj(3, 3) + viewProj(3, 1);

	m_planes[(size_t)FrustumPlane::Near].n.x = viewProj(0, 2);
	m_planes[(size_t)FrustumPlane::Near].n.y = viewProj(1, 2);
	m_planes[(size_t)FrustumPlane::Near].n.z = viewProj(2, 2);
	m_planes[(size_t)FrustumPlane::Near].d   = viewProj(3, 2);

	m_planes[(size_t)FrustumPlane::Far].n.x = viewProj(0, 3) - viewProj(0, 2);
	m_planes[(size_t)FrustumPlane::Far].n.y = viewProj(1, 3) - viewProj(1, 2);
	m_planes[(size_t)FrustumPlane::Far].n.z = viewProj(2, 3) - viewProj(2, 2);
	m_planes[(size_t)FrustumPlane::Far].d   = viewProj(3, 3) - viewProj(3, 2);

	for (u32 i = 0; i < 6; ++i)
	{
		m_planes[i].normalize();
	}
}

void Frustum::setPlane(FrustumPlane fp, const Plane& plane) { m_planes[(size_t)fp] = plane; }

void Frustum::getDepthSlicePoints(float distance, Vec3* outPoints) const
{
	const Plane& n = m_planes[(size_t)FrustumPlane::Near];
	Plane        slice(n.n, n.d - distance);

	Plane::intersect3(slice, plane(FrustumPlane::Left), plane(FrustumPlane::Top), outPoints[0]);
	Plane::intersect3(slice, plane(FrustumPlane::Right), plane(FrustumPlane::Top), outPoints[1]);
	Plane::intersect3(slice, plane(FrustumPlane::Left), plane(FrustumPlane::Bottom), outPoints[2]);
	Plane::intersect3(slice, plane(FrustumPlane::Right), plane(FrustumPlane::Bottom), outPoints[3]);
}

void Frustum::getNearPlanePoints(Vec3* outPoints) const
{
	const Plane& near_plane = m_planes[(size_t)FrustumPlane::Near];

	Plane::intersect3(near_plane, plane(FrustumPlane::Left), plane(FrustumPlane::Top), outPoints[0]);
	Plane::intersect3(near_plane, plane(FrustumPlane::Right), plane(FrustumPlane::Top), outPoints[1]);
	Plane::intersect3(near_plane, plane(FrustumPlane::Left), plane(FrustumPlane::Bottom), outPoints[2]);
	Plane::intersect3(near_plane, plane(FrustumPlane::Right), plane(FrustumPlane::Bottom), outPoints[3]);
}

void Frustum::getFarPlanePoints(Vec3* outPoints) const
{
	const Plane& far_plane = m_planes[(size_t)FrustumPlane::Far];

	Plane::intersect3(far_plane, plane(FrustumPlane::Left), plane(FrustumPlane::Top), outPoints[0]);
	Plane::intersect3(far_plane, plane(FrustumPlane::Right), plane(FrustumPlane::Top), outPoints[1]);
	Plane::intersect3(far_plane, plane(FrustumPlane::Left), plane(FrustumPlane::Bottom), outPoints[2]);
	Plane::intersect3(far_plane, plane(FrustumPlane::Right), plane(FrustumPlane::Bottom), outPoints[3]);
}

float Line2::distance(const Vec2& pos) const
{
	Vec2 v = end - start;
	Vec2 w = pos - start;

	float c1 = w.dot(v);
	if (c1 <= 0.0f)
		return (pos - start).length();

	float c2 = v.dot(v);
	if (c2 <= c1)
		return (pos - end).length();

	float b = c1 / c2;

	Vec2 pb = start + v * b;

	return (pos - pb).length();
}

int Line2::intersect(const Line2& rhs, Vec2* pos) const
{
	float d = (end.x - start.x) * (rhs.end.y - rhs.start.y) - (end.y - start.y) * (rhs.end.x - rhs.start.x);
	if (fabs(d) < 0.0001f)
	{
		return -1;
	}
	float ab =
	    ((start.y - rhs.start.y) * (rhs.end.x - rhs.start.x) - (start.x - rhs.start.x) * (rhs.end.y - rhs.start.y)) / d;
	if (ab > 0.0 && ab < 1.0)
	{
		float cd = ((start.y - rhs.start.y) * (end.x - start.x) - (start.x - rhs.start.x) * (end.y - start.y)) / d;
		if (cd > 0.0 && cd < 1.0)
		{
			if (pos)
			{
				pos->x = start.x + ab * (end.x - start.x);
				pos->y = start.y + ab * (end.y - start.y);
			}
			return 1;
		}
	}
	return 0;
}

bool Plane::intersect3(const Plane& a, const Plane& b, const Plane& c, Vec3& out_point)
{
	Vec3  num = (b.n.cross(c.n)) * a.d + (c.n.cross(a.n)) * b.d + (a.n.cross(b.n)) * c.d;
	float den = a.n.dot(b.n.cross(c.n));

	if (den != 0.0f)
	{
		out_point = -num / den;
		return true;
	}
	else
	{
		return false;
	}
}

Vec3 Plane::intersectInfiniteLine(const Vec3& a, const Vec3& b)
{
	float da = this->distance(a);
	float db = this->distance(b);

	if ((da - db) == 0.0f)
	{
		return a;
	}
	else
	{
		return a + ((b - a) * (-da / (db - da)));
	}
}

Mat4::Mat4(float m00, float m10, float m20, float m30, float m01, float m11, float m21, float m31, float m02, float m12,
    float m22, float m32, float m03, float m13, float m23, float m33)
{
	rows[0] = Vec4(m00, m10, m20, m30);
	rows[1] = Vec4(m01, m11, m21, m31);
	rows[2] = Vec4(m02, m12, m22, m32);
	rows[3] = Vec4(m03, m13, m23, m33);
}

Mat4::Mat4(float m00, float m10, float m20, float m01, float m11, float m21, float m02, float m12, float m22)
{
	rows[0] = Vec4(m00, m10, m20, 0);
	rows[1] = Vec4(m01, m11, m21, 0);
	rows[2] = Vec4(m02, m12, m22, 0);
	rows[3] = Vec4(0, 0, 0, 1);
}

Mat4::Mat4(const Mat3& m)
{
	rows[0] = Vec4(m.rows[0], 0.0f);
	rows[1] = Vec4(m.rows[1], 0.0f);
	rows[2] = Vec4(m.rows[2], 0.0f);
	rows[3] = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

Mat4::Mat4(float* arr)
{
	rows[0].x = arr[0];
	rows[0].y = arr[1];
	rows[0].z = arr[2];
	rows[0].w = arr[3];

	rows[1].x = arr[4];
	rows[1].y = arr[5];
	rows[1].z = arr[6];
	rows[1].w = arr[7];

	rows[2].x = arr[8];
	rows[2].y = arr[9];
	rows[2].z = arr[10];
	rows[2].w = arr[11];

	rows[3].x = arr[12];
	rows[3].y = arr[13];
	rows[3].z = arr[14];
	rows[3].w = arr[15];
}

Vec4 Mat4::column(size_t c) const
{
	switch (c)
	{
	case 0: return Vec4(rows[0].x, rows[1].x, rows[2].x, rows[3].x);
	case 1: return Vec4(rows[0].y, rows[1].y, rows[2].y, rows[3].y);
	case 2: return Vec4(rows[0].z, rows[1].z, rows[2].z, rows[3].z);
	case 3:
	default: return Vec4(rows[0].w, rows[1].w, rows[2].w, rows[3].w);
	}
}

Vec4 Mat4::operator*(const Vec4& v) const
{
	Vec4 res;

	res.x = v.x * m(0, 0) + v.y * m(1, 0) + v.z * m(2, 0) + v.w * m(3, 0);
	res.y = v.x * m(0, 1) + v.y * m(1, 1) + v.z * m(2, 1) + v.w * m(3, 1);
	res.z = v.x * m(0, 2) + v.y * m(1, 2) + v.z * m(2, 2) + v.w * m(3, 2);
	res.w = v.x * m(0, 3) + v.y * m(1, 3) + v.z * m(2, 3) + v.w * m(3, 3);

	return res;
}

Vec3 Mat4::operator*(const Vec3& v) const
{
	Vec3 res;

	res.x = v.x * m(0, 0) + v.y * m(1, 0) + v.z * m(2, 0) + m(3, 0);
	res.y = v.x * m(0, 1) + v.y * m(1, 1) + v.z * m(2, 1) + m(3, 1);
	res.z = v.x * m(0, 2) + v.y * m(1, 2) + v.z * m(2, 2) + m(3, 2);

	return res;
}

Mat4 Mat4::transposed() const
{
	Mat4 res;

	res.m(0, 0) = m(0, 0);
	res.m(0, 1) = m(1, 0);
	res.m(0, 2) = m(2, 0);
	res.m(0, 3) = m(3, 0);

	res.m(1, 0) = m(0, 1);
	res.m(1, 1) = m(1, 1);
	res.m(1, 2) = m(2, 1);
	res.m(1, 3) = m(3, 1);

	res.m(2, 0) = m(0, 2);
	res.m(2, 1) = m(1, 2);
	res.m(2, 2) = m(2, 2);
	res.m(2, 3) = m(3, 2);

	res.m(3, 0) = m(0, 3);
	res.m(3, 1) = m(1, 3);
	res.m(3, 2) = m(2, 3);
	res.m(3, 3) = m(3, 3);

	return res;
}

void Mat4::transpose()
{
	Mat4 res;

	res.m(0, 0) = m(0, 0);
	res.m(0, 1) = m(1, 0);
	res.m(0, 2) = m(2, 0);
	res.m(0, 3) = m(3, 0);

	res.m(1, 0) = m(0, 1);
	res.m(1, 1) = m(1, 1);
	res.m(1, 2) = m(2, 1);
	res.m(1, 3) = m(3, 1);

	res.m(2, 0) = m(0, 2);
	res.m(2, 1) = m(1, 2);
	res.m(2, 2) = m(2, 2);
	res.m(2, 3) = m(3, 2);

	res.m(3, 0) = m(0, 3);
	res.m(3, 1) = m(1, 3);
	res.m(3, 2) = m(2, 3);
	res.m(3, 3) = m(3, 3);

	*this = res;
}

Mat4 Mat4::zero() { return Mat4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); }

Mat4 Mat4::identity() { return Mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1); }

Mat4 Mat4::scaleTranslate(float sx, float sy, float sz, float tx, float ty, float tz)
{
	return scaleTranslate(Vec3(sx, sy, sz), Vec3(tx, ty, tz));
}

Mat4 Mat4::scale(float x, float y, float z) { return scale(Vec3(x, y, z)); }

Quat slerp(const Quat& x, const Quat& y, float a)
{
	// Quaternion implementation from GLM

	Quat z = y;

	float cosTheta = dot(x, y);

	if (cosTheta < 0.0f)
	{
		z.x      = -y.x;
		z.y      = -y.y;
		z.z      = -y.z;
		z.w      = -y.w;
		cosTheta = -cosTheta;
	}

	if (cosTheta > 1.0f - 1e-6f)
	{
		return Quat{lerp(x.x, z.x, a), lerp(x.y, z.y, a), lerp(x.z, z.z, a), lerp(x.w, z.w, a)};
	}
	else
	{
		float angle = acosf(cosTheta);
		return (x * sinf((1.0f - a) * angle) + z * sinf(a * angle)) / sinf(angle);
	}
}

Quat normalize(const Quat& q)
{
	// Quaternion implementation from GLM

	float len = length(q);
	if (len <= 0.0f)
	{
		return Quat{0.0f, 0.0f, 0.0f, 1.0f};
	}

	float lenRcp = 1.0f / len;
	return Quat{q.x * lenRcp, q.y * lenRcp, q.z * lenRcp, q.w * lenRcp};
}

float length(const Quat& q) { return sqrtf(dot(q, q)); }

Quat makeQuat(const Mat3& m)
{
	// Quaternion implementation from GLM

	float fourXSquaredMinus1 = m.rows[0][0] - m.rows[1][1] - m.rows[2][2];
	float fourYSquaredMinus1 = m.rows[1][1] - m.rows[0][0] - m.rows[2][2];
	float fourZSquaredMinus1 = m.rows[2][2] - m.rows[0][0] - m.rows[1][1];
	float fourWSquaredMinus1 = m.rows[0][0] + m.rows[1][1] + m.rows[2][2];

	int   biggestIndex             = 0;
	float fourBiggestSquaredMinus1 = fourWSquaredMinus1;
	if (fourXSquaredMinus1 > fourBiggestSquaredMinus1)
	{
		fourBiggestSquaredMinus1 = fourXSquaredMinus1;
		biggestIndex             = 1;
	}
	if (fourYSquaredMinus1 > fourBiggestSquaredMinus1)
	{
		fourBiggestSquaredMinus1 = fourYSquaredMinus1;
		biggestIndex             = 2;
	}
	if (fourZSquaredMinus1 > fourBiggestSquaredMinus1)
	{
		fourBiggestSquaredMinus1 = fourZSquaredMinus1;
		biggestIndex             = 3;
	}

	float biggestVal = sqrtf(fourBiggestSquaredMinus1 + 1.0f) * 0.5f;
	float mult       = 0.25f / biggestVal;

	Quat result;
	switch (biggestIndex)
	{
	default:
	case 0:
		result.w = biggestVal;
		result.x = (m.rows[1][2] - m.rows[2][1]) * mult;
		result.y = (m.rows[2][0] - m.rows[0][2]) * mult;
		result.z = (m.rows[0][1] - m.rows[1][0]) * mult;
		break;
	case 1:
		result.w = (m.rows[1][2] - m.rows[2][1]) * mult;
		result.x = biggestVal;
		result.y = (m.rows[0][1] + m.rows[1][0]) * mult;
		result.z = (m.rows[2][0] + m.rows[0][2]) * mult;
		break;
	case 2:
		result.w = (m.rows[2][0] - m.rows[0][2]) * mult;
		result.x = (m.rows[0][1] + m.rows[1][0]) * mult;
		result.y = biggestVal;
		result.z = (m.rows[1][2] + m.rows[2][1]) * mult;
		break;
	case 3:
		result.w = (m.rows[0][1] - m.rows[1][0]) * mult;
		result.x = (m.rows[2][0] + m.rows[0][2]) * mult;
		result.y = (m.rows[1][2] + m.rows[2][1]) * mult;
		result.z = biggestVal;
		break;
	}
	return result;
}

Mat3 makeMat3(const Quat& q)
{
	// Quaternion implementation from GLM

	Mat3 result{{Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)}};

	float qxx(q.x * q.x);
	float qyy(q.y * q.y);
	float qzz(q.z * q.z);
	float qxz(q.x * q.z);
	float qxy(q.x * q.y);
	float qyz(q.y * q.z);
	float qwx(q.w * q.x);
	float qwy(q.w * q.y);
	float qwz(q.w * q.z);

	result.rows[0][0] = 1.0f - 2.0f * (qyy + qzz);
	result.rows[0][1] = 2.0f * (qxy + qwz);
	result.rows[0][2] = 2.0f * (qxz - qwy);

	result.rows[1][0] = 2.0f * (qxy - qwz);
	result.rows[1][1] = 1.0f - 2.0f * (qxx + qzz);
	result.rows[1][2] = 2.0f * (qyz + qwx);

	result.rows[2][0] = 2.0f * (qxz + qwy);
	result.rows[2][1] = 2.0f * (qyz - qwx);
	result.rows[2][2] = 1.0f - 2.0f * (qxx + qyy);

	return result;
}
}