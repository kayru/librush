#include "UtilCameraManipulator.h"
#include "Platform.h"
#include "UtilCamera.h"
#include "UtilLog.h"
#include "Window.h"

namespace Rush
{
CameraManipulator::CameraManipulator() : CameraManipulator(KeyboardState{}, MouseState{}) {}

CameraManipulator::CameraManipulator(const KeyboardState& ks, const MouseState& ms)
: m_oldMousePos(0, 0), m_oldMouseWheel(0), m_moveSpeed(20.0f), m_turnSpeed(2.0f), m_mode(Mode_FPS)
{
	init(ks, ms);
	setDefaultKeys();
}

void CameraManipulator::setKey(KeyFunction fun, u8 key) { m_keys[fun] = key; }

u8 CameraManipulator::getKey(KeyFunction fun) { return m_keys[fun]; }

void CameraManipulator::setDefaultKeys()
{
	setKey(KeyFunction_MoveX_Pos, 'D');
	setKey(KeyFunction_MoveX_Neg, 'A');
	setKey(KeyFunction_MoveY_Pos, 'E');
	setKey(KeyFunction_MoveY_Neg, 'Q');
	setKey(KeyFunction_MoveZ_Pos, 'W');
	setKey(KeyFunction_MoveZ_Neg, 'S');

	setKey(KeyFunction_RotateX_Pos, Key_Up);
	setKey(KeyFunction_RotateX_Neg, Key_Down);
	setKey(KeyFunction_RotateY_Pos, Key_Right);
	setKey(KeyFunction_RotateY_Neg, Key_Left);

	setKey(KeyFunction_Faster, Key_LeftShift);
	setKey(KeyFunction_Slower, Key_LeftControl);
}

void CameraManipulator::init(const KeyboardState& ks, const MouseState& ms)
{
	m_oldMousePos   = ms.pos;
	m_oldMouseWheel = ms.wheelV;
}

void CameraManipulator::update(Camera* camera, float dt, const KeyboardState& ks, const MouseState& ms)
{
	RUSH_ASSERT(camera);

	Vec2 mousePos   = ms.pos;
	Vec2 mouseDelta = mousePos - m_oldMousePos;
	m_oldMousePos   = mousePos;

	int mouseWheel  = ms.wheelV;
	int wheelDelta  = mouseWheel - m_oldMouseWheel;
	m_oldMouseWheel = mouseWheel;

	switch (m_mode)
	{
	case Mode_FPS:
	{
		Vec3 camMove(0, 0, 0);
		Vec3 camRotate(0, 0, 0);

		if (ks.isKeyDown(getKey(KeyFunction_MoveX_Pos)))
			camMove.x = 1;
		if (ks.isKeyDown(getKey(KeyFunction_MoveX_Neg)))
			camMove.x = -1;

		if (ks.isKeyDown(getKey(KeyFunction_MoveY_Pos)))
			camMove.y = 1;
		if (ks.isKeyDown(getKey(KeyFunction_MoveY_Neg)))
			camMove.y = -1;

		if (ks.isKeyDown(getKey(KeyFunction_MoveZ_Pos)))
			camMove.z = 1;
		if (ks.isKeyDown(getKey(KeyFunction_MoveZ_Neg)))
			camMove.z = -1;

		if (ks.isKeyDown(getKey(KeyFunction_RotateY_Pos)))
			camRotate.y = dt * m_turnSpeed;
		if (ks.isKeyDown(getKey(KeyFunction_RotateY_Neg)))
			camRotate.y = -dt * m_turnSpeed;

		if (ks.isKeyDown(getKey(KeyFunction_RotateX_Pos)))
			camRotate.x = -dt * m_turnSpeed;
		if (ks.isKeyDown(getKey(KeyFunction_RotateX_Neg)))
			camRotate.x = dt * m_turnSpeed;

		if ((ms.buttons[0]) != 0)
		{
			camRotate.y = mouseDelta.x * 0.005f;
			camRotate.x = mouseDelta.y * 0.005f;
		}

		if (camMove != Vec3(0, 0, 0))
		{
			camMove.normalize();
			if (ks.isKeyDown(getKey(KeyFunction_Faster)))
				camMove *= 10.0f;
			if (ks.isKeyDown(getKey(KeyFunction_Slower)))
				camMove *= 0.1f;
			camera->move(camMove * dt * m_moveSpeed);
		}

		float rot_len = camRotate.length();
		if (rot_len > 0)
		{
			camera->rotateOnAxis(camRotate.y, Vec3(0, 1, 0));
			camera->rotateOnAxis(camRotate.x, camera->getRight());
		}

		break;
	}
	case Mode_Orbit:
	{
		Vec3 camMove(0, 0, 0);

		if (ks.isKeyDown(getKey(KeyFunction_MoveX_Pos)))
			camMove.x = 1;
		if (ks.isKeyDown(getKey(KeyFunction_MoveX_Neg)))
			camMove.x = -1;

		if (ks.isKeyDown(getKey(KeyFunction_MoveY_Pos)))
			camMove.y = 1;
		if (ks.isKeyDown(getKey(KeyFunction_MoveX_Neg)))
			camMove.y = -1;

		if (ks.isKeyDown(getKey(KeyFunction_MoveZ_Pos)))
			camMove.z = 1;
		if (ks.isKeyDown(getKey(KeyFunction_MoveZ_Neg)))
			camMove.z = -1;

		if (ms.buttons[0] != 0)
		{
			Vec3 old_cam_pos = camera->getPosition();
			Vec3 old_cam_dir = camera->getForward();
			Vec3 old_cam_up  = camera->getUp();

			float orbitRadius = 1.0f;
			Vec3  orbitCenter = old_cam_pos + old_cam_dir * orbitRadius;

			Camera tmp(1, 1, 1, 1);
			tmp.lookAt(-old_cam_dir, Vec3(0, 0, 0), old_cam_up);
			tmp.rotateOnAxis(mouseDelta.x * 0.01f, Vec3(0, 1, 0));
			tmp.rotateOnAxis(mouseDelta.y * 0.01f, tmp.getRight());

			camera->lookAt(-tmp.getForward() * orbitRadius + orbitCenter, orbitCenter, tmp.getUp());
		}
		else if (ms.buttons[1] != 0)
		{
			// move
			Vec2 mp   = mouseDelta * 5.0f;
			camMove.x = mp.x * -1;
			camMove.y = mp.y;
		}

		if (wheelDelta)
		{
			camMove.z += float(wheelDelta) * 3.0f;
		}

		if (camMove != Vec3(0, 0, 0))
		{
			if (ks.isKeyDown(getKey(KeyFunction_Faster)))
				camMove *= 10.0f;
			if (ks.isKeyDown(getKey(KeyFunction_Slower)))
				camMove *= 0.1f;
			camera->move(camMove * dt * m_moveSpeed);
		}
		break;
	}
	}
}
}
