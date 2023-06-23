#pragma once

#include <DirectXMath.h>
using namespace DirectX;

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

	void BuildRootSignature();
	void BuildBoxGeometry(ComPtr<ID3D12GraphicsCommandList> commandList);
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

private:
	ComPtr<ID3DBlob> m_PixelShaderBlob;
	ComPtr<ID3DBlob> m_VertexShaderBlob;
	ComPtr<ID3D12Resource> m_BoxConstBuffer;
	uint32_t m_BoxCBSize;
	ComPtr<ID3D12DescriptorHeap> m_BoxCBDescHeap;
	ComPtr<ID3D12RootSignature> m_RootSignature;
	ComPtr<ID3D12PipelineState> m_PSO;

	ObjectConstants m_BoxMVP;
	MeshGeometry m_BoxGeo;

	float m_FoV = 45.0;
};