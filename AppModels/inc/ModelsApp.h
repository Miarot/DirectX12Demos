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
#include <array>
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

	void RenderSobelFilter(
		ComPtr<ID3D12GraphicsCommandList> commandList,
		ID3D12Resource* rtBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE rtv
	);

	void RenderGeometry(
		ComPtr<ID3D12GraphicsCommandList> commandList,
		ComPtr<ID3D12PipelineState> pso,
		ID3D12Resource* rtBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE rtv,
		D3D12_RESOURCE_STATES rtBufferPrevState,
		std::array<FLOAT, 4> rtClearValue
	);

	void RenderSSAO(ComPtr<ID3D12GraphicsCommandList> commandList);

	void RenderBlur(
		ComPtr<ID3D12GraphicsCommandList> commandList,
		ComPtr<ID3D12Resource> rtBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE rtv,
		ComPtr<ID3D12Resource> srBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE srv,
		bool isHorizontal
	);

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
	void UpdateSobelFrameTexture();
	void BuildSobelRootSignature();
	void BuildSobelPipelineStateObject();

	// for SSAO
	void UpdateSSAOBuffersAndViews();
	void BuildSSAONormPipelineStateObject();
	void BuildSSAORootSignature();
	void BuildSSAOPipelineStateObject();
	void BuildRandomMapBuffer(ComPtr<ID3D12GraphicsCommandList> commandList);
	void InitBlurWeights();

private:
	std::array<FLOAT, 4> m_BackGroundColor = { 0.4f, 0.6f, 0.9f, 1.0f };

	enum DrawingType { Ordinar = 0, Normals, SSAO };

	DrawingType m_DrawingType = DrawingType::Ordinar;
	bool m_IsInverseDepth = false;
	bool m_IsShakeEffect = false;
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
	uint32_t m_NextCBV_SRVDescHeapIndex = 0;

	std::unordered_map<std::string, ComPtr<ID3D12RootSignature>> m_RootSignatures;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;

	ComPtr<ID3D12DescriptorHeap> m_FrameTexturesRTVDescHeap;

	// for Sobel filter
	bool m_IsSobelFilter = false;
	static const uint32_t m_NumSobelRTV = 1;
	static const uint32_t m_NumSobelSRV = 1;
	ComPtr<ID3D12Resource> m_SobelFrameTextureBuffer;
	uint32_t m_SobelTextureRTVIndex;
	uint32_t m_SobelTextureSRVIndex;

	// for SSAO
	bool m_IsOnlySSAO = false;
	static const uint32_t m_NumSSAO_RTV = 3;
	static const uint32_t m_NumSSAO_SRV = 5;
	ComPtr<ID3D12Resource> m_NormalMapBuffer;
	ComPtr<ID3D12Resource> m_OcclusionMapBuffer0;
	ComPtr<ID3D12Resource> m_OcclusionMapBuffer1;
	ComPtr<ID3D12Resource> m_RandomMapBuffer;
	ComPtr<ID3D12Resource> m_RandomMapUploadBuffer;
	uint32_t m_SSAO_RTV_StartIndex;
	uint32_t m_SSAO_SRV_StartIndex;
	std::array<FLOAT, 4> m_NormalMapBufferClearValue = { 0.0f, 0.0f, 0.0f, 0.0f };

	static const int m_BlurRadius = 5;
	float m_BlurWeights[2 * m_BlurRadius + 1];
};