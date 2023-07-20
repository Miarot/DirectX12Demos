#pragma once

#include <MyD3D12Lib/BaseApp.h>
#include <MyD3D12Lib/MeshGeometry.h>

#include <DirectXMath.h>
using namespace DirectX;



struct Vertex {
	XMFLOAT3 Position;
	XMFLOAT3 Norm;
	XMFLOAT2 TexC;
	XMFLOAT3 TangentU;
};

struct ObjectConstants {
	XMMATRIX ModelMatrix = XMMatrixIdentity();
	XMMATRIX ModelMatrixInvTrans = XMMatrixIdentity();
};

struct Light {
	XMFLOAT3 Strength;
	float FalloffStart;
	XMFLOAT3 Direction;
	float FalloffEnd;
	XMFLOAT3 Position;
	float SpotPower;

	XMMATRIX LightViewProj = XMMatrixIdentity();
	XMMATRIX LightViewProjTex = XMMatrixIdentity();
};

struct PassConstants {
	XMMATRIX View = XMMatrixIdentity();
	XMMATRIX Proj = XMMatrixIdentity();
	XMMATRIX ViewProj = XMMatrixIdentity();
	XMMATRIX ProjInv = XMMatrixIdentity();
	XMMATRIX ProjTex = XMMatrixIdentity();
	XMMATRIX ViewProjTex = XMMatrixIdentity();

	XMFLOAT3 EyePos;
	float TotalTime = 0.0f;

	XMVECTOR AmbientLight;
	Light Lights[16];

	XMVECTOR RandomDirections[14];
	float OcclusionRadius = 0.2f;
	float OcclusionFadeStart = 0.1f;
	float OcclusionFadeEnd = 0.5f;
	float OcclusionEpsilon = 0.01;

	float OcclusionMapWidthInv;
	float OcclusionMapHeightInv;
};

struct MaterialConstants {
	XMFLOAT4 DiffuseAlbedo;
	XMFLOAT3 FresnelR0;
	float Roughness;
};

struct Material {
	std::string Name;

	uint32_t CBIndex = -1;
	uint32_t NumDirtyFrames = m_NumBackBuffers;

	std::string DiffuseTexName = "default";
	std::string NormalMapTexName = "default";

	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.0f, 0.0f, 0.0f };
	float Roughness = 1.0f;
};

struct RenderItem {
	RenderItem() = default;

	XMMATRIX m_ModelMatrix = XMMatrixIdentity();
	XMMATRIX m_ModelMatrixInvTrans = XMMatrixIdentity();
	uint32_t m_NumDirtyFramse = m_NumBackBuffers;

	MeshGeometry* m_MeshGeo = nullptr;
	Material* m_Material = nullptr;

	uint32_t m_IndexCount = 0;
	uint32_t m_StartIndexLocation = 0;
	uint32_t m_BaseVertexLocation = 0;

	D3D12_PRIMITIVE_TOPOLOGY m_PrivitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	uint32_t m_CBIndex = -1;
};

struct Texture {
	std::string Name;
	std::wstring FileName;

	ComPtr<ID3D12Resource> Resource;
	ComPtr<ID3D12Resource> UploadResource;
	uint32_t SRVHeapIndex = -1;
};