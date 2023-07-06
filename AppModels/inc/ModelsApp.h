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
using namespace DirectX;

#include <assimp/scene.h>

#include <map>
#include <filesystem>

class ModelsApp : public BaseApp {
public:
	explicit ModelsApp(HINSTANCE hInstance);

	ModelsApp(const ModelsApp& other) = delete;
	ModelsApp& operator= (const ModelsApp& other) = delete;

	~ModelsApp();

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

	void InitSceneState();
	void BuildLights();
	void BuildTextures(ComPtr<ID3D12GraphicsCommandList> commandList);
	void BuildGeometry(ComPtr<ID3D12GraphicsCommandList> commandList);
	void BuildMaterials();
	void BuildRenderItems();
	void BuildRecursivelyRenderItems(aiNode* node, XMMATRIX modelMatrix);
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
	std::filesystem::path m_SceneFolder;
	const aiScene* m_Scene;

	std::unordered_map<std::string, std::unique_ptr<Texture>> m_Textures;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_Geometries;
	std::vector<std::unique_ptr<Material>> m_Materials;
	std::vector<std::unique_ptr<RenderItem>> m_RenderItems;
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

	// for SSAO
	bool m_IsSSAO = false;
};