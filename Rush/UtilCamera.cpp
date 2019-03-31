#include "UtilCamera.h"
#include "UtilLog.h"
#include "Window.h"

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

CameraManipulator::CameraManipulator() : CameraManipulator(KeyboardState{}, MouseState{}) {}

CameraManipulator::CameraManipulator(const KeyboardState& ks, const MouseState& ms)
	: m_oldMousePos(0, 0), m_oldMouseWheel(0), m_moveSpeed(20.0f), m_turnSpeed(2.0f), m_mode(Mode_FirstPerson)
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
	m_oldMousePos = ms.pos;
	m_oldMouseWheel = ms.wheelV;
}

void CameraManipulator::update(Camera* camera, float dt, const KeyboardState& ks, const MouseState& ms)
{
	RUSH_ASSERT(camera);

	Vec2 mousePos = ms.pos;
	Vec2 mouseDelta = mousePos - m_oldMousePos;
	m_oldMousePos = mousePos;

	int mouseWheel = ms.wheelV;
	int wheelDelta = mouseWheel - m_oldMouseWheel;
	m_oldMouseWheel = mouseWheel;

	switch (m_mode)
	{
	case Mode_FirstPerson:
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
			Vec3 old_cam_up = camera->getUp();

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
			Vec2 mp = mouseDelta * 5.0f;
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
