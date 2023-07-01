#pragma once

#include <SimpleGeoApp.h>

class SimpleGeoApp::Material {
public:
	std::string Name;

	uint32_t MaterialCBIndex = -1;
	uint32_t NumDirtyFrames = m_NumBackBuffers;

	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.1f, 0.1f, 0.1f };
	float Roughness = 0.25f;
};