#pragma once

#include <DirectXMath.h>
using namespace DirectX;

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

class Camera {
public:
	Camera();
	Camera(float fov, float theta, float phi, float radius, XMVECTOR focusPos, XMVECTOR upDir);

	void UpdatePosAndDirection();

	XMMATRIX GetViewMatrix();
	float GetFoV() const;
	XMVECTOR GetCameraPos() const;

	void MoveCamera(WPARAM direction);
	void RotateCamera(int dx, int dy);
	void ChangeRadius(int dx, int dy);
	void ChangeFoV(int wheelDelta);

private:
	float m_FoV;
	float m_Theta;
	float m_Phi;
	float m_Radius;
	bool m_IsViewMatrixChanged;
	XMVECTOR m_CameraPos;
	XMVECTOR m_FocusPos;
	XMVECTOR m_CameraUpDirection;
	XMVECTOR m_CameraForwardDirection;
	XMVECTOR m_CameraRightDirection;
	XMMATRIX m_ViewMatrix;
};