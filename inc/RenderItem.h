#pragma once

#include <SimpleGeoApp.h>

class SimpleGeoApp::RenderItem{
public:
	RenderItem() = default;

	XMMATRIX m_ModelMatrix = XMMatrixIdentity();
	uint32_t m_NumDirtyFramse = m_NumBackBuffers;

	MeshGeometry* m_MeshGeo = nullptr;
	uint32_t m_IndexCount = 0;
	uint32_t m_StartIndexLocation = 0;
	uint32_t m_BaseVertexLocation = 0;

	D3D12_PRIMITIVE_TOPOLOGY m_PrivitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	uint32_t m_CBIndex = -1;
};