#pragma once

#include "MathTypes.h"

namespace Rush
{

class KeyboardState;
class MouseState;

class Camera
{

public:
	Camera();
	Camera(float aspect, float fov, float clipNear);
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
	Mat4 buildProjMatrix(bool reverseZ=false) const;

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

class CameraManipulator
{
public:
	enum KeyFunction
	{
		KeyFunction_MoveX_Pos,
		KeyFunction_MoveX_Neg,
		KeyFunction_MoveY_Pos,
		KeyFunction_MoveY_Neg,
		KeyFunction_MoveZ_Pos,
		KeyFunction_MoveZ_Neg,

		KeyFunction_RotateX_Pos,
		KeyFunction_RotateX_Neg,
		KeyFunction_RotateY_Pos,
		KeyFunction_RotateY_Neg,

		KeyFunction_Faster,
		KeyFunction_Slower,

		KeyFunction_COUNT
	};

	enum Mode
	{
		Mode_FirstPerson,
		Mode_Orbit
	};

	CameraManipulator();
	CameraManipulator(const KeyboardState& ks, const MouseState& ms);

	void init(const KeyboardState& ks, const MouseState& ms);

	void update(Camera* camera, float dt, const KeyboardState& ks, const MouseState& ms);

	void setMoveSpeed(float speed) { m_moveSpeed = speed; }
	void setTurnSpeed(float speed) { m_turnSpeed = speed; }

	void setMode(Mode mode) { m_mode = mode; }
	Mode getMode() const { return m_mode; }

	void setDefaultKeys();
	void setKey(KeyFunction fun, u8 key);
	u8   getKey(KeyFunction fun);

	void setUpDirection(const Vec3& v) { m_upDirection = v; }

private:

	Vec2 m_oldMousePos;
	int  m_oldMouseWheel;

	float m_moveSpeed;
	float m_turnSpeed;

	Mode m_mode;
	u8   m_keys[KeyFunction_COUNT];

	Vec3 m_upDirection = Vec3(0, 0, 1);
};

}
