#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d12.h>

#include <DirectXMath.h>
using namespace DirectX;

#include <wrl.h>
using Microsoft::WRL::ComPtr;

#include <string>
#include <unordered_map>

#include <BaseApp.h>
#include <MeshGeometry.h>

struct VertexPosNorm {
	XMFLOAT3 Position;
	XMFLOAT3 Norm;
};

struct ObjectConstants {
	XMMATRIX ModelMatrix = XMMatrixIdentity();
};

struct Light {
	XMFLOAT3 Strength;
	float FalloffStart;
	XMFLOAT3 Direction;
	float FalloffEnd;
	XMFLOAT3 Position;
	float SpotPower;
};

struct PassConstants {
	XMMATRIX View = XMMatrixIdentity();
	XMMATRIX Proj = XMMatrixIdentity();
	XMMATRIX ViewProj = XMMatrixIdentity();

	XMFLOAT3 EyePos;
	float TotalTime = 0.0f;
	float SomeConst1;
	float SomeConst2;
	float SomeConst3;
	uint32_t IsDrawNorm = 0;

	XMVECTOR AmbientLight;
	Light Lights[16];
};


struct MaterialConstants {
	XMFLOAT4 DiffuseAlbedo;
	XMFLOAT3 FresnelR0;
	float Roughness;
};

struct Material {
	std::string Name;

	uint32_t MaterialCBIndex = -1;
	uint32_t NumDirtyFrames = m_NumBackBuffers;

	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.1f, 0.1f, 0.1f };
	float Roughness = 0.25f;
};

struct RenderItem {
	RenderItem() = default;

	XMMATRIX m_ModelMatrix = XMMatrixIdentity();
	uint32_t m_NumDirtyFramse = m_NumBackBuffers;

	MeshGeometry* m_MeshGeo = nullptr;
	Material* m_Material = nullptr;

	uint32_t m_IndexCount = 0;
	uint32_t m_StartIndexLocation = 0;
	uint32_t m_BaseVertexLocation = 0;

	D3D12_PRIMITIVE_TOPOLOGY m_PrivitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	uint32_t m_CBIndex = -1;
};