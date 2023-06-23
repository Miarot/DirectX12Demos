#pragma once

#include <DirectXMath.h>
using namespace DirectX;

#include <map>

#include <BaseApp.h>
#include <MeshGeometry.h>

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

	void BuildRootSignature();
	void BuildBoxGeometry(ComPtr<ID3D12GraphicsCommandList> commandList);
	void BuildBoxConstantBuffer();
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
	ComPtr<ID3D12Resource> m_BoxConstBuffer;
	uint32_t m_BoxCBSize;
	ComPtr<ID3D12DescriptorHeap> m_BoxCBDescHeap;
	ComPtr<ID3D12RootSignature> m_RootSignature;
	std::map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;

	ObjectConstants m_BoxMVP;
	MeshGeometry m_BoxGeo;

	float m_FoV = 45.0;
	bool m_IsInverseDepth = false;

	// shake effect state data
	bool m_IsShakeEffect = false;
	float m_ShakePixelAmplitude;
	std::vector<XMVECTOR> m_ShakeDirections;
	size_t m_ShakeDirectionIndex;

	// mouse position
	POINT m_LastMousePos;

	// camera state
	float m_Theta = 1.5f * XM_PI;
	float m_Phi = XM_PIDIV4;
	float m_Radius = 5.0f;
	XMVECTOR m_CameraPos;
	XMVECTOR m_FocusPos = { 0.0f, 0.0f, 0.0f, 1.0f };
	XMVECTOR m_CameraUpDirection = { 0.0f, 1.0f, 0.0f, 0.0f };
	XMVECTOR m_CameraForwardDirection;
	XMVECTOR m_CameraRightDirection;
};