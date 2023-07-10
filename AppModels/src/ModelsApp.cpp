#include <ModelsApp.h>
#include <MyD3D12Lib/D3D12Utils.h>
#include <MyD3D12Lib/Helpers.h>

#include <d3dx12.h>
#include <DirectXPackedVector.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/mesh.h>

#include <array>

ModelsApp::ModelsApp(HINSTANCE hInstance) : BaseApp(hInstance) {
	Initialize();
}

ModelsApp::~ModelsApp() {};

bool ModelsApp::Initialize() {
	// initialization for DirectXTK12
	ThrowIfFailed(::CoInitializeEx(NULL, COINIT_MULTITHREADED));

	// load scene
	Assimp::Importer importer;

	m_SceneFolder = "../../3rd-party/Sponza/glTF/";
	std::filesystem::path scenePath = m_SceneFolder;
	scenePath += "Sponza.gltf";

	m_Scene = importer.ReadFile(
		scenePath.string(),
		aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder | aiProcess_FlipUVs
	);

	assert(m_Scene && "Scene not loaded");

	InitSceneState();

	ComPtr<ID3D12GraphicsCommandList> commandList = m_DirectCommandQueue->GetCommandList();

	BuildTextures(commandList);
	BuildLights();
	BuildGeometry(commandList);
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildRootSignature();
	BuildPipelineStateObject();

	// count size for CBV and SRV descriptor heap
	uint32_t numCBVandSRVforRenderItems = 0;

	numCBVandSRVforRenderItems += 1; // for pass constants
	numCBVandSRVforRenderItems += m_RenderItems.size(); // for object constants
	numCBVandSRVforRenderItems += m_Materials.size(); // for material constants
	numCBVandSRVforRenderItems *= m_NumBackBuffers; // repeat all prev constants for each back buffer
	numCBVandSRVforRenderItems += m_Textures.size(); // for textures

	m_CBV_SRVDescHeap = CreateDescriptorHeap(
		m_Device,
		numCBVandSRVforRenderItems + m_NumSobelSRV + m_NumSSAO_SRV,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);

	BuildSRViews();
	BuildCBViews();

	// recreate rtv descriptro heap for effects
	m_RTVDescHeap = CreateDescriptorHeap(
		m_Device,
		m_NumBackBuffers + m_NumSobelRTV + m_NumSSAO_RTV,
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);
	UpdateBackBuffersView();

	// for Sobel filter
	m_SobelTextureRTVIndex = m_NumBackBuffers;
	m_SobelTextureSRVIndex = m_NextCBV_SRVDescHeapIndex;
	UpdateSobelFrameTexture();
	BuildSobelRootSignature();
	BuildSobelPipelineStateObject();

	// for SSAO
	m_SSAO_RTV_StartIndex = m_SobelTextureRTVIndex + m_NumSobelRTV;
	m_SSAO_SRV_StartIndex = m_SobelTextureSRVIndex + m_NumSobelSRV;
	BuildSSAONormPipelineStateObject();
	BuildSSAORootSignature();
	BuildSSAOPipelineStateObject();
	BuildRandomMapBuffer(commandList);
	UpdateSSAOBuffersAndViews();
	InitBlurWeights();

	// wait while all data loaded
	uint32_t fenceValue = m_DirectCommandQueue->ExecuteCommandList(commandList);
	m_DirectCommandQueue->WaitForFenceValue(fenceValue);

	// release upload heaps after vertex, index and textures loading
	for (auto& it : m_Geometries) {
		it.second->DisposeUploaders();
	}

	for (auto& it : m_Textures) {
		it.second->UploadResource = nullptr;
	}

	m_RandomMapUploadBuffer = nullptr;

	return true;
}

void ModelsApp::OnUpdate() {
	m_CurrentFrameResources = m_FramesResources[m_CurrentBackBufferIndex].get();

	m_Timer.Tick();

	// log fps
	if (m_Timer.GetMeasuredTime() >= 1.0) {
		char buffer[500];
		auto fps = m_Timer.GetMeasuredTicks() / m_Timer.GetMeasuredTime();
		::sprintf_s(buffer, 500, "FPS: %f\n", fps);
		::OutputDebugString(buffer);

		XMFLOAT3 cameraPos;
		XMStoreFloat3(&cameraPos, m_Camera.GetCameraPos());
		::sprintf_s(buffer, 500, "camear position: %f %f %f\n", cameraPos.x, cameraPos.y, cameraPos.z);
		::OutputDebugString(buffer);

		m_Timer.StartMeasurement();
	}
	
	// update pass constants
	m_PassConstants.View = m_Camera.GetViewMatrix();

	m_PassConstants.Proj = GetProjectionMatrix(
		m_IsInverseDepth,
		m_Camera.GetFoV(),
		m_ClientWidth / static_cast<float>(m_ClientHeight),
		0.1f
	);

	if (m_IsShakeEffect) {
		m_Shaker.Shake(m_PassConstants.Proj, m_ClientWidth, m_ClientHeight);
	}

	m_PassConstants.ViewProj = XMMatrixMultiply(m_PassConstants.View, m_PassConstants.Proj);

	m_PassConstants.ProjInv = XMMatrixInverse(
		&XMMatrixDeterminant(m_PassConstants.Proj),
		m_PassConstants.Proj
	);

	m_PassConstants.ProjTex = XMMatrixMultiply(
		m_PassConstants.Proj, 
		XMMATRIX(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f
		)
	);

	m_PassConstants.ViewProjTex = XMMatrixMultiply(
		m_PassConstants.View,
		m_PassConstants.ProjTex
	);

	XMStoreFloat3(&m_PassConstants.EyePos, m_Camera.GetCameraPos());
	m_PassConstants.TotalTime = float(m_Timer.GetTotalTime());

	m_PassConstants.OcclusionMapWidthInv = 2.0f / static_cast<float>(m_ClientWidth);
	m_PassConstants.OcclusionMapHeightInv = 2.0f / static_cast<float>(m_ClientHeight);

	m_CurrentFrameResources->m_PassConstantsBuffer->CopyData(0, m_PassConstants);

	// update materials if necessary
	for (auto& it : m_Materials) {
		if (it->NumDirtyFrames > 0) {
			m_CurrentFrameResources->m_MaterialsConstantsBuffer->CopyData(
				it->CBIndex,
				{
					it->DiffuseAlbedo,
					it->FresnelR0,
					it->Roughness
				}
			);

			--it->NumDirtyFrames;
		}
	}

	// update object constants if necessary
	for (auto& it: m_RenderItems) {
		if (it->m_NumDirtyFramse > 0) {
			m_CurrentFrameResources->m_ObjectsConstantsBuffer->CopyData(
				it->m_CBIndex, 
				{ 
					it->m_ModelMatrix,
					it->m_ModelMatrixInvTrans
				}
			);

			--it->m_NumDirtyFramse;
		}
	}

}

void ModelsApp::OnRender() {
	ComPtr<ID3D12GraphicsCommandList> commandList = m_DirectCommandQueue->GetCommandList();

	ID3D12Resource* mainRTBuffer = m_BackBuffers[m_CurrentBackBufferIndex].Get();
	CD3DX12_CPU_DESCRIPTOR_HANDLE mainRTV(
		m_RTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
		m_CurrentBackBufferIndex, m_RTVDescSize
	);

	// clear depth stenicl buffer
	commandList->ClearDepthStencilView(
		m_DSVDescHeap->GetCPUDescriptorHandleForHeapStart(),
		D3D12_CLEAR_FLAG_DEPTH,
		m_DepthClearValue,
		m_SteniclClearValue,
		0, NULL
	);

	if (m_DrawingType == DrawingType::SSAO) {
		// render occlusion map 
		RenderSSAO(commandList);
	}

	// render geometry
	{
		// chose pso depending on z-buffer type and drawing type
		ComPtr<ID3D12PipelineState> pso;

		switch (m_DrawingType)
		{
			case DrawingType::Ordinar:
				pso = m_IsInverseDepth ? m_PSOs["inverseDepth"] : m_PSOs["straightDepth"];
				break;
			case DrawingType::Normals:
				pso = m_IsInverseDepth ? m_PSOs["normInverseDepth"] : m_PSOs["normStraightDepth"];
				break;
			case DrawingType::SSAO:
				pso = m_IsOnlySSAO ? m_PSOs["SSAOonly"] : m_PSOs["geoWithSSAO"];
				break;
		}

		// for Sobel filter render geometry in intermediate texture
		if (m_IsSobelFilter) {
			CD3DX12_CPU_DESCRIPTOR_HANDLE sobelTexRTV(
				m_RTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
				m_SobelTextureRTVIndex, m_RTVDescSize
			);

			RenderGeometry(
				commandList, pso, 
				m_SobelFrameTextureBuffer.Get(), sobelTexRTV, 
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				m_BackGroundColor
			);
		}
		else {
			RenderGeometry(
				commandList, pso,
				mainRTBuffer, mainRTV,
				D3D12_RESOURCE_STATE_PRESENT,
				m_BackGroundColor
			);
		}
	}

	if (m_IsSobelFilter) {
		// apply Sobel filter
		RenderSobelFilter(commandList, mainRTBuffer, mainRTV);
	}

	// Present
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			mainRTBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT
		);

		commandList->ResourceBarrier(1, &barrier);

		m_BackBuffersFenceValues[m_CurrentBackBufferIndex] = m_DirectCommandQueue->ExecuteCommandList(commandList);

		UINT syncInterval = m_Vsync ? 1 : 0;
		UINT flags = m_AllowTearing && !m_Vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		ThrowIfFailed(m_SwapChain->Present(syncInterval, flags));

		m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
		m_DirectCommandQueue->WaitForFenceValue(m_BackBuffersFenceValues[m_CurrentBackBufferIndex]);
	}
}

void ModelsApp::RenderGeometry(
	ComPtr<ID3D12GraphicsCommandList> commandList,
	ComPtr<ID3D12PipelineState> pso,
	ID3D12Resource* rtBuffer,
	D3D12_CPU_DESCRIPTOR_HANDLE rtv,
	D3D12_RESOURCE_STATES rtBufferPrevState,
	std::array<FLOAT, 4> rtClearValue)
{
	CD3DX12_RESOURCE_BARRIER rtBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		rtBuffer,
		rtBufferPrevState,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	commandList->ResourceBarrier(1, &rtBarrier);
	commandList->ClearRenderTargetView(rtv, rtClearValue.data(), 0, NULL);

	// set root signature
	commandList->SetGraphicsRootSignature(m_RootSignatures["Geometry"].Get());

	// set descripotr heaps
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_CBV_SRVDescHeap.Get() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	if (m_DrawingType == DrawingType::SSAO) {
		// set occlusion map
		commandList->SetGraphicsRootDescriptorTable(
			4,
			CD3DX12_GPU_DESCRIPTOR_HANDLE(
				m_CBV_SRVDescHeap->GetGPUDescriptorHandleForHeapStart(),
				m_SSAO_SRV_StartIndex + 3,
				m_CBV_SRV_UAVDescSize
			)
		);
	}

	// set pass constants
	commandList->SetGraphicsRootDescriptorTable(
		1,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			m_CBV_SRVDescHeap->GetGPUDescriptorHandleForHeapStart(),
			m_PassConstantsViewsStartIndex + m_CurrentBackBufferIndex,
			m_CBV_SRV_UAVDescSize
		)
	);

	commandList->SetPipelineState(pso.Get());

	// set Rasterizer Stage
	commandList->RSSetScissorRects(1, &m_ScissorRect);
	commandList->RSSetViewports(1, &m_ViewPort);

	// set Output Mergere Stage
	commandList->OMSetRenderTargets(1, &rtv, FALSE, &m_DSVDescHeap->GetCPUDescriptorHandleForHeapStart());

	// draw render items
	for (uint32_t i = 0; i < m_RenderItems.size(); ++i) {
		auto renderItem = m_RenderItems[i].get();
		auto mat = renderItem->m_Material;

		// set Input-Assembler state
		commandList->IASetVertexBuffers(0, 1, &renderItem->m_MeshGeo->VertexBufferView());
		commandList->IASetIndexBuffer(&renderItem->m_MeshGeo->IndexBufferView());
		commandList->IASetPrimitiveTopology(renderItem->m_PrivitiveType);

		// set object and material constants and texture
		CD3DX12_GPU_DESCRIPTOR_HANDLE objCBDescHandle(
			m_CBV_SRVDescHeap->GetGPUDescriptorHandleForHeapStart(),
			m_ObjectConstantsViewsStartIndex + m_CurrentBackBufferIndex * m_RenderItems.size() + renderItem->m_CBIndex,
			m_CBV_SRV_UAVDescSize
		);

		CD3DX12_GPU_DESCRIPTOR_HANDLE matCBDescHandle(
			m_CBV_SRVDescHeap->GetGPUDescriptorHandleForHeapStart(),
			m_MaterialConstantsViewsStartIndex + m_CurrentBackBufferIndex * m_Materials.size() + mat->CBIndex,
			m_CBV_SRV_UAVDescSize
		);

		CD3DX12_GPU_DESCRIPTOR_HANDLE textureDescHandle(
			m_CBV_SRVDescHeap->GetGPUDescriptorHandleForHeapStart(),
			m_TexturesViewsStartIndex + m_Textures[mat->TextureName]->SRVHeapIndex,
			m_CBV_SRV_UAVDescSize
		);

		commandList->SetGraphicsRootDescriptorTable(0, objCBDescHandle);
		commandList->SetGraphicsRootDescriptorTable(2, matCBDescHandle);
		commandList->SetGraphicsRootDescriptorTable(3, textureDescHandle);

		// draw
		commandList->DrawIndexedInstanced(
			renderItem->m_IndexCount,
			1,
			renderItem->m_StartIndexLocation,
			renderItem->m_BaseVertexLocation,
			0
		);
	}
}

void ModelsApp::RenderSobelFilter(
	ComPtr<ID3D12GraphicsCommandList> commandList,
	ID3D12Resource* rtBuffer,
	D3D12_CPU_DESCRIPTOR_HANDLE rtv) 
{
	CD3DX12_RESOURCE_BARRIER sobelTexBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_SobelFrameTextureBuffer.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);

	CD3DX12_RESOURCE_BARRIER rtBufferBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		rtBuffer,
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	D3D12_RESOURCE_BARRIER bariers[] = { sobelTexBarrier, rtBufferBarrier };
	commandList->ResourceBarrier(_countof(bariers), bariers);

	// unbind all resources from pipeline
	commandList->ClearState(NULL);

	// set root signature and descripotr heaps
	commandList->SetGraphicsRootSignature(m_RootSignatures["Sobel"].Get());
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_CBV_SRVDescHeap.Get() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	// set pso
	commandList->SetPipelineState(m_PSOs["Sobel"].Get());

	// set Rasterizer Stage
	commandList->RSSetScissorRects(1, &m_ScissorRect);
	commandList->RSSetViewports(1, &m_ViewPort);

	// set Output Mergere Stage
	commandList->OMSetRenderTargets(1, &rtv, FALSE, NULL);

	// set Input Asembler Stage
	commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// set root parameter
	commandList->SetGraphicsRootDescriptorTable(
		0,
		CD3DX12_GPU_DESCRIPTOR_HANDLE(
			m_CBV_SRVDescHeap->GetGPUDescriptorHandleForHeapStart(),
			m_SobelTextureSRVIndex,
			m_CBV_SRV_UAVDescSize
		)
	);

	// draw
	commandList->DrawInstanced(6, 1, 0, 0);
}

void ModelsApp::RenderSSAO(ComPtr<ID3D12GraphicsCommandList> commandList) {
	// get RTV's
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescHandle(
		m_RTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
		m_SSAO_RTV_StartIndex, m_RTVDescSize
	);

	D3D12_CPU_DESCRIPTOR_HANDLE normalMapRTV = rtvDescHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE occlusionMap0RTV = rtvDescHandle.Offset(m_RTVDescSize);
	D3D12_CPU_DESCRIPTOR_HANDLE occlusionMap1RTV = rtvDescHandle.Offset(m_RTVDescSize);

	// get SRV's
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvDescHandle(
		m_CBV_SRVDescHeap->GetGPUDescriptorHandleForHeapStart(),
		m_SSAO_SRV_StartIndex, m_CBV_SRV_UAVDescSize
	);

	D3D12_GPU_DESCRIPTOR_HANDLE normalMapSRV = srvDescHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE occlusionMap0SRV = srvDescHandle.Offset(3 * m_CBV_SRV_UAVDescSize);
	D3D12_GPU_DESCRIPTOR_HANDLE occlusionMap1SRV = srvDescHandle.Offset(m_CBV_SRV_UAVDescSize);

	// draw normal map
	{
		ComPtr<ID3D12PipelineState> pso = m_IsInverseDepth ? m_PSOs["ssaoNormInverseDepth"] : m_PSOs["ssaoNormStraightDepth"];

		RenderGeometry(
			commandList, pso, 
			m_NormalMapBuffer.Get(), normalMapRTV, 
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			m_NormalMapBufferClearValue
		);
	}

	// draw occlusion map
	{
		CD3DX12_RESOURCE_BARRIER normalMapBarrir = CD3DX12_RESOURCE_BARRIER::Transition(
			m_NormalMapBuffer.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);

		CD3DX12_RESOURCE_BARRIER dsMapBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_DSBuffer.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);

		CD3DX12_RESOURCE_BARRIER occlusionMap0Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_OcclusionMapBuffer0.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);

		CD3DX12_RESOURCE_BARRIER barriers[] = { normalMapBarrir, dsMapBarrier, occlusionMap0Barrier };
		commandList->ResourceBarrier(_countof(barriers), barriers);

		// unbind all resources from pipeline
		commandList->ClearState(NULL);

		// set root signature
		commandList->SetGraphicsRootSignature(m_RootSignatures["SSAO"].Get());

		// set desc heap
		ID3D12DescriptorHeap* descriptorHeaps[] = { m_CBV_SRVDescHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		// set pass constants
		commandList->SetGraphicsRootDescriptorTable(
			2,
			CD3DX12_GPU_DESCRIPTOR_HANDLE(
				m_CBV_SRVDescHeap->GetGPUDescriptorHandleForHeapStart(),
				m_PassConstantsViewsStartIndex + m_CurrentBackBufferIndex,
				m_CBV_SRV_UAVDescSize
			)
		);

		// set normal, depth and random map as SRV
		commandList->SetGraphicsRootDescriptorTable(
			0,
			normalMapSRV
		);

		// set pso
		commandList->SetPipelineState(m_PSOs["SSAO"].Get());

		// set Rasterizer Stage
		D3D12_VIEWPORT curViewPort = m_ViewPort;
		curViewPort.Width /= 2;
		curViewPort.Height /= 2;

		commandList->RSSetScissorRects(1, &m_ScissorRect);
		commandList->RSSetViewports(1, &curViewPort);

		// set Output Mergere Stage
		commandList->OMSetRenderTargets(1, &occlusionMap0RTV, FALSE, NULL);

		// set Input Asembler Stage
		commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// draw
		commandList->DrawInstanced(6, 1, 0, 0);
	}

	// blur occlusion map
	{
		commandList->SetGraphicsRoot32BitConstant(3, m_BlurRadius, 0);
		commandList->SetGraphicsRoot32BitConstants(3, 11, m_BlurWeights, 1);

		for (int i = 0; i < 2; ++i) {
			RenderBlur(commandList, m_OcclusionMapBuffer1, occlusionMap1RTV, m_OcclusionMapBuffer0, occlusionMap0SRV, false);
			RenderBlur(commandList, m_OcclusionMapBuffer0, occlusionMap0RTV, m_OcclusionMapBuffer1, occlusionMap1SRV, true);
		}
	}

	// return ds buffer in write state and occlusion map in sr state
	{
		CD3DX12_RESOURCE_BARRIER occlusionMap0Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_OcclusionMapBuffer0.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);

		CD3DX12_RESOURCE_BARRIER dsMapBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_DSBuffer.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		);

		CD3DX12_RESOURCE_BARRIER barriers[] = {
			occlusionMap0Barrier,
			dsMapBarrier,
		};

		commandList->ResourceBarrier(_countof(barriers), barriers);
	}
}

void ModelsApp::RenderBlur(
	ComPtr<ID3D12GraphicsCommandList> commandList,
	ComPtr<ID3D12Resource> rtBuffer,
	D3D12_CPU_DESCRIPTOR_HANDLE rtv,
	ComPtr<ID3D12Resource> srBuffer,
	D3D12_GPU_DESCRIPTOR_HANDLE srv,
	bool isHorizontal)
{
	CD3DX12_RESOURCE_BARRIER occlusionMap1Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		rtBuffer.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	CD3DX12_RESOURCE_BARRIER occlusionMap0Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		srBuffer.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);

	CD3DX12_RESOURCE_BARRIER barriers[] = {
		occlusionMap0Barrier,
		occlusionMap1Barrier
	};

	commandList->ResourceBarrier(_countof(barriers), barriers);
	commandList->SetGraphicsRoot32BitConstant(3, isHorizontal, 12);
	commandList->SetGraphicsRootDescriptorTable(1, srv);
	commandList->SetPipelineState(m_PSOs["Blur"].Get());
	commandList->OMSetRenderTargets(1, &rtv, FALSE, NULL);
	commandList->DrawInstanced(6, 1, 0, 0);
}

void ModelsApp::OnResize() {
	BaseApp::OnResize();
	UpdateSobelFrameTexture();
	UpdateSSAOBuffersAndViews();
}

void ModelsApp::OnKeyPressed(WPARAM wParam) {
	switch (wParam)
	{
	case '0':
		InitSceneState();
		break;
	case '1':
		m_IsShakeEffect = !m_IsShakeEffect;
		break;
	case '2':
		m_IsInverseDepth = !m_IsInverseDepth;

		if (m_IsInverseDepth) {
			m_DepthClearValue = 0.0f;
		} else {
			m_DepthClearValue = 1.0f;
		}

		m_DirectCommandQueue->Flush();
		ResizeDSBuffer();
		UpdateSSAOBuffersAndViews();
		break;
	case '3':
		m_IsSobelFilter = !m_IsSobelFilter;
		break;
	case '4':
		if (m_DrawingType == DrawingType::Normals) {
			m_DrawingType = DrawingType::Ordinar;
		} else {
			m_DrawingType = DrawingType::Normals;
		}
		break;
	case '5':
		if (m_DrawingType == DrawingType::SSAO) {
			m_DrawingType = DrawingType::Ordinar;
		}
		else {
			m_DrawingType = DrawingType::SSAO;
		}
		break;
	case '6':
		m_IsOnlySSAO = !m_IsOnlySSAO;
		break;
	case 'W':
	case 'S':
	case 'A':
	case 'D':
		m_Camera.MoveCamera(wParam);
		break;
	case 'R':
		m_DirectCommandQueue->Flush();
		BuildPipelineStateObject();
		BuildSobelPipelineStateObject();
		BuildSSAONormPipelineStateObject();
		BuildSSAOPipelineStateObject();
		break;
	}
}

void ModelsApp::OnMouseWheel(int wheelDelta) {
	m_Camera.ChangeFoV(wheelDelta);
}

void ModelsApp::OnMouseDown(WPARAM wParam, int x, int y) {
	m_LastMousePos.x = x;
	m_LastMousePos.y = y;

	::SetCapture(m_WindowHandle);
}

void ModelsApp::OnMouseUp(WPARAM wParam, int x, int y) {
	::ReleaseCapture();
}

void ModelsApp::OnMouseMove(WPARAM wParam, int x, int y) {
	if ((wParam & MK_LBUTTON) != 0) {
		m_Camera.RotateCamera(x - m_LastMousePos.x, y - m_LastMousePos.y);
	} else if ((wParam & MK_RBUTTON) != 0) {
		m_Camera.ChangeRadius(x - m_LastMousePos.x, y - m_LastMousePos.y);
	}

	m_LastMousePos.x = x;
	m_LastMousePos.y = y;
}

void ModelsApp::InitSceneState() {
	m_DrawingType = DrawingType::Ordinar;

	m_Camera = Camera(
		45.0f,
		0,
		XM_PIDIV2,
		2.0f,
		XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
	);

	m_Timer = Timer();
	m_Timer.StartMeasurement();
	
	m_Shaker = Shaker();
}

void ModelsApp::BuildLights() {
	m_PassConstants.AmbientLight = XMVectorSet(0.4f, 0.4f, 0.4f, 1.0f);
	uint32_t curLight = 0;
	// directional light 1 
	{
		m_PassConstants.Lights[curLight].Strength = { 0.0f, 0.0f, 0.0f };
		m_PassConstants.Lights[curLight].Direction = XMFLOAT3(0.0f, -1.0f, 0.0f);
		++curLight;
	}

	// point light 1
	{
		m_PassConstants.Lights[curLight].Strength = { 1.0f, 1.0f, 1.0f };
		m_PassConstants.Lights[curLight].Position = XMFLOAT3(0.0f, 10.0f, 0.0f);
		m_PassConstants.Lights[curLight].FalloffStart = 5.0f;
		m_PassConstants.Lights[curLight].FalloffEnd = 20.0f;
		++curLight;
	}

	// spot light 1
	{
		m_PassConstants.Lights[curLight].Strength = { 0.0f, 1.0f, 0.0f };
		m_PassConstants.Lights[curLight].Position = { 4.0f, 8.0f, 0.0f };
		m_PassConstants.Lights[curLight].FalloffStart = 4.0f;
		m_PassConstants.Lights[curLight].FalloffEnd = 20.0f;
		m_PassConstants.Lights[curLight].Direction = XMFLOAT3(0.0f, -1.0f, 0.0f);
		m_PassConstants.Lights[curLight].SpotPower = 12.0f;
		++curLight;
	}

	// spot light 2
	{
		m_PassConstants.Lights[curLight].Strength = { 1.0f, 0.0f, 0.0f };
		m_PassConstants.Lights[curLight].Position = { 0.0f, 8.0f, 0.0f };
		m_PassConstants.Lights[curLight].FalloffStart = 4.0f;
		m_PassConstants.Lights[curLight].FalloffEnd = 20.0f;
		m_PassConstants.Lights[curLight].Direction = XMFLOAT3(0.0f, -1.0f, 0.0f);
		m_PassConstants.Lights[curLight].SpotPower = 12.0f;
		++curLight;
	}

	// spot light 3
	{
		m_PassConstants.Lights[curLight].Strength = { 0.0f, 0.0f, 1.0f };
		m_PassConstants.Lights[curLight].Position = { -4.0f, 8.0f, 0.0f };
		m_PassConstants.Lights[curLight].FalloffStart = 4.0f;
		m_PassConstants.Lights[curLight].FalloffEnd = 20.0f;
		m_PassConstants.Lights[curLight].Direction = XMFLOAT3(0.0f, -1.0f, 0.0f);
		m_PassConstants.Lights[curLight].SpotPower = 12.0f;
		++curLight;
	}
}

void ModelsApp::BuildTextures(ComPtr<ID3D12GraphicsCommandList> commandList) {
	for (uint32_t i = 0; i < m_Scene->mNumMaterials; ++i) {
		aiString textureRelPath;
		m_Scene->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE, 0, &textureRelPath);

		if (textureRelPath.length == 0) {
			continue;
		}

		std::filesystem::path textureAbsPath = m_SceneFolder;
		textureAbsPath += textureRelPath.C_Str();

		auto tex = std::make_unique<Texture>();

		tex->Name = textureRelPath.C_Str();
		tex->FileName = textureAbsPath;

		CreateWICTextureFromFile(
			m_Device,
			commandList,
			tex->FileName,
			tex->Resource,
			tex->UploadResource
		);

		m_Textures[tex->Name] = std::move(tex);
	}
}

void ModelsApp::BuildGeometry(ComPtr<ID3D12GraphicsCommandList> commandList) {
	for (uint32_t i = 0; i < m_Scene->mNumMeshes; ++i) {
		auto geo = std::make_unique<MeshGeometry>();
		aiMesh* mesh = m_Scene->mMeshes[i];

		uint32_t numVertexes = mesh->mNumVertices;
		std::vector<Vertex> vertexes;
		vertexes.reserve(numVertexes);

		for (uint32_t j = 0; j < numVertexes; ++j) {
			aiVector3D vertexPos = mesh->mVertices[j];
			aiVector3D vertexTexC = mesh->mTextureCoords[0][j];
			aiVector3D vertexNorm = mesh->mNormals[j];

			vertexes.push_back({
				XMFLOAT3(vertexPos.x, vertexPos.y, vertexPos.z),
				XMFLOAT3(vertexNorm.x, vertexNorm.y, vertexNorm.z),
				XMFLOAT2(vertexTexC.x, vertexTexC.y)
				});
		}

		uint32_t numIndexes = mesh->mNumFaces * 3;
		std::vector<uint16_t> indexes;
		indexes.reserve(numIndexes);

		for (uint32_t j = 0; j < mesh->mNumFaces; ++j) {
			aiFace face = mesh->mFaces[j];

			assert(face.mNumIndices == 3 && "Faces not traingles!");

			for (uint32_t k = 0; k < face.mNumIndices; ++k) {
				indexes.push_back(face.mIndices[k]);
			}
		}

		uint32_t vbByteSize = sizeof(Vertex) * vertexes.size();
		uint32_t ibByteSize = sizeof(uint16_t) * indexes.size();

		geo->VertexBufferGPU = CreateGPUResourceAndLoadData(
			m_Device,
			commandList,
			geo->VertexBufferUploader,
			vertexes.data(),
			vbByteSize
		);

		geo->IndexBufferGPU = CreateGPUResourceAndLoadData(
			m_Device,
			commandList,
			geo->IndexBufferUploader,
			indexes.data(),
			ibByteSize
		);

		geo->name = mesh->mName.C_Str();
		geo->VertexBufferByteSize = vbByteSize;
		geo->VertexByteStride = sizeof(Vertex);
		geo->IndexBufferByteSize = ibByteSize;
		geo->IndexBufferFormat = DXGI_FORMAT_R16_UINT;

		SubmeshGeometry submesh;
		submesh.IndexCount = numIndexes;
		submesh.BaseVertexLocation = 0;
		submesh.StartIndexLocation = 0;

		geo->DrawArgs[mesh->mName.C_Str()] = submesh;

		m_Geometries[geo->name] = std::move(geo);
	}
}

void ModelsApp::BuildMaterials() {
	m_Materials.reserve(m_Scene->mNumMaterials);

	for (uint32_t i = 0; i < m_Scene->mNumMaterials; ++i) {
		aiMaterial* aimat = m_Scene->mMaterials[i];
		
		aiString texturePath;
		aimat->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);
		aiColor3D diffuseColor(0.0f, 0.0f, 0.0f);
		aimat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
		aiColor3D specularColor(0.0f, 0.0f, 0.0f);
		aimat->Get(AI_MATKEY_COLOR_SPECULAR, specularColor);
		float shininess = 0.0f;
		aimat->Get(AI_MATKEY_SHININESS, shininess);

		auto mat = std::make_unique<Material>();
		
		mat->CBIndex = i;
		mat->TextureName = texturePath.C_Str();
		mat->DiffuseAlbedo = XMFLOAT4(diffuseColor.r, diffuseColor.g, diffuseColor.b, 1.0f);
		mat->FresnelR0 = XMFLOAT3(specularColor.r, specularColor.g, specularColor.b);
		mat->Roughness = 0.99f - shininess;

		m_Materials.push_back(std::move(mat));
	}
}

void ModelsApp::BuildRenderItems() {
	BuildRecursivelyRenderItems(m_Scene->mRootNode, XMMatrixIdentity());
}

void ModelsApp::BuildRecursivelyRenderItems(aiNode* node, XMMATRIX modelMatrix) {
	aiMatrix4x4 m = node->mTransformation;

	modelMatrix = XMMatrixMultiply(
		XMLoadFloat4x4(&XMFLOAT4X4(
			m.a1, m.b1, m.c1, m.d1,
			m.a2, m.b2, m.c2, m.d2,
			m.a3, m.b3, m.c3, m.d3,
			m.a4, m.b4, m.c4, m.d4
		)),
		modelMatrix
	);

	XMMATRIX modelMatrixInvTrans = XMMatrixTranspose(XMMatrixInverse(
		&XMMatrixDeterminant(modelMatrix),
		modelMatrix
	));

	for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
		auto ri = std::make_unique<RenderItem>();

		aiMesh* curMesh = m_Scene->mMeshes[node->mMeshes[i]];
		std::string curMeshName = curMesh->mName.C_Str();

		auto curGeo = m_Geometries[curMeshName].get();

		ri->m_ModelMatrix = modelMatrix;
		ri->m_ModelMatrixInvTrans = modelMatrixInvTrans;
		ri->m_MeshGeo = curGeo;
		ri->m_Material = m_Materials[curMesh->mMaterialIndex].get();
		ri->m_IndexCount = curGeo->DrawArgs[curMeshName].IndexCount;
		ri->m_StartIndexLocation = curGeo->DrawArgs[curMeshName].StartIndexLocation;
		ri->m_BaseVertexLocation = curGeo->DrawArgs[curMeshName].BaseVertexLocation;
		ri->m_CBIndex = m_RenderItems.size();

		m_RenderItems.push_back(std::move(ri));
	}

	for (uint32_t i = 0; i < node->mNumChildren; ++i) {
		BuildRecursivelyRenderItems(node->mChildren[i], modelMatrix);
	}
}

void ModelsApp::BuildFrameResources() {
	m_FramesResources.reserve(m_NumBackBuffers);

	for (uint32_t i = 0; i < m_NumBackBuffers; ++i) {
		m_FramesResources.push_back(std::make_unique<FrameResources>(
			m_Device,
			1, 
			m_RenderItems.size(),
			m_Materials.size()
		));
	}
}

void ModelsApp::BuildSRViews() {
	m_TexturesViewsStartIndex = m_NextCBV_SRVDescHeapIndex;

	D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};

	viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	viewDesc.Texture2D.MostDetailedMip = 0;
	viewDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle(m_CBV_SRVDescHeap->GetCPUDescriptorHandleForHeapStart());

	size_t i = m_TexturesViewsStartIndex;
	for (auto& it : m_Textures) {
		it.second->SRVHeapIndex = i;
		ID3D12Resource* curTexBuffer = it.second->Resource.Get();

		viewDesc.Format = curTexBuffer->GetDesc().Format;
		viewDesc.Texture2D.MipLevels = curTexBuffer->GetDesc().MipLevels;

		m_Device->CreateShaderResourceView(curTexBuffer, &viewDesc, descHandle);

		descHandle.Offset(m_CBV_SRV_UAVDescSize);
		++i;
	}

	m_NextCBV_SRVDescHeapIndex += m_Textures.size();
}

void ModelsApp::BuildCBViews() {
	// for object constants
	m_ObjectConstantsViewsStartIndex = m_NextCBV_SRVDescHeapIndex;

	CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle(
		m_CBV_SRVDescHeap->GetCPUDescriptorHandleForHeapStart(),
		m_ObjectConstantsViewsStartIndex,
		m_CBV_SRV_UAVDescSize
	);

	uint32_t objectConstansElementByteSize = m_FramesResources[0]->m_ObjectsConstantsBuffer->GetElementByteSize();

	for (uint32_t i = 0; i < m_NumBackBuffers; ++i) {
		auto objectsConstantsBuffer = m_FramesResources[i]->m_ObjectsConstantsBuffer->Get();
		D3D12_GPU_VIRTUAL_ADDRESS objectConstantsBufferGPUAdress = objectsConstantsBuffer->GetGPUVirtualAddress();

		for (uint32_t j = 0; j < m_RenderItems.size(); ++j) {
			D3D12_CONSTANT_BUFFER_VIEW_DESC CBViewDesc;

			CBViewDesc.BufferLocation = objectConstantsBufferGPUAdress;
			CBViewDesc.SizeInBytes = objectConstansElementByteSize;

			m_Device->CreateConstantBufferView(
				&CBViewDesc,
				descHandle
			);

			descHandle.Offset(m_CBV_SRV_UAVDescSize);
			objectConstantsBufferGPUAdress += objectConstansElementByteSize;
		}
	}

	m_NextCBV_SRVDescHeapIndex += m_NumBackBuffers * m_RenderItems.size();

	// for pass constants
	m_PassConstantsViewsStartIndex =m_NextCBV_SRVDescHeapIndex;
	uint32_t passConstansElementByteSize = m_FramesResources[0]->m_PassConstantsBuffer->GetElementByteSize();

	for (uint32_t i = 0; i < m_NumBackBuffers; ++i) {
		auto passConstantsBuffer = m_FramesResources[i]->m_PassConstantsBuffer->Get();

		D3D12_CONSTANT_BUFFER_VIEW_DESC CBViewDesc;

		CBViewDesc.BufferLocation = passConstantsBuffer->GetGPUVirtualAddress();
		CBViewDesc.SizeInBytes = passConstansElementByteSize;

		m_Device->CreateConstantBufferView(
			&CBViewDesc,
			descHandle
		);

		descHandle.Offset(m_CBV_SRV_UAVDescSize);
	}

	m_NextCBV_SRVDescHeapIndex += m_NumBackBuffers;

	// for material constants
	m_MaterialConstantsViewsStartIndex = m_NextCBV_SRVDescHeapIndex;
	uint32_t materialConstantsElementByteSize = m_FramesResources[0]->m_MaterialsConstantsBuffer->GetElementByteSize();

	for (uint32_t i = 0; i < m_NumBackBuffers; ++i) {
		auto materialConstantsBuffer = m_FramesResources[i]->m_MaterialsConstantsBuffer->Get();
		D3D12_GPU_VIRTUAL_ADDRESS materialConstantsBufferGPUAdress = materialConstantsBuffer->GetGPUVirtualAddress();

		for (uint32_t j = 0; j < m_Materials.size(); ++j) {
			D3D12_CONSTANT_BUFFER_VIEW_DESC CBViewDesc;

			CBViewDesc.BufferLocation = materialConstantsBufferGPUAdress;
			CBViewDesc.SizeInBytes = materialConstantsElementByteSize;

			m_Device->CreateConstantBufferView(
				&CBViewDesc,
				descHandle
			);

			descHandle.Offset(m_CBV_SRV_UAVDescSize);
			materialConstantsBufferGPUAdress += materialConstantsElementByteSize;
		}
	}

	m_NextCBV_SRVDescHeapIndex += m_NumBackBuffers * m_Materials.size();
}

void ModelsApp::BuildRootSignature() {
	// init parameters
	CD3DX12_ROOT_PARAMETER1 rootParameters[5];

	CD3DX12_DESCRIPTOR_RANGE1 objConstsDescRange;
	objConstsDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE1 passConstsDescRange;
	passConstsDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

	CD3DX12_DESCRIPTOR_RANGE1 matConstsDescRange;
	matConstsDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);

	CD3DX12_DESCRIPTOR_RANGE1 texDescRange;
	texDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE1 occlusionMapDescRange;
	occlusionMapDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

	rootParameters[0].InitAsDescriptorTable(1, &objConstsDescRange);
	rootParameters[1].InitAsDescriptorTable(1, &passConstsDescRange);
	rootParameters[2].InitAsDescriptorTable(1, &matConstsDescRange);
	rootParameters[3].InitAsDescriptorTable(1, &texDescRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[4].InitAsDescriptorTable(1, &occlusionMapDescRange, D3D12_SHADER_VISIBILITY_PIXEL);

	// set access flags
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	// create sampler
	CD3DX12_STATIC_SAMPLER_DESC samplerDesc(
		0,
		D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP
	);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &samplerDesc, rootSignatureFlags);

	ComPtr<ID3DBlob> rootSignatureBlob;
	ComPtr<ID3DBlob> errorBlob;

	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
		&rootSignatureDesc,
		GetRootSignatureVersion(m_Device),
		&rootSignatureBlob,
		&errorBlob
	));

	ComPtr<ID3D12RootSignature> rootSignature;

	ThrowIfFailed(m_Device->CreateRootSignature(
		0,
		rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature)
	));

	m_RootSignatures["Geometry"] = rootSignature;
}

void ModelsApp::BuildPipelineStateObject() {
	// Compile shaders	
	ComPtr<ID3DBlob> geoVertexShaderBlob = CompileShader(L"../../AppModels/shaders/GeoVertexShader.hlsl", "main", "vs_5_1");
	
	D3D_SHADER_MACRO geoDefines[] = {
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	ComPtr<ID3DBlob> geoPixelShaderBlob = CompileShader(L"../../AppModels/shaders/GeoPixelShader.hlsl", "main", "ps_5_1", geoDefines);
    
	D3D_SHADER_MACRO normDefines[] = {
		"ALPHA_TEST", "1",
		"DRAW_NORMS", "1",
		NULL, NULL
	};

	ComPtr<ID3DBlob> normPixelShaderBlob = CompileShader(L"../../AppModels/shaders/GeoPixelShader.hlsl", "main", "ps_5_1", normDefines);

	D3D_SHADER_MACRO ssaoDefines[] = {
		"ALPHA_TEST", "1",
		"SSAO", "1",
		NULL, NULL
	};

	ComPtr<ID3DBlob> ssaoPixelShaderBlob = CompileShader(L"../../AppModels/shaders/GeoPixelShader.hlsl", "main", "ps_5_1", ssaoDefines);

	D3D_SHADER_MACRO ssaoOnlyDefines[] = {
		"ALPHA_TEST", "1",
		"SSAO", "1",
		"SSAO_ONLY", "1",
		NULL, NULL
	};

	ComPtr<ID3DBlob> ssaoOnlyPixelShaderBlob = CompileShader(L"../../AppModels/shaders/GeoPixelShader.hlsl", "main", "ps_5_1", ssaoOnlyDefines);

	// Create rasterizer state description
	CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
	//rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;

	// Create input layout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[]{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORM", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	D3D12_INPUT_LAYOUT_DESC inpuitLayout{ inputElementDescs, _countof(inputElementDescs) };

	// Create pipeline state object description
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;

	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	psoDesc.pRootSignature = m_RootSignatures["Geometry"].Get();

	psoDesc.VS = {
		reinterpret_cast<BYTE*>(geoVertexShaderBlob->GetBufferPointer()),
		geoVertexShaderBlob->GetBufferSize()
	};

	psoDesc.PS = {
		reinterpret_cast<BYTE*>(geoPixelShaderBlob->GetBufferPointer()),
		geoPixelShaderBlob->GetBufferSize()
	};

	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.InputLayout = inpuitLayout;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = m_DepthSencilViewFormat;
	psoDesc.SampleDesc = { 1, 0 };

	ComPtr<ID3D12PipelineState> straightDepthPSO;
	ComPtr<ID3D12PipelineState> inverseDepthPSO;
	ComPtr<ID3D12PipelineState> normStraightDepthPSO;
	ComPtr<ID3D12PipelineState> normInverseDepthPSO;
	ComPtr<ID3D12PipelineState> ssaoPSO;
	ComPtr<ID3D12PipelineState> ssaoOnlyPSO;

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&straightDepthPSO)));

	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&inverseDepthPSO)));

	psoDesc.PS = {
		reinterpret_cast<BYTE*>(normPixelShaderBlob->GetBufferPointer()),
		normPixelShaderBlob->GetBufferSize()
	};

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&normInverseDepthPSO)));
	
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&normStraightDepthPSO)));

	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;

	psoDesc.PS = {
		reinterpret_cast<BYTE*>(ssaoPixelShaderBlob->GetBufferPointer()),
		ssaoPixelShaderBlob->GetBufferSize()
	};

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&ssaoPSO)));

	psoDesc.PS = {
		reinterpret_cast<BYTE*>(ssaoOnlyPixelShaderBlob->GetBufferPointer()),
		ssaoOnlyPixelShaderBlob->GetBufferSize()
	};

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&ssaoOnlyPSO)));

	m_PSOs["straightDepth"] = straightDepthPSO;
	m_PSOs["inverseDepth"] = inverseDepthPSO;	
	m_PSOs["normStraightDepth"] = normStraightDepthPSO;
	m_PSOs["normInverseDepth"] = normInverseDepthPSO;	
	m_PSOs["geoWithSSAO"] = ssaoPSO;
	m_PSOs["SSAOonly"] = ssaoOnlyPSO;
}

void ModelsApp::UpdateSobelFrameTexture() {
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_ClientWidth, m_ClientHeight);
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	CD3DX12_CLEAR_VALUE clearValue{ DXGI_FORMAT_R8G8B8A8_UNORM , m_BackGroundColor.data() };

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
		&resourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(&m_SobelFrameTextureBuffer)
	));

	m_Device->CreateRenderTargetView(
		m_SobelFrameTextureBuffer.Get(),
		nullptr, 
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			m_RTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
			m_SobelTextureRTVIndex,
			m_CBV_SRV_UAVDescSize
		)
	);

	m_Device->CreateShaderResourceView(
		m_SobelFrameTextureBuffer.Get(),
		nullptr, 
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			m_CBV_SRVDescHeap->GetCPUDescriptorHandleForHeapStart(),
			m_SobelTextureSRVIndex,
			m_CBV_SRV_UAVDescSize
		)
	);
}

void ModelsApp::BuildSobelRootSignature() {
	CD3DX12_ROOT_PARAMETER1 rootParameters[1];

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRange;
	descriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	rootParameters[0].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(1, rootParameters, 0, NULL, rootSignatureFlags);

	ComPtr<ID3DBlob> rootSignatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
		&rootSignatureDesc,
		GetRootSignatureVersion(m_Device),
		&rootSignatureBlob,
		&errorBlob
	));

	ComPtr<ID3D12RootSignature> sobelRootSignature;

	ThrowIfFailed(m_Device->CreateRootSignature(
		0,
		rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&sobelRootSignature)
	));

	m_RootSignatures["Sobel"] = sobelRootSignature;
}

void ModelsApp::BuildSobelPipelineStateObject() {
	// Compile shaders	
	ComPtr<ID3DBlob> sobelVertexShaderBlob = CompileShader(L"../../AppModels/shaders/FullScreenQuadVS.hlsl", "main", "vs_5_1");
	ComPtr<ID3DBlob> sobelPixelShaderBlob = CompileShader(L"../../AppModels/shaders/SobelPixelShader.hlsl", "main", "ps_5_1");

	// Create pipeline state object description
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;

	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	psoDesc.pRootSignature = m_RootSignatures["Sobel"].Get();

	psoDesc.VS = {
		reinterpret_cast<BYTE*>(sobelVertexShaderBlob->GetBufferPointer()),
		sobelVertexShaderBlob->GetBufferSize()
	};

	psoDesc.PS = {
		reinterpret_cast<BYTE*>(sobelPixelShaderBlob->GetBufferPointer()),
		sobelPixelShaderBlob->GetBufferSize()
	};

	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	CD3DX12_DEPTH_STENCIL_DESC depthStencilState(D3D12_DEFAULT);
	depthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState = depthStencilState;

	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	psoDesc.SampleDesc = { 1, 0 };

	ComPtr<ID3D12PipelineState> sobelPSO;

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&sobelPSO)));

	m_PSOs["Sobel"] = sobelPSO;
}

void ModelsApp::UpdateSSAOBuffersAndViews() {
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_RTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
		m_SSAO_RTV_StartIndex,
		m_RTVDescSize
	);

	CD3DX12_CPU_DESCRIPTOR_HANDLE srvDescHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_CBV_SRVDescHeap->GetCPUDescriptorHandleForHeapStart(),
		m_SSAO_SRV_StartIndex,
		m_CBV_SRV_UAVDescSize
	);

	// create normal map buffer
	CD3DX12_RESOURCE_DESC normalMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		m_ClientWidth,
		m_ClientHeight
	);

	normalMapDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	CD3DX12_CLEAR_VALUE normalMapClearValue{ DXGI_FORMAT_R16G16B16A16_FLOAT , m_NormalMapBufferClearValue.data() };

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
		&normalMapDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&normalMapClearValue,
		IID_PPV_ARGS(&m_NormalMapBuffer)
	));

	// crate RTV for normal map buffer
	m_Device->CreateRenderTargetView(
		m_NormalMapBuffer.Get(),
		nullptr,
		rtvDescHandle
	);

	// create SRV for normal map buffer
	m_Device->CreateShaderResourceView(
		m_NormalMapBuffer.Get(),
		nullptr,
		srvDescHandle
	);

	// create SRV for depth buffer
	D3D12_SHADER_RESOURCE_VIEW_DESC dsViewDesc = {};

	dsViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	dsViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	dsViewDesc.Texture2D.MostDetailedMip = 0;
	dsViewDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	dsViewDesc.Format = DXGI_FORMAT_R32_FLOAT;
	dsViewDesc.Texture2D.MipLevels = 1;

	m_Device->CreateShaderResourceView(
		m_DSBuffer.Get(),
		&dsViewDesc,
		srvDescHandle.Offset(m_CBV_SRV_UAVDescSize)
	);

	// create SRV for random map buffer
	m_Device->CreateShaderResourceView(
		m_RandomMapBuffer.Get(),
		nullptr,
		srvDescHandle.Offset(m_CBV_SRV_UAVDescSize)
	);

	// create occlusion maps buffers
	CD3DX12_RESOURCE_DESC occlusionMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R16_UNORM,
		m_ClientWidth / 2,
		m_ClientHeight / 2
	);

	occlusionMapDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
		&occlusionMapDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		NULL,
		IID_PPV_ARGS(&m_OcclusionMapBuffer0)
	));

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
		&occlusionMapDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		NULL,
		IID_PPV_ARGS(&m_OcclusionMapBuffer1)
	));

	// create RTV's for occlusion maps buffers
	m_Device->CreateRenderTargetView(
		m_OcclusionMapBuffer0.Get(),
		nullptr,
		rtvDescHandle.Offset(m_RTVDescSize)
	);

	m_Device->CreateRenderTargetView(
		m_OcclusionMapBuffer1.Get(),
		nullptr,
		rtvDescHandle.Offset(m_RTVDescSize)
	);

	// create SRV's for occlusion maps buffers
	m_Device->CreateShaderResourceView(
		m_OcclusionMapBuffer0.Get(),
		nullptr,
		srvDescHandle.Offset(m_CBV_SRV_UAVDescSize)
	);

	m_Device->CreateShaderResourceView(
		m_OcclusionMapBuffer1.Get(),
		nullptr,
		srvDescHandle.Offset(m_CBV_SRV_UAVDescSize)
	);
}

void ModelsApp::BuildSSAONormPipelineStateObject() {
	// Compile shaders	
	ComPtr<ID3DBlob> geoVertexShaderBlob = CompileShader(L"../../AppModels/shaders/GeoVertexShader.hlsl", "main", "vs_5_1");

	D3D_SHADER_MACRO defines[] = {
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	ComPtr<ID3DBlob> geoPixelShaderBlob = CompileShader(L"../../AppModels/shaders/SSAONormVPixelShader.hlsl", "main", "ps_5_1", defines);

	// Create rasterizer state description
	CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
	//rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;

	// Create input layout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[]{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORM", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	D3D12_INPUT_LAYOUT_DESC inpuitLayout{ inputElementDescs, _countof(inputElementDescs) };

	// Create pipeline state object description
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;

	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	psoDesc.pRootSignature = m_RootSignatures["Geometry"].Get();

	psoDesc.VS = {
		reinterpret_cast<BYTE*>(geoVertexShaderBlob->GetBufferPointer()),
		geoVertexShaderBlob->GetBufferSize()
	};

	psoDesc.PS = {
		reinterpret_cast<BYTE*>(geoPixelShaderBlob->GetBufferPointer()),
		geoPixelShaderBlob->GetBufferSize()
	};

	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.InputLayout = inpuitLayout;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.DSVFormat = m_DepthSencilViewFormat;
	psoDesc.SampleDesc = { 1, 0 };

	ComPtr<ID3D12PipelineState> straightDepthPSO;
	ComPtr<ID3D12PipelineState> inverseDepthPSO;

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&straightDepthPSO)));

	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&inverseDepthPSO)));

	m_PSOs["ssaoNormStraightDepth"] = straightDepthPSO;
	m_PSOs["ssaoNormInverseDepth"] = inverseDepthPSO;
}

void ModelsApp::BuildSSAORootSignature() {
	CD3DX12_ROOT_PARAMETER1 rootParameters[4];

	CD3DX12_DESCRIPTOR_RANGE1 normalDepthRandomMapsDescRange;
	normalDepthRandomMapsDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);
	
	CD3DX12_DESCRIPTOR_RANGE1 occlusionMapDesRange;
	occlusionMapDesRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);

	CD3DX12_DESCRIPTOR_RANGE1 passConstantsDescRange;
	passConstantsDescRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	rootParameters[0].InitAsDescriptorTable(1, &normalDepthRandomMapsDescRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[1].InitAsDescriptorTable(1, &occlusionMapDesRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[2].InitAsDescriptorTable(1, &passConstantsDescRange);
	rootParameters[3].InitAsConstants(13, 1);

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	// create sampler
	CD3DX12_STATIC_SAMPLER_DESC depthSamplerDesc(
		0,
		D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0.0f, 1,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE
	);

	CD3DX12_STATIC_SAMPLER_DESC linWrapSamplerDesc(
		1,
		D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP
	);

	CD3DX12_STATIC_SAMPLER_DESC pointClampSamplerDesc(
		2,
		D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	);

	D3D12_STATIC_SAMPLER_DESC samplersDescs[] = { 
		depthSamplerDesc, 
		linWrapSamplerDesc,
		pointClampSamplerDesc
	};

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, _countof(samplersDescs), samplersDescs, rootSignatureFlags);

	ComPtr<ID3DBlob> rootSignatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
		&rootSignatureDesc,
		GetRootSignatureVersion(m_Device),
		&rootSignatureBlob,
		&errorBlob
	));

	ComPtr<ID3D12RootSignature> ssaoRootSignature;

	ThrowIfFailed(m_Device->CreateRootSignature(
		0,
		rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&ssaoRootSignature)
	));

	m_RootSignatures["SSAO"] = ssaoRootSignature;
}

void ModelsApp::BuildSSAOPipelineStateObject() {
	// Compile shaders	
	ComPtr<ID3DBlob> ssaoVertexShaderBlob = CompileShader(L"../../AppModels/shaders/SSAOVertexShader.hlsl", "main", "vs_5_1");
	ComPtr<ID3DBlob> ssaoPixelShaderBlob = CompileShader(L"../../AppModels/shaders/SSAOPixelShader.hlsl", "main", "ps_5_1");

	// Create pipeline state object description
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;

	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	psoDesc.pRootSignature = m_RootSignatures["SSAO"].Get();

	psoDesc.VS = {
		reinterpret_cast<BYTE*>(ssaoVertexShaderBlob->GetBufferPointer()),
		ssaoVertexShaderBlob->GetBufferSize()
	};

	psoDesc.PS = {
		reinterpret_cast<BYTE*>(ssaoPixelShaderBlob->GetBufferPointer()),
		ssaoPixelShaderBlob->GetBufferSize()
	};

	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	CD3DX12_DEPTH_STENCIL_DESC depthStencilState(D3D12_DEFAULT);
	depthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState = depthStencilState;

	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	psoDesc.SampleDesc = { 1, 0 };

	ComPtr<ID3D12PipelineState> ssaoPSO;

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&ssaoPSO)));

	m_PSOs["SSAO"] = ssaoPSO;

	// Compile shaders for blur
	ComPtr<ID3DBlob> blurVertexShaderBlob = CompileShader(L"../../AppModels/shaders/FullScreenQuadVS.hlsl", "main", "vs_5_1");
	ComPtr<ID3DBlob> blurPixelShaderBlob = CompileShader(L"../../AppModels/shaders/BlurPixelShader.hlsl", "main", "ps_5_1");

	psoDesc.VS = {
	reinterpret_cast<BYTE*>(blurVertexShaderBlob->GetBufferPointer()),
	blurVertexShaderBlob->GetBufferSize()
	};

	psoDesc.PS = {
		reinterpret_cast<BYTE*>(blurPixelShaderBlob->GetBufferPointer()),
		blurPixelShaderBlob->GetBufferSize()
	};

	ComPtr<ID3D12PipelineState> blurPSO;

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&blurPSO)));

	m_PSOs["Blur"] = blurPSO;
}

void ModelsApp::BuildRandomMapBuffer(ComPtr<ID3D12GraphicsCommandList> commandList) {
	// evenly distributed vectors 
	m_PassConstants.RandomDirections[0] = XMVectorSet(-1.0f, -1.0f, -1.0f, 0.0f);
	m_PassConstants.RandomDirections[1] = XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);

	m_PassConstants.RandomDirections[2] = XMVectorSet(1.0f, -1.0f, -1.0f, 0.0f);
	m_PassConstants.RandomDirections[3] = XMVectorSet(-1.0f, 1.0f, 1.0f, 0.0f);

	m_PassConstants.RandomDirections[4] = XMVectorSet(-1.0f, 1.0f, -1.0f, 0.0f);
	m_PassConstants.RandomDirections[5] = XMVectorSet(1.0f, -1.0f, 1.0f, 0.0f);

	m_PassConstants.RandomDirections[6] = XMVectorSet(1.0f, -1.0f, -1.0f, 0.0f);
	m_PassConstants.RandomDirections[7] = XMVectorSet(-1.0f, 1.0f, 1.0f, 0.0f);

	m_PassConstants.RandomDirections[8] = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	m_PassConstants.RandomDirections[9] = XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f);

	m_PassConstants.RandomDirections[10] = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	m_PassConstants.RandomDirections[11] = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);

	m_PassConstants.RandomDirections[12] = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	m_PassConstants.RandomDirections[13] = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);

	for (uint32_t i = 0; i < 14; ++i) {
		m_PassConstants.RandomDirections[i] = randFloat(0.25f, 1.0f) * XMVector3Normalize(m_PassConstants.RandomDirections[i]);
	}

	const uint32_t texWidth = 256;
	const uint32_t texHeight = 256;

	PackedVector::XMCOLOR* data = new PackedVector::XMCOLOR[texWidth * texHeight];

	for (uint32_t i = 0; i < texHeight; ++i) {
		for (uint32_t j = 0; j < texWidth; ++j) {
			data[255*i + j] = PackedVector::XMCOLOR(randFloat(), randFloat(), randFloat(), 0.0f);
		}
	}

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, texWidth, texHeight, 1, 1),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_RandomMapBuffer)
	));

	uint64_t uploadBufferSize = GetRequiredIntermediateSize(m_RandomMapBuffer.Get(), 0, 1);

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_RandomMapUploadBuffer)
	));

	D3D12_SUBRESOURCE_DATA subResouceData = {};
	subResouceData.pData = data;
	subResouceData.RowPitch = texWidth * sizeof(PackedVector::XMCOLOR);
	subResouceData.SlicePitch = subResouceData.RowPitch * texHeight;

	UpdateSubresources(
		commandList.Get(),
		m_RandomMapBuffer.Get(),
		m_RandomMapUploadBuffer.Get(),
		0, 0, 1,
		&subResouceData
	);

	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_RandomMapBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_GENERIC_READ
	);

	commandList->ResourceBarrier(1, &barrier);

	delete[] data;
}

void ModelsApp::InitBlurWeights() {
	// weights for Gaussian blur
	float sigma = 3.0f;
	float weightsSum = 0.0f;

	for (int i = -m_BlurRadius; i < m_BlurRadius; ++i) {
		m_BlurWeights[m_BlurRadius + i] = std::exp(-std::pow(static_cast<float>(i) / sigma, 2.0f) / 2.0f);
		weightsSum += m_BlurWeights[m_BlurRadius + i];
	}

	for (int i = -m_BlurRadius; i < m_BlurRadius; ++i) {
		m_BlurWeights[m_BlurRadius + i] /= weightsSum;
	}
}
