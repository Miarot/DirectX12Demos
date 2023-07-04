#include <MyD3D12Lib/Camera.h>
#include <MyD3D12Lib/Helpers.h>

Camera::Camera() :
	m_FoV(45.0f),
	m_Theta(0.0f),
	m_Phi(XM_PIDIV2),
	m_Radius(7.0f),
	m_FocusPos({ 2.0f, 0.0f, 2.0f, 1.0f }),
	m_CameraUpDirection({ 0.0f, 1.0f, 0.0f, 0.0f })
{
	UpdatePosAndDirection();
}

Camera::Camera(float fov, float theta, float phi, float radius, XMVECTOR focusPos, XMVECTOR upDir) :
	m_FoV(fov),
	m_Theta(theta),
	m_Phi(phi),
	m_Radius(radius),
	m_FocusPos(focusPos),
	m_CameraUpDirection(upDir)
{
	UpdatePosAndDirection();
}

void Camera::UpdatePosAndDirection() {
	// spherical coordinates to cartesian
	float x = m_Radius * sinf(m_Phi) * cosf(m_Theta);
	float z = m_Radius * sinf(m_Phi) * sinf(m_Theta);
	float y = m_Radius * cosf(m_Phi);

	m_CameraPos = m_FocusPos + XMVectorSet(x, y, z, 1);
	m_CameraForwardDirection = -XMVector3Normalize(XMVectorSet(x, y, z, 0));
	m_CameraRightDirection = XMVector3Normalize(XMVector3Cross(m_CameraForwardDirection, m_CameraUpDirection));

	m_IsViewMatrixChanged = true;
}

XMMATRIX Camera::GetViewMatrix() {
	if (m_IsViewMatrixChanged) {
		m_ViewMatrix = XMMatrixLookAtLH(m_CameraPos, m_FocusPos, m_CameraUpDirection);
		m_IsViewMatrixChanged = false;
	}

	return m_ViewMatrix;
}

float Camera::GetFoV() const {
	return m_FoV;
}

XMVECTOR Camera::GetCameraPos() const {
	return m_CameraPos;
}

void Camera::MoveCamera(WPARAM direction) {
	m_CameraPos -= m_FocusPos;

	switch (direction) 
	{
	case 'W':
		// Forward
		m_FocusPos += 0.4 * m_CameraForwardDirection;
		break;
	case 'S':
		// Backward
		m_FocusPos -= 0.4 * m_CameraForwardDirection;
		break;
	case 'A':
		// Left
		m_FocusPos += 0.4 * m_CameraRightDirection;
		break;
	case 'D':
		// Right
		m_FocusPos -= 0.4 * m_CameraRightDirection;
		break;
	}

	m_CameraPos += m_FocusPos;
	m_IsViewMatrixChanged = true;
}

void Camera::RotateCamera(int dx, int dy) {
	float dTheta = XMConvertToRadians(0.25f * static_cast<float>(dx));
	float dPhi = XMConvertToRadians(0.25f * static_cast<float>(dy));

	m_Theta -= dTheta;
	m_Phi -= dPhi;
	m_Phi = clamp(m_Phi, 0.1f, XM_PI - 0.1f);
	
	UpdatePosAndDirection();
}

void Camera::ChangeRadius(int dx, int dy) {
	m_Radius += 0.005f * (dx - dy);
	m_Radius = clamp(m_Radius, 3.0f, 15.0f);

	UpdatePosAndDirection();
}

void Camera::ChangeFoV(int wheelDelta) {
	m_FoV += wheelDelta / 10.0f;
	m_FoV = clamp(m_FoV, 12.0f, 90.0f);
}