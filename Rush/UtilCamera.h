#pragma once

#include "MathTypes.h"

namespace Rush
{
class Camera
{

public:
	Camera();
	Camera(float aspect, float fov, float clipNear, float clipFar);

	void setAspect(float aspect) { m_aspect = aspect; }
	void setFov(float fov) { m_fov = fov; }
	void setClip(float near, float far);

	void move(const Vec3& delta);
	void moveOnAxis(float delta, const Vec3& axis);
	void rotate(const Vec3& delta);
	void rotateOnAxis(float delta, const Vec3& axis);

	void lookAt(const Vec3& position, const Vec3& target, const Vec3& up = Vec3(0, 1, 0));

	float getNearPlane() const { return m_clipNear; }
	float getFarPlane() const { return m_clipFar; }

	float getAspect() const { return m_aspect; }
	float getFov() const { return m_fov; }

	const Vec3& getPosition() const { return m_position; }
	Vec3&       getPosition() { return m_position; }

	Mat4 buildViewMatrix() const;
	Mat4 buildProjMatrix(ProjectionFlags flags) const;

	const Vec3& getRight() const { return m_axisX; }
	const Vec3& getUp() const { return m_axisY; }
	const Vec3& getForward() const { return m_axisZ; }

	void blendTo(const Camera& other, float positionAlpha, float orientationAlpha, float parameterAlpha = 1.0f);

private:
	Vec3 m_position;

	Vec3 m_axisX; // right
	Vec3 m_axisY; // up
	Vec3 m_axisZ; // front

	float m_aspect;
	float m_fov;
	float m_clipNear;
	float m_clipFar;
};
}
