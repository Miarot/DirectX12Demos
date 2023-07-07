#include <SimpleGeoApp.h>
#include <MyD3D12Lib/D3D12Utils.h>

#include <d3dx12.h>

#include <array>

SimpleGeoApp::SimpleGeoApp(HINSTANCE hInstance) : BaseApp(hInstance) {
	Initialize();
}

SimpleGeoApp::~SimpleGeoApp() {};

bool SimpleGeoApp::Initialize() {
	ComPtr<ID3D12GraphicsCommandList> commandList = m_DirectCommandQueue->GetCommandList();

	InitSceneState();

	BuildTextures(commandList);
	BuildLights();
	BuildGeometry(commandList);
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();

	m_CBV_SRVDescHeap = CreateDescriptorHeap(
		m_Device,
		m_Textures.size() + (m_RenderItems.size() + 1 + m_Materials.size()) * m_NumBackBuffers,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);

	BuildSRViews();
	BuildCBViews();

	BuildRootSignature();
	BuildPipelineStateObject();

	// for Sobel filter
	m_FrameTextureRTVDescHeap = CreateDescriptorHeap(
		m_Device,
		1,
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);

	m_FrameTextureSRVDescHeap = CreateDescriptorHeap(
		m_Device,
		1,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);

	UpdateFramesTextures();
	BuildSobelRootSignature();
	BuildSobelPipelineStateObject();

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

	return true;
}

void SimpleGeoApp::OnUpdate() {
	m_CurrentFrameResources = m_FramesResources[m_CurrentBackBufferIndex].get();

	m_Timer.Tick();

	// log fps
	if (m_Timer.GetMeasuredTime() >= 1.0) {
		char buffer[500];
		auto fps = m_Timer.GetMeasuredTicks() / m_Timer.GetMeasuredTime();
		::sprintf_s(buffer, 500, "FPS: %f\n", fps);
		::OutputDebugString(buffer);

		m_Timer.StartMeasurement();
	}
	
	// update pass constants
	m_PassConstants.View = m_Camera.GetViewMatrix();

	m_PassConstants.Proj = GetProjectionMatrix(
		m_IsInverseDepth,
		m_Camera.GetFoV(),
		m_ClientWidth / static_cast<float>(m_ClientHeight)
	);

	if (m_IsShakeEffect) {
		m_Shaker.Shake(m_PassConstants.Proj, m_ClientWidth, m_ClientHeight);
	}

	m_PassConstants.ViewProj = XMMatrixMultiply(m_PassConstants.View, m_PassConstants.Proj);
	XMStoreFloat3(&m_PassConstants.EyePos, m_Camera.GetCameraPos());
	m_PassConstants.TotalTime = float(m_Timer.GetTotalTime());

	m_CurrentFrameResources->m_PassConstantsBuffer->CopyData(0, m_PassConstants);

	// update materials if necessary
	for (auto& it : m_Materials) {
		auto mat = it.second.get();

		if (mat->NumDirtyFrames > 0) {
			m_CurrentFrameResources->m_MaterialsConstantsBuffer->CopyData(
				mat->MaterialCBIndex,
				{
					mat->DiffuseAlbedo,
					mat->FresnelR0,
					mat->Roughness
				}
			);

			--mat->NumDirtyFrames;
		}
	}

	// rotate box
	auto boxRenderItem = m_RenderItems[2].get();
	float angle = static_cast<float>(m_Timer.GetTotalTime() * 90.0);
	const XMVECTOR rotationAxis = XMVectorSet(0, 1, 1, 0);
	boxRenderItem->m_ModelMatrix = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(angle));
	boxRenderItem->m_NumDirtyFramse = 3;

	// update object constants if necessary
	for (auto& it: m_RenderItems) {
		if (it->m_NumDirtyFramse > 0) {
			m_CurrentFrameResources->m_ObjectsConstantsBuffer->CopyData(
				it->m_CBIndex, 
				{ it->m_ModelMatrix }
			);

			--it->m_NumDirtyFramse;
		}
	}

}

void SimpleGeoApp::OnRender() {
	ComPtr<ID3D12GraphicsCommandList> commandList = m_DirectCommandQueue->GetCommandList();

	auto backBuffer = m_BackBuffers[m_CurrentBackBufferIndex];
	auto frameTextureBuffer = m_FrameTexturesBuffers;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mainRTV(
		m_BackBuffersDescHeap->GetCPUDescriptorHandleForHeapStart(),
		m_CurrentBackBufferIndex, m_RTVDescSize
	);

	CD3DX12_CPU_DESCRIPTOR_HANDLE textureRTV(m_FrameTextureRTVDescHeap->GetCPUDescriptorHandleForHeapStart());

	CD3DX12_CPU_DESCRIPTOR_HANDLE geometryRTV;

	if (m_IsSobelFilter) {
		geometryRTV = textureRTV;
	} else {
		geometryRTV = mainRTV;
	}

	// clear RTs and DS buffer
	{
		CD3DX12_RESOURCE_BARRIER backBufferBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.Get(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);

		if (m_IsSobelFilter) {
			CD3DX12_RESOURCE_BARRIER frameTextureBufferBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
				frameTextureBuffer.Get(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			);

			D3D12_RESOURCE_BARRIER bariers[] = { backBufferBarrier, frameTextureBufferBarrier };
			commandList->ResourceBarrier(_countof(bariers), bariers);

			commandList->ClearRenderTargetView(textureRTV, m_BackGroundColor, 0, NULL);
		} else {
			D3D12_RESOURCE_BARRIER bariers[] = { backBufferBarrier };
			commandList->ResourceBarrier(_countof(bariers), bariers);
		}

		commandList->ClearRenderTargetView(mainRTV, m_BackGroundColor, 0, NULL);

		commandList->ClearDepthStencilView(
			m_DSVDescHeap->GetCPUDescriptorHandleForHeapStart(),
			D3D12_CLEAR_FLAG_DEPTH,
			m_DepthClearValue, 
			m_SteniclClearValue,
			0, NULL
		);
	}

	// draw geometry
	{
		// set root signature
		commandList->SetGraphicsRootSignature(m_RootSignatures["Geometry"].Get());

		// set descripotr heaps
		ID3D12DescriptorHeap* descriptorHeaps[] = { m_CBV_SRVDescHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		// set pass constants
		commandList->SetGraphicsRootDescriptorTable(
			1, 
			CD3DX12_GPU_DESCRIPTOR_HANDLE(
				m_CBV_SRVDescHeap->GetGPUDescriptorHandleForHeapStart(),
				m_PassConstantsViewsStartIndex + m_CurrentBackBufferIndex,
				m_CBV_SRV_UAVDescSize
			)
		);

		// set PSO
		// chose pso depending on z-buffer type and drawing type
		ComPtr<ID3D12PipelineState> pso;

		if (m_IsInverseDepth) {
			if (m_IsDrawNorm) {
				pso = m_PSOs["normInverseDepth"];
			} else {
				pso = m_PSOs["inverseDepth"];
			}
		} else {
			if (m_IsDrawNorm) {
				pso = m_PSOs["normStraightDepth"];
			}
			else {
				pso = m_PSOs["straightDepth"];
			}
		}

		commandList->SetPipelineState(pso.Get());

		// set Rasterizer Stage
		commandList->RSSetScissorRects(1, &m_ScissorRect);
		commandList->RSSetViewports(1, &m_ViewPort);

		// set Output Mergere Stage
		commandList->OMSetRenderTargets(1, &geometryRTV, FALSE, &m_DSVDescHeap->GetCPUDescriptorHandleForHeapStart());
		
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
				m_MaterialConstantsViewsStartIndex + m_CurrentBackBufferIndex * m_Materials.size() + mat->MaterialCBIndex,
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

	// apply filter and draw to main render target
	if (m_IsSobelFilter) {
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			frameTextureBuffer.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);

		commandList->ResourceBarrier(1, &barrier);

		// unbind all resources from pipeline
		commandList->ClearState(NULL);

		// set root signature and descripotr heaps
		commandList->SetGraphicsRootSignature(m_RootSignatures["Sobel"].Get());
		ID3D12DescriptorHeap* descriptorHeaps[] = { m_FrameTextureSRVDescHeap.Get() };
		commandList->SetDescriptorHeaps(1, descriptorHeaps);

		// set pso
		commandList->SetPipelineState(m_PSOs["Sobel"].Get());

		// set Rasterizer Stage
		commandList->RSSetScissorRects(1, &m_ScissorRect);
		commandList->RSSetViewports(1, &m_ViewPort);

		// set Output Mergere Stage
		commandList->OMSetRenderTargets(1, &mainRTV, FALSE, NULL);

		// set Input Asembler Stage
		commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// set root parameter
		commandList->SetGraphicsRootDescriptorTable(0, m_FrameTextureSRVDescHeap->GetGPUDescriptorHandleForHeapStart());

		// draw
		commandList->DrawInstanced(3, 1, 0, 0);
	}

	// Present
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.Get(),
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

void SimpleGeoApp::OnResize() {
	BaseApp::OnResize();
	UpdateFramesTextures();
}

void SimpleGeoApp::OnKeyPressed(WPARAM wParam) {
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
		break;
	case '3':
		m_IsSobelFilter = !m_IsSobelFilter;
		break;
	case '4':
		m_IsDrawNorm = !m_IsDrawNorm;
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
		break;
	}
}

void SimpleGeoApp::OnMouseWheel(int wheelDelta) {
	m_Camera.ChangeFoV(wheelDelta);
}

void SimpleGeoApp::OnMouseDown(WPARAM wParam, int x, int y) {
	m_LastMousePos.x = x;
	m_LastMousePos.y = y;

	::SetCapture(m_WindowHandle);
}

void SimpleGeoApp::OnMouseUp(WPARAM wParam, int x, int y) {
	::ReleaseCapture();
}

void SimpleGeoApp::OnMouseMove(WPARAM wParam, int x, int y) {
	if ((wParam & MK_LBUTTON) != 0) {
		m_Camera.RotateCamera(x - m_LastMousePos.x, y - m_LastMousePos.y);
	} else if ((wParam & MK_RBUTTON) != 0) {
		m_Camera.ChangeRadius(x - m_LastMousePos.x, y - m_LastMousePos.y);
	}

	m_LastMousePos.x = x;
	m_LastMousePos.y = y;
}

void SimpleGeoApp::InitSceneState() {
	m_Camera = Camera(
		45.0f,
		XM_PIDIV2 + 0.4,
		XM_PIDIV4 + 0.4,
		15.0f,
		XMVectorSet(4.0f, 0.0f, 4.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
	);

	m_Timer = Timer();
	m_Timer.StartMeasurement();
	
	m_Shaker = Shaker();
}

void SimpleGeoApp::BuildLights() {
	m_PassConstants.AmbientLight = XMVectorSet(0.1f, 0.1f, 0.1f, 1.0f);

	// directional light 1 
	{
		m_PassConstants.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
		XMVECTOR direction = XMVector3Normalize(-XMVectorSet(1.0f, 2.0f, 3.0f, 0.0f));
		XMStoreFloat3(&m_PassConstants.Lights[0].Direction, direction);
	}


	// point light 1
	{
		m_PassConstants.Lights[1].Strength = { 0.6f, 0.0f, 0.0f };
		m_PassConstants.Lights[1].Position = { 7.0f, 2.0f, 5.0f };
		m_PassConstants.Lights[1].FalloffStart = 1.0f;
		m_PassConstants.Lights[1].FalloffEnd = 8.0f;
	}

	// spot light 1
	{
		m_PassConstants.Lights[2].Strength = { 0.0f, 0.8f, 0.0f };
		m_PassConstants.Lights[2].Position = { 5.0f, 0.0f, 7.0f };
		m_PassConstants.Lights[2].FalloffStart = 1.0f;
		m_PassConstants.Lights[2].FalloffEnd = 8.0f;
		m_PassConstants.Lights[2].Direction = XMFLOAT3(1.0f, 0.0f, 0.0f);
		m_PassConstants.Lights[2].SpotPower = 12.0f;
	}
}

void SimpleGeoApp::BuildTextures(ComPtr<ID3D12GraphicsCommandList> commandList) {
	// load default texture
	{
		auto tex = std::make_unique<Texture>();

		tex->Name = "default";
		tex->FileName = L"../../AppSimpleGeometry/textures/default.dds";

		CreateDDSTextureFromFile(
			m_Device,
			commandList,
			tex->FileName,
			tex->Resource,
			tex->UploadResource
		);

		m_Textures[tex->Name] = std::move(tex);
	}

	// load crate texture
	{
		auto crateTex = std::make_unique<Texture>();

		crateTex->Name = "crate";
		crateTex->FileName = L"../../AppSimpleGeometry/textures/WoodCrate01.dds";

		CreateDDSTextureFromFile(
			m_Device,
			commandList,
			crateTex->FileName,
			crateTex->Resource,
			crateTex->UploadResource
		);

		m_Textures[crateTex->Name] = std::move(crateTex);
	}

	// load briks texture
	{
		auto brickTex = std::make_unique<Texture>();

		brickTex->Name = "bricks";
		brickTex->FileName = L"../../AppSimpleGeometry/textures/bricks.dds";

		CreateDDSTextureFromFile(
			m_Device,
			commandList,
			brickTex->FileName,
			brickTex->Resource,
			brickTex->UploadResource
		);

		m_Textures[brickTex->Name] = std::move(brickTex);
	}
}

void SimpleGeoApp::BuildGeometry(ComPtr<ID3D12GraphicsCommandList> commandList) {
	auto boxAndPiramidGeo = std::make_unique<MeshGeometry>();

	const uint32_t numBoxVertexes = 24;
	const uint32_t numPiramidVertexes = 16;

	std::array<Vertex, numBoxVertexes + numPiramidVertexes> vertexes = {
		// box
		// front face
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(), XMFLOAT2(0.0f, 1.0f) }), // 0, 0
		Vertex({ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(), XMFLOAT2(0.0f, 0.0f) }), // 1, 1
		Vertex({ XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(), XMFLOAT2(1.0f, 0.0f) }), // 2, 2
		Vertex({ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(), XMFLOAT2(1.0f, 1.0f) }), // 3, 3

		// left face
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(), XMFLOAT2(1.0f, 1.0f) }), // 0, 4
		Vertex({ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(), XMFLOAT2(0.0f, 1.0f) }), // 4, 5
		Vertex({ XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(), XMFLOAT2(0.0f, 0.0f) }), // 5, 6
		Vertex({ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(), XMFLOAT2(1.0f, 0.0f) }), // 1, 7

		// right face
		Vertex({ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(), XMFLOAT2(0.0f, 1.0f) }), // 3, 8
		Vertex({ XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(), XMFLOAT2(0.0f, 0.0f) }), // 2, 9
		Vertex({ XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(), XMFLOAT2(1.0f, 0.0f) }), // 6, 10
		Vertex({ XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(), XMFLOAT2(1.0f, 1.0f) }), // 7, 11

		// top face
		Vertex({ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(), XMFLOAT2(0.0f, 1.0f) }), // 1, 12
		Vertex({ XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(), XMFLOAT2(0.0f, 0.0f) }), // 5, 13
		Vertex({ XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(), XMFLOAT2(1.0f, 0.0f) }), // 6, 14
		Vertex({ XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(), XMFLOAT2(1.0f, 1.0f) }), // 2, 15

		// bottom face
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(), XMFLOAT2(1.0f, 1.0f) }), // 0, 16
		Vertex({ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(), XMFLOAT2(1.0f, 0.0f) }), // 4, 17
		Vertex({ XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(), XMFLOAT2(0.0f, 0.0f) }), // 7, 18
		Vertex({ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(), XMFLOAT2(0.0f, 1.0f) }), // 3, 19

		// back face
		Vertex({ XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(), XMFLOAT2(0.0f, 1.0f) }), // 7, 20
		Vertex({ XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(), XMFLOAT2(0.0f, 0.0f) }), // 6, 21
		Vertex({ XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(), XMFLOAT2(1.0f, 0.0f) }), // 5, 22
		Vertex({ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(), XMFLOAT2(1.0f, 1.0f) }), // 4, 23

		// piramid
		// bottom face
		Vertex({ XMFLOAT3(0.0f, 0.0f, 0.0f) }), // 0, 0
		Vertex({ XMFLOAT3(1.0f, 0.0f, 0.0f) }), // 1, 1
		Vertex({ XMFLOAT3(1.0f, 0.0f, 1.0f) }), // 2, 2
		Vertex({ XMFLOAT3(0.0f, 0.0f, 1.0f) }), // 3, 3

		// front face
		Vertex({ XMFLOAT3(0.5f, 1.0f, 0.5f) }), // 4, 4
		Vertex({ XMFLOAT3(1.0f, 0.0f, 0.0f) }), // 1, 5
		Vertex({ XMFLOAT3(0.0f, 0.0f, 0.0f) }), // 0, 6

		// back face
		Vertex({ XMFLOAT3(0.5f, 1.0f, 0.5f) }), // 4, 7
		Vertex({ XMFLOAT3(0.0f, 0.0f, 1.0f) }), // 3, 8
		Vertex({ XMFLOAT3(1.0f, 0.0f, 1.0f) }), // 2, 9

		// left face
		Vertex({ XMFLOAT3(0.5f, 1.0f, 0.5f) }), // 4, 10
		Vertex({ XMFLOAT3(0.0f, 0.0f, 0.0f) }), // 0, 11
		Vertex({ XMFLOAT3(0.0f, 0.0f, 1.0f) }), // 3, 12

		// right face
		Vertex({ XMFLOAT3(0.5f, 1.0f, 0.5f) }), // 4, 13
		Vertex({ XMFLOAT3(1.0f, 0.0f, 1.0f) }), // 2, 14
		Vertex({ XMFLOAT3(1.0f, 0.0f, 0.0f) }), // 1, 15
	};

	const uint32_t numBoxIndexes = 36;
	const uint32_t numPiramidIndexes = 18;

	std::array<uint16_t, numBoxIndexes + numPiramidIndexes> indexes =
	{
		// box
		0, 1, 2, 0, 2, 3, // front face
		4, 5, 6, 4, 6, 7, // left face
		8, 9, 10, 8, 10, 11, // right face
		12, 13, 14, 12, 14, 15, // top face
		19, 18, 17, 19, 17, 16, // bottom face
		20, 21, 22, 20, 22, 23, // back face
		// piramid
		0, 1, 3, 1, 2, 3, // bottom face
		4, 5, 6, // front face
		7, 8, 9, // back face
		10, 11, 12, // left face
		13, 14, 15 // right face
	};

	// computer norms for each vertex
	std::array<XMVECTOR, vertexes.size()> vertexesNorm;

	for (uint32_t i = 0; i < vertexes.size(); ++i) {
		vertexesNorm[i] = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	}
	
	// for box
	for (uint32_t i = 0; i < numBoxIndexes; i += 3) {
		uint16_t i0 = indexes[i];
		uint16_t i1 = indexes[i + 1];
		uint16_t i2 = indexes[i + 2];

		XMVECTOR p0 = XMLoadFloat3(&vertexes[i0].Position);
		XMVECTOR p1 = XMLoadFloat3(&vertexes[i1].Position);
		XMVECTOR p2 = XMLoadFloat3(&vertexes[i2].Position);

		XMVECTOR triangleNorm = XMVector3Cross(p1 - p0, p2 - p0);

		vertexesNorm[i0] += triangleNorm;
		vertexesNorm[i1] += triangleNorm;
		vertexesNorm[i2] += triangleNorm;
	}

	// for piramid
	for (uint32_t i = numBoxIndexes; i < indexes.size(); i += 3) {
		uint16_t i0 = indexes[i] + numBoxVertexes;
		uint16_t i1 = indexes[i + 1] + numBoxVertexes;
		uint16_t i2 = indexes[i + 2] + numBoxVertexes;

		XMVECTOR p0 = XMLoadFloat3(&vertexes[i0].Position);
		XMVECTOR p1 = XMLoadFloat3(&vertexes[i1].Position);
		XMVECTOR p2 = XMLoadFloat3(&vertexes[i2].Position);

		XMVECTOR triangleNorm = XMVector3Cross(p1 - p0, p2 - p0);

		vertexesNorm[i0] += triangleNorm;
		vertexesNorm[i1] += triangleNorm;
		vertexesNorm[i2] += triangleNorm;
	}

	for (uint32_t i = 0; i < vertexes.size(); ++i) {
		XMStoreFloat3(&vertexes[i].Norm, XMVector3Normalize(vertexesNorm[i]));
	}

	UINT vbByteSize = vertexes.size() * sizeof(Vertex);
	UINT ibByteSize = indexes.size() * sizeof(uint16_t);

	boxAndPiramidGeo->VertexBufferGPU = CreateGPUResourceAndLoadData(
		m_Device,
		commandList,
		boxAndPiramidGeo->VertexBufferUploader,
		vertexes.data(),
		vbByteSize
	);

	boxAndPiramidGeo->IndexBufferGPU = CreateGPUResourceAndLoadData(
		m_Device,
		commandList,
		boxAndPiramidGeo->IndexBufferUploader,
		indexes.data(),
		ibByteSize
	);

	boxAndPiramidGeo->name = "BoxAndPiramid";
	boxAndPiramidGeo->VertexBufferByteSize = vbByteSize;
	boxAndPiramidGeo->VertexByteStride = sizeof(Vertex);
	boxAndPiramidGeo->IndexBufferByteSize = ibByteSize;
	boxAndPiramidGeo->IndexBufferFormat = DXGI_FORMAT_R16_UINT;

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = numBoxIndexes;
	boxSubmesh.BaseVertexLocation = 0;
	boxSubmesh.StartIndexLocation = 0;

	SubmeshGeometry piramidSubmesh;
	piramidSubmesh.IndexCount = numPiramidIndexes;
	piramidSubmesh.BaseVertexLocation = numBoxVertexes;
	piramidSubmesh.StartIndexLocation = numBoxIndexes;

	boxAndPiramidGeo->DrawArgs["Box"] = boxSubmesh;
	boxAndPiramidGeo->DrawArgs["Piramid"] = piramidSubmesh;

	m_Geometries[boxAndPiramidGeo->name] = std::move(boxAndPiramidGeo);
}

void SimpleGeoApp::BuildMaterials() {
	// grass material
	{
		auto grass = std::make_unique<Material>();

		grass->Name = "grass";
		grass->MaterialCBIndex = m_Materials.size();
		grass->DiffuseAlbedo = XMFLOAT4(0.2f, 0.6f, 0.3f, 1.0f);
		grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
		grass->Roughness = 0.125f;

		m_Materials[grass->Name] = std::move(grass);
	}

	// water material
	{
		auto water = std::make_unique<Material>();

		water->Name = "water";
		water->MaterialCBIndex = m_Materials.size();
		water->DiffuseAlbedo = XMFLOAT4(0.0f, 0.2f, 0.6f, 1.0f);
		water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
		water->Roughness = 0.0f;

		m_Materials[water->Name] = std::move(water);
	}

	// bricks material
	{
		auto bricks = std::make_unique<Material>();

		bricks->Name = "bricks";
		bricks->MaterialCBIndex = m_Materials.size();
		bricks->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		bricks->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
		bricks->Roughness = 0.0f;
		bricks->TextureName = "bricks";

		m_Materials[bricks->Name] = std::move(bricks);
	}

	// crate material
	{
		auto crate = std::make_unique<Material>();

		crate->Name = "crate";
		crate->MaterialCBIndex = m_Materials.size();
		crate->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		crate->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
		crate->Roughness = 0.5f;
		crate->TextureName = "crate";

		m_Materials[crate->Name] = std::move(crate);
	}

	// light material 1
	{
		auto light = std::make_unique<Material>();

		light->Name = "light1";
		light->MaterialCBIndex = m_Materials.size();

		light->DiffuseAlbedo = XMFLOAT4(
			1000.0f * m_PassConstants.Lights[1].Strength.x, 
			1000.0f * m_PassConstants.Lights[1].Strength.y,
			1000.0f * m_PassConstants.Lights[1].Strength.z,
			1.0f
		);

		light->FresnelR0 = XMFLOAT3(1.0f, 1.0f, 1.0f);
		light->Roughness = 0.0f;

		m_Materials[light->Name] = std::move(light);
	}

	// light material 2
	{
		auto light = std::make_unique<Material>();

		light->Name = "light2";
		light->MaterialCBIndex = m_Materials.size();

		light->DiffuseAlbedo = XMFLOAT4(
			1000.0f * m_PassConstants.Lights[2].Strength.x,
			1000.0f * m_PassConstants.Lights[2].Strength.y,
			1000.0f * m_PassConstants.Lights[2].Strength.z,
			1.0f
		);

		light->FresnelR0 = XMFLOAT3(1.0f, 1.0f, 1.0f);
		light->Roughness = 0.0f;

		m_Materials[light->Name] = std::move(light);
	}
}

void SimpleGeoApp::BuildRenderItems() {
	auto curGeo = m_Geometries["BoxAndPiramid"].get();

	// grass piramid
	{
		auto piramid = std::make_unique<RenderItem>();

		piramid->m_ModelMatrix = XMMatrixTranslation(2, 0, 2);
		piramid->m_MeshGeo = curGeo;
		piramid->m_Material = m_Materials["grass"].get();
		piramid->m_IndexCount = curGeo->DrawArgs["Piramid"].IndexCount;
		piramid->m_StartIndexLocation = curGeo->DrawArgs["Piramid"].StartIndexLocation;
		piramid->m_BaseVertexLocation = curGeo->DrawArgs["Piramid"].BaseVertexLocation;
		piramid->m_CBIndex = m_RenderItems.size();

		m_RenderItems.push_back(std::move(piramid));
	}

	// whater piramid
	{
		auto piramid = std::make_unique<RenderItem>();

		piramid->m_ModelMatrix = XMMatrixTranslation(4, 0, 4);
		piramid->m_Material = m_Materials["water"].get();
		piramid->m_MeshGeo = curGeo;
		piramid->m_IndexCount = curGeo->DrawArgs["Piramid"].IndexCount;
		piramid->m_StartIndexLocation = curGeo->DrawArgs["Piramid"].StartIndexLocation;
		piramid->m_BaseVertexLocation = curGeo->DrawArgs["Piramid"].BaseVertexLocation;
		piramid->m_CBIndex = m_RenderItems.size();

		m_RenderItems.push_back(std::move(piramid));
	}

	// rotating crate box
	{
		auto box = std::make_unique<RenderItem>();

		box->m_ModelMatrix = XMMatrixTranslation(0, 0, 0);
		box->m_MeshGeo = curGeo;
		box->m_Material = m_Materials["crate"].get();
		box->m_IndexCount = curGeo->DrawArgs["Box"].IndexCount;
		box->m_StartIndexLocation = curGeo->DrawArgs["Box"].StartIndexLocation;
		box->m_BaseVertexLocation = curGeo->DrawArgs["Box"].BaseVertexLocation;
		box->m_CBIndex = m_RenderItems.size();

		m_RenderItems.push_back(std::move(box));
	}

	// still bricks box
	{
		auto box = std::make_unique<RenderItem>();

		box->m_ModelMatrix = XMMatrixTranslation(7, 0, 7);
		box->m_MeshGeo = curGeo;
		box->m_Material = m_Materials["bricks"].get();
		box->m_IndexCount = curGeo->DrawArgs["Box"].IndexCount;
		box->m_StartIndexLocation = curGeo->DrawArgs["Box"].StartIndexLocation;
		box->m_BaseVertexLocation = curGeo->DrawArgs["Box"].BaseVertexLocation;
		box->m_CBIndex = m_RenderItems.size();

		m_RenderItems.push_back(std::move(box));
	}

	// point light piramid 1
	{
		auto piramid = std::make_unique<RenderItem>();

		piramid->m_ModelMatrix = XMMatrixTranslation(
			m_PassConstants.Lights[1].Position.x,
			m_PassConstants.Lights[1].Position.y,
			m_PassConstants.Lights[1].Position.z
		);

		piramid->m_ModelMatrix = XMMatrixMultiply(XMMatrixScaling(0.2f, 0.2f, 0.2f), piramid->m_ModelMatrix);
		piramid->m_Material = m_Materials["light1"].get();
		piramid->m_MeshGeo = curGeo;
		piramid->m_IndexCount = curGeo->DrawArgs["Piramid"].IndexCount;
		piramid->m_StartIndexLocation = curGeo->DrawArgs["Piramid"].StartIndexLocation;
		piramid->m_BaseVertexLocation = curGeo->DrawArgs["Piramid"].BaseVertexLocation;
		piramid->m_CBIndex = m_RenderItems.size();

		m_RenderItems.push_back(std::move(piramid));
	}

	// spot light piramid 1
	{
		auto piramid = std::make_unique<RenderItem>();

		piramid->m_ModelMatrix = XMMatrixTranslation(
			m_PassConstants.Lights[2].Position.x,
			m_PassConstants.Lights[2].Position.y,
			m_PassConstants.Lights[2].Position.z
		);

		piramid->m_ModelMatrix = XMMatrixMultiply(XMMatrixScaling(0.2f, 0.2f, 0.2f), piramid->m_ModelMatrix);
		piramid->m_Material = m_Materials["light2"].get();
		piramid->m_MeshGeo = curGeo;
		piramid->m_IndexCount = curGeo->DrawArgs["Piramid"].IndexCount;
		piramid->m_StartIndexLocation = curGeo->DrawArgs["Piramid"].StartIndexLocation;
		piramid->m_BaseVertexLocation = curGeo->DrawArgs["Piramid"].BaseVertexLocation;
		piramid->m_CBIndex = m_RenderItems.size();

		m_RenderItems.push_back(std::move(piramid));
	}

}

void SimpleGeoApp::BuildFrameResources() {
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

void SimpleGeoApp::BuildSRViews() {
	m_TexturesViewsStartIndex = 0;

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
}

void SimpleGeoApp::BuildCBViews() {
	m_ObjectConstantsViewsStartIndex = m_TexturesViewsStartIndex +  m_Textures.size();

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

	m_PassConstantsViewsStartIndex = m_ObjectConstantsViewsStartIndex + m_NumBackBuffers * m_RenderItems.size();
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


	m_MaterialConstantsViewsStartIndex = m_PassConstantsViewsStartIndex + m_NumBackBuffers;
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
}

void SimpleGeoApp::BuildRootSignature() {
	// init parameters
	CD3DX12_ROOT_PARAMETER1 rootParameters[4];

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRange0;
	descriptorRange0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRange1;
	descriptorRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRange2;
	descriptorRange2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRange3;
	descriptorRange3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	rootParameters[0].InitAsDescriptorTable(1, &descriptorRange0);
	rootParameters[1].InitAsDescriptorTable(1, &descriptorRange1);
	rootParameters[2].InitAsDescriptorTable(1, &descriptorRange2);
	rootParameters[3].InitAsDescriptorTable(1, &descriptorRange3, D3D12_SHADER_VISIBILITY_PIXEL);

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

void SimpleGeoApp::BuildPipelineStateObject() {
	// Compile shaders	
	ComPtr<ID3DBlob> geoVertexShaderBlob = CompileShader(L"../../AppSimpleGeometry/shaders/GeoVertexShader.hlsl", "main", "vs_5_1");
	ComPtr<ID3DBlob> geoPixelShaderBlob = CompileShader(L"../../AppSimpleGeometry/shaders/GeoPixelShader.hlsl", "main", "ps_5_1");
	ComPtr<ID3DBlob> normPixelShaderBlob = CompileShader(L"../../AppSimpleGeometry/shaders/NormPixelShader.hlsl", "main", "ps_5_1");

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
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = m_DepthSencilViewFormat;
	psoDesc.SampleDesc = { 1, 0 };

	ComPtr<ID3D12PipelineState> straightDepthPSO;
	ComPtr<ID3D12PipelineState> inverseDepthPSO;
	ComPtr<ID3D12PipelineState> normStraightDepthPSO;
	ComPtr<ID3D12PipelineState> normInverseDepthPSO;

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&straightDepthPSO)));

	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&inverseDepthPSO)));

	psoDesc.PS = {
		reinterpret_cast<BYTE*>(normPixelShaderBlob->GetBufferPointer()),
		normPixelShaderBlob->GetBufferSize()
	};

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&normInverseDepthPSO)));
	
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&normStraightDepthPSO)));

	m_PSOs["straightDepth"] = straightDepthPSO;
	m_PSOs["inverseDepth"] = inverseDepthPSO;	
	m_PSOs["normStraightDepth"] = normStraightDepthPSO;
	m_PSOs["normInverseDepth"] = normInverseDepthPSO;
}

void SimpleGeoApp::UpdateFramesTextures() {
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_ClientWidth, m_ClientHeight);
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	CD3DX12_CLEAR_VALUE clearValue{ DXGI_FORMAT_R8G8B8A8_UNORM , m_BackGroundColor };

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
		&resourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(&m_FrameTexturesBuffers)
	));

	m_Device->CreateRenderTargetView(
		m_FrameTexturesBuffers.Get(), 
		nullptr, 
		m_FrameTextureRTVDescHeap->GetCPUDescriptorHandleForHeapStart()
	);

	m_Device->CreateShaderResourceView(
		m_FrameTexturesBuffers.Get(),
		nullptr, 
		m_FrameTextureSRVDescHeap->GetCPUDescriptorHandleForHeapStart()
	);
}

void SimpleGeoApp::BuildSobelRootSignature() {
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

void SimpleGeoApp::BuildSobelPipelineStateObject() {
	// Compile shaders	
	ComPtr<ID3DBlob> sobelVertexShaderBlob = CompileShader(L"../../AppSimpleGeometry/shaders/SobelVertexShader.hlsl", "main", "vs_5_1");
	ComPtr<ID3DBlob> sobelPixelShaderBlob = CompileShader(L"../../AppSimpleGeometry/shaders/SobelPixelShader.hlsl", "main", "ps_5_1");

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