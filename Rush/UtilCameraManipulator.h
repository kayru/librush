#pragma once

#include "MathTypes.h"

namespace Rush
{

class Camera;
class KeyboardState;
class MouseState;

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
		Mode_FPS,
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

private:
	Vec2 m_oldMousePos;
	int  m_oldMouseWheel;

	float m_moveSpeed;
	float m_turnSpeed;

	Mode m_mode;
	u8   m_keys[KeyFunction_COUNT];
};
}
