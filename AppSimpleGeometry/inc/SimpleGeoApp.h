#pragma once

#include <AppStructures.h>
#include <FrameResources.h>
#include <MyD3D12Lib/BaseApp.h>
#include <MyD3D12Lib/Camera.h>
#include <MyD3D12Lib/MeshGeometry.h>
#include <MyD3D12Lib/Shaker.h>
#include <MyD3D12Lib/Timer.h>
#include <MyD3D12Lib/UploadBuffer.h>

#include <DirectXMath.h>

#include <map>

class SimpleGeoApp : public BaseApp {
public:
	explicit SimpleGeoApp(HINSTANCE hInstance);

	SimpleGeoApp(const SimpleGeoApp& other) = delete;
	SimpleGeoApp& operator= (const SimpleGeoApp& other) = delete;

	~SimpleGeoApp();

private:
	virtual bool Initialize() override;
	virtual void OnUpdate() override;
	virtual void OnRender() override;
	virtual void OnResize() override;
	virtual void OnKeyPressed(WPARAM wParam) override;
	virtual void OnMouseWheel(int wheelDelta) override;
	virtual void OnMouseDown(WPARAM wParam, int x, int y) override;
	virtual void OnMouseUp(WPARAM wParam, int x, int y) override;
	virtual void OnMouseMove(WPARAM wParam, int x, int y) override;

	void SimpleGeoApp::RenderRenderItem(ComPtr<ID3D12GraphicsCommandList> commandList, RenderItem* renderItem);

	void InitSceneState();
	void BuildLights();
	void BuildTextures(ComPtr<ID3D12GraphicsCommandList> commandList);
	void BuildGeometry(ComPtr<ID3D12GraphicsCommandList> commandList);
	void BuildMaterials();
	void BuildRenderItems();
	void BuildFrameResources();
	void BuildSRViews();
	void BuildCBViews();
	void BuildRootSignature();
	void BuildPipelineStateObject();

	// for Sobel filter
	void UpdateFramesTextures();
	void BuildSobelRootSignature();
	void BuildSobelPipelineStateObject();

private:
	FLOAT m_BackGroundColor[4] = { 0.4f, 0.6f, 0.9f, 1.0f };

	bool m_IsInverseDepth = false;
	bool m_IsShakeEffect = false;
	bool m_IsDrawNorm = false;
	PassConstants m_PassConstants;
	POINT m_LastMousePos;
	Camera m_Camera;
	Timer m_Timer;
	Shaker m_Shaker;

	std::unordered_map<std::string, std::unique_ptr<Texture>> m_Textures;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_Geometries;
	std::unordered_map <std::string, std::unique_ptr<Material>> m_Materials;
	std::vector<std::unique_ptr<RenderItem>> m_AllRenderItems;
	std::vector<RenderItem *> m_OpaqueRenderItems;
	std::vector<RenderItem *> m_TransparentRenderItems;
	RenderItem* m_GroundRenderItem;
	DirectX::XMMATRIX m_GroundProjectiveMatrix = DirectX::XMMatrixIdentity();
	std::vector<RenderItem *> m_ShadowsRenderItems;
	std::vector<std::unique_ptr<FrameResources>> m_FramesResources;
	FrameResources* m_CurrentFrameResources;

	ComPtr<ID3D12DescriptorHeap> m_CBV_SRVDescHeap;
	uint32_t m_TexturesViewsStartIndex;
	uint32_t m_ObjectConstantsViewsStartIndex;
	uint32_t m_PassConstantsViewsStartIndex;
	uint32_t m_MaterialConstantsViewsStartIndex;

	std::unordered_map<std::string, ComPtr<ID3D12RootSignature>> m_RootSignatures;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;

	// for Sobel filter
	bool m_IsSobelFilter = false;
	ComPtr<ID3D12Resource> m_FrameTexturesBuffers;
	ComPtr<ID3D12DescriptorHeap> m_FrameTextureRTVDescHeap;
	ComPtr<ID3D12DescriptorHeap> m_FrameTextureSRVDescHeap;
};