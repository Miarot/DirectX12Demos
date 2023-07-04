#pragma once

#include <AppStructures.h>
#include <FrameResources.h>
#include <MyD3D12Lib/BaseApp.h>
#include <MyD3D12Lib/Camera.h>
#include <MyD3D12Lib/MeshGeometry.h>
#include <MyD3D12Lib/Timer.h>
#include <MyD3D12Lib/UploadBuffer.h>

#include <DirectXTex/DDSTextureLoader/DDSTextureLoader12.h>

#include <DirectXMath.h>
using namespace DirectX;

#include <map>

constexpr uint32_t m_NumLights = 16;

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
	void BuildLights();
	void BuildTextures(ComPtr<ID3D12GraphicsCommandList> commandList);
	void BuildGeometry(ComPtr<ID3D12GraphicsCommandList> commandList);
	void BuildMaterials();
	void BuildRenderItems();
	void BuildFrameResources();
	void BuildSRViews();
	void BuildCBViews();
	void BuildPipelineStateObject();

	// for Sobel filter
	void UpdateFramesTextures();
	void BuildSobelRootSignature();
	void BuildSobelPipelineStateObject();

	XMMATRIX GetProjectionMatrix();

	void CreateDDSTextureFromFile(ComPtr<ID3D12GraphicsCommandList> commandList, Texture* tex);

private:
	ComPtr<ID3DBlob> m_GeoVertexShaderBlob;
	ComPtr<ID3DBlob> m_GeoPixelShaderBlob;
	ComPtr<ID3DBlob> m_NormPixelShaderBlob;
	std::vector<std::unique_ptr<FrameResources>> m_FramesResources;
	FrameResources* m_CurrentFrameResources;
	ComPtr<ID3D12DescriptorHeap> m_CBV_SRVDescHeap;
	uint32_t m_TexturesViewsStartIndex;
	uint32_t m_ObjectConstantsViewsStartIndex;
	uint32_t m_PassConstantsViewsStartIndex;
	uint32_t m_MaterialConstantsViewsStartIndex;
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

	std::unordered_map<std::string, std::unique_ptr<Texture>> m_Textures;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_Geometries;
	std::unordered_map <std::string, std::unique_ptr<Material>> m_Materials;
	std::vector<std::unique_ptr<RenderItem>> m_RenderItems;

	bool m_IsInverseDepth = false;
	bool m_IsDrawNorm = false;

	PassConstants m_PassConstants;
	POINT m_LastMousePos;
	Camera m_Camera;
	Timer m_Timer;

	// for shake effect
	bool m_IsShakeEffect;
	float m_ShakePixelAmplitude;
	std::vector<XMVECTOR> m_ShakeDirections;
	size_t m_ShakeDirectionIndex;
};