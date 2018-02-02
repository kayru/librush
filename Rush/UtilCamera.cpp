#include "UtilCamera.h"

namespace Rush
{

Camera::Camera() : Camera(1.0f, Pi * 0.25f, 1.0f, 1000.0f) {}

Camera::Camera(float aspect, float fov, float clip_near, float clip_far)
: m_position(0, 0, 0)
, m_axisX(1, 0, 0)
, m_axisY(0, 1, 0)
, m_axisZ(0, 0, 1)
, m_aspect(aspect)
, m_fov(fov)
, m_clipNear(clip_near)
, m_clipFar(clip_far)
{
}

Mat4 Camera::buildViewMatrix() const
{
	float x = -m_axisX.dot(m_position);
	float y = -m_axisY.dot(m_position);
	float z = -m_axisZ.dot(m_position);

	Mat4 res(m_axisX.x, m_axisY.x, m_axisZ.x, 0, m_axisX.y, m_axisY.y, m_axisZ.y, 0, m_axisX.z, m_axisY.z, m_axisZ.z, 0,
	    x, y, z, 1);

	return res;
}

Mat4 Camera::buildProjMatrix(ProjectionFlags flags) const
{
	return Mat4::perspective(m_aspect, m_fov, m_clipNear, m_clipFar, flags);
}

void Camera::blendTo(const Camera& other, float positionAlpha, float orientationAlpha, float parameterAlpha)
{
	m_position = lerp(m_position, other.m_position, positionAlpha);

	m_aspect   = lerp(m_aspect, other.m_aspect, parameterAlpha);
	m_fov      = lerp(m_fov, other.m_fov, parameterAlpha);
	m_clipNear = lerp(m_clipNear, other.m_clipNear, parameterAlpha);
	m_clipFar  = lerp(m_clipFar, other.m_clipFar, parameterAlpha);

	Mat3 orientationA{{Vec3{m_axisX.x, m_axisY.x, m_axisZ.x}, Vec3{m_axisX.y, m_axisY.y, m_axisZ.y},
	    Vec3{m_axisX.z, m_axisY.z, m_axisZ.z}}};

	Mat3 orientationB{{Vec3{other.m_axisX.x, other.m_axisY.x, other.m_axisZ.x},
	    Vec3{other.m_axisX.y, other.m_axisY.y, other.m_axisZ.y},
	    Vec3{other.m_axisX.z, other.m_axisY.z, other.m_axisZ.z}}};

	Quat quatA = makeQuat(orientationA);
	Quat quatB = makeQuat(orientationB);

	Quat quat = normalize(slerp(quatA, quatB, orientationAlpha));

	Mat3 orientation = transpose(makeMat3(quat));

	m_axisX = orientation.rows[0];
	m_axisY = orientation.rows[1];
	m_axisZ = orientation.rows[2];
}

void Camera::move(const Vec3& delta)
{
	m_position += m_axisX * delta.x;
	m_position += m_axisY * delta.y;
	m_position += m_axisZ * delta.z;
}

void Camera::moveOnAxis(float delta, const Vec3& axis) { m_position += axis * delta; }

void Camera::rotate(const Vec3& delta)
{
	Mat4 mx = Mat4::rotationAxis(m_axisX, delta.x);
	Mat4 my = Mat4::rotationAxis(m_axisY, delta.y);
	Mat4 mz = Mat4::rotationAxis(m_axisZ, delta.z);

	Mat4 mat(mx * my * mz);

	m_axisX = mat * m_axisX;
	m_axisY = mat * m_axisY;
	m_axisZ = mat * m_axisZ;

	m_axisX.normalize();
	m_axisY.normalize();
	m_axisZ.normalize();
}

void Camera::rotateOnAxis(float delta, const Vec3& axis)
{
	Mat4 mat = Mat4::rotationAxis(axis, delta);

	m_axisX = mat * m_axisX;
	m_axisY = mat * m_axisY;
	m_axisZ = mat * m_axisZ;

	m_axisX.normalize();
	m_axisY.normalize();
	m_axisZ.normalize();
}

void Camera::lookAt(const Vec3& position, const Vec3& target, const Vec3& up)
{
	m_position = position;

	m_axisZ = target - position;
	m_axisZ.normalize();

	m_axisX = normalize(up).cross(m_axisZ);
	m_axisY = m_axisZ.cross(m_axisX);
	m_axisX = m_axisY.cross(m_axisZ);

	m_axisX.normalize();
	m_axisY.normalize();
	m_axisZ.normalize();
}

void Camera::setClip(float near_dist, float far_dist)
{
	m_clipNear = near_dist;
	m_clipFar  = far_dist;
}
}
