#include <d3dx12.h>

#include <chrono>
#include <array>

#include <SimpleGeoApp.h>
#include <FrameResources.h>

SimpleGeoApp::SimpleGeoApp(HINSTANCE hInstance) : BaseApp(hInstance) {}

SimpleGeoApp::~SimpleGeoApp() {};

bool SimpleGeoApp::Initialize() {
	if (!BaseApp::Initialize()) {
		return false;
	}

	ComPtr<ID3D12GraphicsCommandList> commandList = m_DirectCommandQueue->GetCommandList();

	InitAppState();

	BuildFrameResources();

	BuildBoxAndPiramidGeometry(commandList);

	m_PiramidMVP.ModelMatrix = XMMatrixTranslation(2, 0, 2);

	for (uint32_t i = 0; i < m_NumBackBuffers; ++i) {
		m_FramesResources[i]->m_ObjectsConstantsBuffer->CopyData(1, m_PiramidMVP);
	}

	m_CBDescHeap = CreateDescriptorHeap(
		(m_NumGeo + 1) * m_NumBackBuffers,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);
	BuildObjectsAndPassConstantsBufferViews();

	BuildRootSignature();
	BuildPipelineStateObject();

	// for Sobel filter
	m_FrameTextureRTVDescHeap = CreateDescriptorHeap(1, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
	m_FrameTextureSRVDescHeap = CreateDescriptorHeap(1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
	UpdateFramesTextures();
	BuildSobelRootSignature();
	BuildSobelPipelineStateObject();

	// wait while all data loaded
	uint32_t fenceValue = m_DirectCommandQueue->ExecuteCommandList(commandList);
	m_DirectCommandQueue->WaitForFenceValue(fenceValue);

	// release upload heaps after vertex and inedx loading
	m_BoxAndPiramidGeo.DisposeUploaders();

	return true;
}

void SimpleGeoApp::OnUpdate() {
	m_Timer.Tick();

	if (m_Timer.GetMeasuredTime() >= 1.0) {
		char buffer[500];
		auto fps = m_Timer.GetMeasuredTicks() / m_Timer.GetMeasuredTime();
		::sprintf_s(buffer, 500, "FPS: %f\n", fps);
		::OutputDebugString(buffer);

		m_Timer.StartMeasurement();
	}
	
	m_CurrentFrameResources = m_FramesResources[m_CurrentBackBufferIndex].get();

	// update pass constants
	XMMATRIX View = m_Camera.GetViewMatrix();
	XMMATRIX Proj = GetProjectionMatrix();
	XMMATRIX ViewProj = XMMatrixMultiply(View, Proj);

	m_CurrentFrameResources->m_PassConstantsBuffer->CopyData(
		0,
		{
			float(m_Timer.GetTotalTime()),
			View,
			Proj,
			ViewProj
		}
	);

	// update objects constants
	float angle = static_cast<float>(m_Timer.GetTotalTime() * 90.0);
	const XMVECTOR rotationAxis = XMVectorSet(0, 1, 1, 0);
	m_BoxMVP.ModelMatrix = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(angle));

	m_CurrentFrameResources->m_ObjectsConstantsBuffer->CopyData(0, m_BoxMVP);
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
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
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
			0, 0, NULL
		);
	}

	// draw geometry
	{
		// set root signature
		commandList->SetGraphicsRootSignature(m_RootSignature.Get());

		// set descripotr heaps
		ID3D12DescriptorHeap* descriptorHeaps[] = { m_CBDescHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		// set pass constants
		commandList->SetGraphicsRootDescriptorTable(
			1, 
			CD3DX12_GPU_DESCRIPTOR_HANDLE(
				m_CBDescHeap->GetGPUDescriptorHandleForHeapStart(),
				m_NumGeo * m_NumBackBuffers + m_CurrentBackBufferIndex,
				m_CBDescSize
			)
		);

		// set PSO
		// chose pso depending on z-buffer type
		ComPtr<ID3D12PipelineState> pso;

		if (m_IsInverseDepth) {
			pso = m_PSOs["inverseDepth"];
		}
		else {
			pso = m_PSOs["straightDepth"];
		}

		commandList->SetPipelineState(pso.Get());

		// set Rasterizer Stage
		commandList->RSSetScissorRects(1, &m_ScissorRect);
		commandList->RSSetViewports(1, &m_ViewPort);

		// set Output Mergere Stage
		commandList->OMSetRenderTargets(1, &geometryRTV, FALSE, &m_DSVDescHeap->GetCPUDescriptorHandleForHeapStart());

		// set Input Asembler Stage
		commandList->IASetVertexBuffers(0, 1, &m_BoxAndPiramidGeo.VertexBufferView());
		commandList->IASetIndexBuffer(&m_BoxAndPiramidGeo.IndexBufferView());
		commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// draw box
		// set box descriptor to root signature
		CD3DX12_GPU_DESCRIPTOR_HANDLE geoCBDescHandle(
			m_CBDescHeap->GetGPUDescriptorHandleForHeapStart(),
			m_CurrentBackBufferIndex * m_NumGeo,
			m_CBDescSize
		);

		commandList->SetGraphicsRootDescriptorTable(0, geoCBDescHandle);

		// draw vertexes by its indexes and primitive topology
		SubmeshGeometry submes = m_BoxAndPiramidGeo.DrawArgs["box"];
		commandList->DrawIndexedInstanced(
			submes.IndexCount,
			1,
			submes.StartIndexLocation,
			submes.BaseVertexLocation,
			0
		);

		// draw piramid
		// set piramid descriptor to root signature
		commandList->SetGraphicsRootDescriptorTable(0, geoCBDescHandle.Offset(m_CBDescSize));

		// draw vertexes by its indexes and primitive topology
		submes = m_BoxAndPiramidGeo.DrawArgs["piramid"];
		commandList->DrawIndexedInstanced(
			submes.IndexCount,
			1,
			submes.StartIndexLocation,
			submes.BaseVertexLocation,
			0
		);
	}

	// apply filter and draw to main render target
	if (m_IsSobelFilter) {
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			frameTextureBuffer.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
		);

		commandList->ResourceBarrier(1, &barrier);

		// unbind all resources from pipeline
		commandList->ClearState(NULL);

		// set root signature and descripotr heaps
		commandList->SetGraphicsRootSignature(m_SobelRootSignature.Get());
		ID3D12DescriptorHeap* descriptorHeaps[] = { m_FrameTextureSRVDescHeap.Get() };
		commandList->SetDescriptorHeaps(1, descriptorHeaps);

		// set pso
		commandList->SetPipelineState(m_SobelPSO.Get());

		// set Rasterizer Stage
		commandList->RSSetScissorRects(1, &m_ScissorRect);
		commandList->RSSetViewports(1, &m_ViewPort);

		// set Output Mergere Stage
		commandList->OMSetRenderTargets(1, &mainRTV, FALSE, NULL);

		// set Input Asembler Stage
		commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// set root parameter
		commandList->SetGraphicsRootDescriptorTable(0, m_FrameTextureSRVDescHeap->GetGPUDescriptorHandleForHeapStart());

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
		InitAppState();
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

void SimpleGeoApp::InitAppState() {
	m_Camera = Camera();

	m_Timer = Timer();
	m_Timer.StartMeasurement();

	// init shake effect state data
	m_IsShakeEffect = false;
	m_ShakePixelAmplitude = 5.0f;
	m_ShakeDirections = {
		{ 0.0f,     1.0f,  0.0f,    0.0f },
		{ 0.0f,     -1.0f, 0.0f,    0.0f },
		{ 1.0f,     0.0f,  0.0f,    0.0f },
		{ -1.0f,    0.0f,  0.0f,    0.0f }
	};
	m_ShakeDirectionIndex = 0;
	//
}

void SimpleGeoApp::BuildRootSignature() {
	CD3DX12_ROOT_PARAMETER1 rootParameters[2];

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRange1;
	descriptorRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRange2;
	descriptorRange2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

	rootParameters[0].InitAsDescriptorTable(1, &descriptorRange1);
	rootParameters[1].InitAsDescriptorTable(1, &descriptorRange2);

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, NULL, rootSignatureFlags);

	D3D12_FEATURE_DATA_ROOT_SIGNATURE rsVersion;
	rsVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(m_Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rsVersion, sizeof(rsVersion)))) {
		rsVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	ComPtr<ID3DBlob> rootSignatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
		&rootSignatureDesc,
		rsVersion.HighestVersion,
		&rootSignatureBlob,
		&errorBlob
	));

	ThrowIfFailed(m_Device->CreateRootSignature(
		0,
		rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&m_RootSignature)
	));
}

void SimpleGeoApp::BuildBoxAndPiramidGeometry(ComPtr<ID3D12GraphicsCommandList> commandList) {
	std::array<VertexPosColor, 13> vertexes = {
		// box
		VertexPosColor({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }), // 0
		VertexPosColor({ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) }), // 1
		VertexPosColor({ XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f) }), // 2
		VertexPosColor({ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }), // 3
		VertexPosColor({ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) }), // 4
		VertexPosColor({ XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 1.0f) }), // 5
		VertexPosColor({ XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) }), // 6
		VertexPosColor({ XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 1.0f) }), // 7
		// piramid
		VertexPosColor({ XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }), // 0
		VertexPosColor({ XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) }), // 1
		VertexPosColor({ XMFLOAT3(1.0f, 0.0f, 1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f) }), // 2
		VertexPosColor({ XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }), // 3
		VertexPosColor({ XMFLOAT3(0.5f, 1.0f, 0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f) }), // 4
	};


	std::array<uint16_t, 54> indexes =
	{
		// box
		0, 1, 2, 0, 2, 3,
		4, 6, 5, 4, 7, 6,
		4, 5, 1, 4, 1, 0,
		3, 2, 6, 3, 6, 7,
		1, 5, 6, 1, 6, 2,
		4, 0, 3, 4, 3, 7,
		// piramid
		0, 1, 3,
		1, 2, 3,
		0, 4, 1,
		1, 4, 2,
		2, 4, 3,
		0, 3, 4
	};

	UINT vbByteSize = vertexes.size() * sizeof(VertexPosColor);
	UINT ibByteSize = indexes.size() * sizeof(uint16_t);

	m_BoxAndPiramidGeo.VertexBufferGPU = CreateGPUResourceAndLoadData(
		commandList,
		m_BoxAndPiramidGeo.VertexBufferUploader,
		vertexes.data(),
		vbByteSize
	);

	m_BoxAndPiramidGeo.IndexBufferGPU = CreateGPUResourceAndLoadData(
		commandList,
		m_BoxAndPiramidGeo.IndexBufferUploader,
		indexes.data(),
		ibByteSize
	);

	m_BoxAndPiramidGeo.name = "BoxAndPiramidGeo";
	m_BoxAndPiramidGeo.VertexBufferByteSize = vbByteSize;
	m_BoxAndPiramidGeo.VertexByteStride = sizeof(VertexPosColor);
	m_BoxAndPiramidGeo.IndexBufferByteSize = ibByteSize;
	m_BoxAndPiramidGeo.IndexBufferFormat = DXGI_FORMAT_R16_UINT;

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = 36;
	boxSubmesh.BaseVertexLocation = 0;
	boxSubmesh.StartIndexLocation = 0;

	SubmeshGeometry piramidSubmesh;
	piramidSubmesh.IndexCount = 18;
	piramidSubmesh.BaseVertexLocation = 8;
	piramidSubmesh.StartIndexLocation = 36;

	m_BoxAndPiramidGeo.DrawArgs["piramid"] = piramidSubmesh;
	m_BoxAndPiramidGeo.DrawArgs["box"] = boxSubmesh;
}

void SimpleGeoApp::BuildFrameResources() {
	m_FramesResources.reserve(m_NumBackBuffers);

	for (uint32_t i = 0; i < m_NumBackBuffers; ++i) {
		m_FramesResources.push_back(std::make_unique<FrameResources>(m_Device, 1, m_NumGeo));
	}
}

void SimpleGeoApp::BuildObjectsAndPassConstantsBufferViews() {
	m_CBDescSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle(m_CBDescHeap->GetCPUDescriptorHandleForHeapStart());

	uint32_t objectConstansElementByteSize = m_FramesResources[0]->m_ObjectsConstantsBuffer->GetElementByteSize();

	for (uint32_t i = 0; i < m_NumBackBuffers; ++i) {
		auto objectsConstantsBuffer = m_FramesResources[i]->m_ObjectsConstantsBuffer->Get();
		D3D12_GPU_VIRTUAL_ADDRESS objectConstantsBufferGPUAdress = objectsConstantsBuffer->GetGPUVirtualAddress();

		for (uint32_t j = 0; j < m_NumGeo; ++j) {
			D3D12_CONSTANT_BUFFER_VIEW_DESC CBViewDesc;

			CBViewDesc.BufferLocation = objectConstantsBufferGPUAdress;
			CBViewDesc.SizeInBytes = objectConstansElementByteSize;

			m_Device->CreateConstantBufferView(
				&CBViewDesc,
				descHandle
			);

			descHandle.Offset(m_CBDescSize);
			objectConstantsBufferGPUAdress += objectConstansElementByteSize;
		}
	}

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

		descHandle.Offset(m_CBDescSize);
	}
}

void SimpleGeoApp::BuildPipelineStateObject() {
	// Compile shaders	
	m_VertexShaderBlob = CompileShader(L"..\\shaders\\VertexShader.hlsl", "main", "vs_5_1");
	m_PixelShaderBlob = CompileShader(L"..\\shaders\\PixelShader.hlsl", "main", "ps_5_1");

	// Create rasterizer state description
	CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
	//rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;

	// Create input layout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[]{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	D3D12_INPUT_LAYOUT_DESC inpuitLayout{ inputElementDescs, _countof(inputElementDescs) };

	// Create pipeline state object description
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;

	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	psoDesc.pRootSignature = m_RootSignature.Get();

	psoDesc.VS = {
		reinterpret_cast<BYTE*>(m_VertexShaderBlob->GetBufferPointer()),
		m_VertexShaderBlob->GetBufferSize()
	};

	psoDesc.PS = {
		reinterpret_cast<BYTE*>(m_PixelShaderBlob->GetBufferPointer()),
		m_PixelShaderBlob->GetBufferSize()
	};

	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.InputLayout = inpuitLayout;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc = { 1, 0 };

	ComPtr<ID3D12PipelineState> straightDepthPSO;
	ComPtr<ID3D12PipelineState> inverseDepthPSO;

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&straightDepthPSO)));

	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&inverseDepthPSO)));

	m_PSOs["straightDepth"] = straightDepthPSO;
	m_PSOs["inverseDepth"] = inverseDepthPSO;
}

XMMATRIX SimpleGeoApp::GetProjectionMatrix() {
	float aspectRatio = m_ClientHeight / static_cast<float>(m_ClientWidth);
	float focalLengh = 1 / tan(XMConvertToRadians(m_Camera.GetFoV()) / 2);

	float n = 0.1f;
	float f = 100.0f;

	float l = -n / focalLengh;
	float r = n / focalLengh;
	float b = -aspectRatio * n / focalLengh;
	float t = aspectRatio * n / focalLengh;

	float alpha = 0.0f;
	float beta = 1.0f;

	if (m_IsInverseDepth) {
		alpha = 1.0f;
		beta = 0.0f;
	}

	XMVECTOR xProj = { 2 * n / (r - l), 0.0f, (r + l) / (r - l), 0.0f };
	XMVECTOR yProj = { 0.0f, 2 * n / (t - b), (t + b) / (t - b), 0.0f };
	XMVECTOR zProj = { 0.0f, 0.0f, -(alpha * n - beta * f) / (f - n), (alpha - beta) * n * f / (f - n) };
	XMVECTOR wProj = { 0.0f, 0.0f, 1.0f, 0.0f };
	XMMATRIX projectionMatrix = { xProj, yProj, zProj, wProj };
	projectionMatrix = XMMatrixTranspose(projectionMatrix);

	if (m_IsShakeEffect) {
		XMVECTOR pixelNorm = { 2.0f / m_ClientWidth, 2.0f / m_ClientHeight, 0.0f, 0.0f };
		XMVECTOR displacement = m_ShakeDirections[m_ShakeDirectionIndex] * pixelNorm;

		projectionMatrix.r[2] += displacement * m_ShakePixelAmplitude;
		m_ShakeDirectionIndex = (m_ShakeDirectionIndex + 1) % m_ShakeDirections.size();
	}

	return projectionMatrix;
}

void SimpleGeoApp::UpdateFramesTextures() {
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_ClientWidth, m_ClientHeight);
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	CD3DX12_CLEAR_VALUE clearValue{ DXGI_FORMAT_R8G8B8A8_UNORM , m_BackGroundColor };

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
		&resourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
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

	rootParameters[0].InitAsDescriptorTable(1, &descriptorRange);

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(1, rootParameters, 0, NULL, rootSignatureFlags);

	D3D12_FEATURE_DATA_ROOT_SIGNATURE rsVersion;
	rsVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(m_Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rsVersion, sizeof(rsVersion)))) {
		rsVersion.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	ComPtr<ID3DBlob> rootSignatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
		&rootSignatureDesc,
		rsVersion.HighestVersion,
		&rootSignatureBlob,
		&errorBlob
	));

	ThrowIfFailed(m_Device->CreateRootSignature(
		0,
		rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&m_SobelRootSignature)
	));
}

void SimpleGeoApp::BuildSobelPipelineStateObject() {
	// Compile shaders	
	m_SobelVertexShaderBlob = CompileShader(L"..\\shaders\\SobelVertexShader.hlsl", "main", "vs_5_1");
	m_SobelPixelShaderBlob = CompileShader(L"..\\shaders\\SobelPixelShader.hlsl", "main", "ps_5_1");

	// Create pipeline state object description
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;

	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	psoDesc.pRootSignature = m_SobelRootSignature.Get();

	psoDesc.VS = {
		reinterpret_cast<BYTE*>(m_SobelVertexShaderBlob->GetBufferPointer()),
		m_SobelVertexShaderBlob->GetBufferSize()
	};

	psoDesc.PS = {
		reinterpret_cast<BYTE*>(m_SobelPixelShaderBlob->GetBufferPointer()),
		m_SobelPixelShaderBlob->GetBufferSize()
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

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_SobelPSO)));
}