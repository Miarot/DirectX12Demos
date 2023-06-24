#pragma once

#include <DirectXMath.h>
using namespace DirectX;

#include <map>

#include <BaseApp.h>
#include <MeshGeometry.h>
#include <Camera.h>

class SimpleGeoApp : public BaseApp {
public:
	explicit SimpleGeoApp(HINSTANCE hInstance);

	SimpleGeoApp(const SimpleGeoApp& other) = delete;
	SimpleGeoApp& operator= (const SimpleGeoApp& other) = delete;

	~SimpleGeoApp();

	virtual bool Initialize() override;

private:
	virtual void OnUpdate() override;
	virtual void OnRender() override;
	virtual void OnResize() override;
	virtual void OnKeyPressed(WPARAM wParam) override;
	virtual void OnMouseWheel(int wheelDelta) override;
	virtual void OnMouseDown(WPARAM wParam, int x, int y) override;
	virtual void OnMouseUp(WPARAM wParam, int x, int y) override;
	virtual void OnMouseMove(WPARAM wParam, int x, int y) override;

	void InitAppState();
	void BuildRootSignature();
	void BuildBoxAndPiramidGeometry(ComPtr<ID3D12GraphicsCommandList> commandList);
	void BuildPiramidGeometry(ComPtr<ID3D12GraphicsCommandList> commandList);
	void BuildGeoConstantBufferAndViews();
	void BuildPipelineStateObject();

	// Vertex Shader input data structure
	struct VertexPosColor {
		XMFLOAT3 Position;
		XMFLOAT3 Color;
	};

	// Vertex Shader parameter data structure
	struct ObjectConstants {
		XMMATRIX MVP = XMMatrixIdentity();
	};

	XMMATRIX GetProjectionMatrix();

private:
	ComPtr<ID3DBlob> m_PixelShaderBlob;
	ComPtr<ID3DBlob> m_VertexShaderBlob;
	ComPtr<ID3D12Resource> m_GeoConstBuffer;
	uint32_t m_GeoCBSize;
	uint32_t m_PiramidCBSize;
	ComPtr<ID3D12DescriptorHeap> m_GeoCBDescHeap;
	ComPtr<ID3D12RootSignature> m_RootSignature;
	std::map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;

	const uint32_t m_NumGeo = 2;
	MeshGeometry m_BoxAndPiramidGeo;
	ObjectConstants m_BoxMVP;
	ObjectConstants m_PiramidMVP;

	bool m_IsInverseDepth;

	// shake effect state data
	bool m_IsShakeEffect;
	float m_ShakePixelAmplitude;
	std::vector<XMVECTOR> m_ShakeDirections;
	size_t m_ShakeDirectionIndex;

	// mouse position
	POINT m_LastMousePos;

	Camera m_Camera;
};