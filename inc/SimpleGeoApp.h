#pragma once

#include <DirectXMath.h>
using namespace DirectX;

#include <map>

#include <BaseApp.h>
#include <MeshGeometry.h>
#include <Camera.h>
#include <Timer.h>
#include <UploadBuffer.h>

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
	void BuildGeometry(ComPtr<ID3D12GraphicsCommandList> commandList);
	void BuildRenderItems();
	void BuildFrameResources();
	void BuildObjectsAndPassConstantsBufferViews();
	void BuildPipelineStateObject();

	// for Sobel filter
	void UpdateFramesTextures();
	void BuildSobelRootSignature();
	void BuildSobelPipelineStateObject();

	// Vertex Shader input data structure
	struct VertexPosColor {
		XMFLOAT3 Position;
		XMFLOAT3 Color;
	};

	// Vertex Shader parameter data structure
	struct ObjectConstants {
		XMMATRIX ModelMatrix = XMMatrixIdentity();
	};

	struct PassConstants {
		float TotalTime = 0.0;

		XMMATRIX View = XMMatrixIdentity();
		XMMATRIX Proj = XMMatrixIdentity();
		XMMATRIX ViewProj = XMMatrixIdentity();
	};

	class FrameResources;

	class RenderItem;

	XMMATRIX GetProjectionMatrix();

private:
	ComPtr<ID3DBlob> m_PixelShaderBlob;
	ComPtr<ID3DBlob> m_VertexShaderBlob;
	std::vector<std::unique_ptr<FrameResources>> m_FramesResources;
	FrameResources* m_CurrentFrameResources;
	ComPtr<ID3D12DescriptorHeap> m_CBDescHeap;
	ComPtr<ID3D12RootSignature> m_RootSignature;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;

	// for Sobel filter
	bool m_IsSobelFilter = false;
	ComPtr<ID3D12Resource> m_FrameTexturesBuffers;
	ComPtr<ID3D12DescriptorHeap> m_FrameTextureRTVDescHeap;
	ComPtr<ID3D12DescriptorHeap> m_FrameTextureSRVDescHeap;
	ComPtr<ID3D12RootSignature> m_SobelRootSignature;
	ComPtr<ID3DBlob> m_SobelPixelShaderBlob;
	ComPtr<ID3DBlob> m_SobelVertexShaderBlob;
	ComPtr<ID3D12PipelineState> m_SobelPSO;

	FLOAT m_BackGroundColor[4] = {0.4f, 0.6f, 0.9f, 1.0f};

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_Geometries;
	std::vector<std::unique_ptr<RenderItem>> m_RenderItems;

	bool m_IsInverseDepth = false;

	POINT m_LastMousePos;
	Camera m_Camera;
	Timer m_Timer;

	// for shake effect
	bool m_IsShakeEffect;
	float m_ShakePixelAmplitude;
	std::vector<XMVECTOR> m_ShakeDirections;
	size_t m_ShakeDirectionIndex;
};