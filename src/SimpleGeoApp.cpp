#include <d3dx12.h>

#include <chrono>
#include <array>

#include <SimpleGeoApp.h>

SimpleGeoApp::SimpleGeoApp(HINSTANCE hInstance) : BaseApp(hInstance) {}

SimpleGeoApp::~SimpleGeoApp() {};

bool SimpleGeoApp::Initialize() {
	if (!BaseApp::Initialize()) {
		return false;
	}

	// init shake effect state data
	m_ShakePixelAmplitude = 5.0f;
	m_ShakeDirections = {
		{ 0.0f,     1.0f,  0.0f,    0.0f },
		{ 0.0f,     -1.0f, 0.0f,    0.0f },
		{ 1.0f,     0.0f,  0.0f,    0.0f },
		{ -1.0f,    0.0f,  0.0f,    0.0f }
	};
	m_ShakeDirectionIndex = 0;
	//

	ComPtr<ID3D12GraphicsCommandList> commandList = m_DirectCommandQueue->GetCommandList();

	BuildBoxGeometry(commandList);
	BuildBoxConstantBuffer();

	BuildRootSignature();
	BuildPipelineStateObject();

	// wait while all data loaded
	uint32_t fenceValue = m_DirectCommandQueue->ExecuteCommandList(commandList);
	m_DirectCommandQueue->WaitForFenceValue(fenceValue);

	// release upload heaps after vertex and inedx loading
	m_BoxGeo.DisposeUploaders();

	return true;
}

void SimpleGeoApp::OnUpdate() {
	static double elapsedTime = 0.0;
	static double totalTime = 0.0;
	static uint64_t frameCounter = 0;
	static std::chrono::high_resolution_clock clock;
	static auto prevTime = clock.now();

	auto currentTime = clock.now();
	auto deltaTime = currentTime - prevTime;
	prevTime = currentTime;
	elapsedTime += deltaTime.count() * 1e-9;
	totalTime += deltaTime.count() * 1e-9;
	++frameCounter;

	if (elapsedTime >= 1.0) {
		char buffer[500];
		auto fps = frameCounter / elapsedTime;
		::sprintf_s(buffer, 500, "FPS: %f\n", fps);
		::OutputDebugString(buffer);

		frameCounter = 0;
		elapsedTime = 0.0;
	}

	float angle = static_cast<float>(0 * 90.0);
	const XMVECTOR rotationAxis = XMVectorSet(0, 1, 1, 0);
	XMMATRIX modelMatrix = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(angle));

	// spherical coordinates to cartesian
	float x = m_Radius * sinf(m_Phi) * cosf(m_Theta);
	float z = m_Radius * sinf(m_Phi) * sinf(m_Theta);
	float y = m_Radius * cosf(m_Phi);

	m_CameraPos = m_FocusPos + XMVectorSet(x, y, z, 1);
	m_CameraForwardDirection = -XMVector3Normalize(XMVectorSet(x, y, z, 0));
	m_CameraRightDirection = XMVector3Normalize(XMVector3Cross(m_CameraForwardDirection, m_CameraUpDirection));

	XMMATRIX viewMatrix = XMMatrixLookAtLH(m_CameraPos, m_FocusPos, m_CameraUpDirection);

	XMMATRIX projectionMatrix = GetProjectionMatrix();

	m_BoxMVP.MVP = XMMatrixMultiply(modelMatrix, viewMatrix);
	m_BoxMVP.MVP = XMMatrixMultiply(m_BoxMVP.MVP, projectionMatrix);

	LoadDataToCB<ObjectConstants>(m_BoxConstBuffer, m_BoxMVP, m_BoxCBSize);
}

void SimpleGeoApp::OnRender() {
	ComPtr<ID3D12GraphicsCommandList> commandList = m_DirectCommandQueue->GetCommandList();
	auto backBuffer = m_BackBuffers[m_CurrentBackBufferIndex];

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(
		m_BackBuffersDescHeap->GetCPUDescriptorHandleForHeapStart(),
		m_CurrentBackBufferIndex, m_RTVDescSize
	);

	// Clear RTV
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.Get(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);

		commandList->ResourceBarrier(1, &barrier);

		FLOAT backgroundColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
		commandList->ClearRenderTargetView(rtv, backgroundColor, 0, NULL);

		commandList->ClearDepthStencilView(
			m_DSVDescHeap->GetCPUDescriptorHandleForHeapStart(),
			D3D12_CLEAR_FLAG_DEPTH,
			m_DepthClearValue, 
			0, 0, NULL
		);
	}

	// Set root signature and its parameters
	commandList->SetGraphicsRootSignature(m_RootSignature.Get());
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_BoxCBDescHeap.Get() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);
	commandList->SetGraphicsRootDescriptorTable(0, m_BoxCBDescHeap->GetGPUDescriptorHandleForHeapStart());

	// Set PSO
	// chose pso depending on z-buffer type
	ComPtr<ID3D12PipelineState> pso;

	if (m_IsInverseDepth) {
		pso = m_PSOs["inverseDepth"];
	}
	else {
		pso = m_PSOs["straightDepth"];
	}

	commandList->SetPipelineState(pso.Get());

	// Set Input Asembler Stage
	commandList->IASetVertexBuffers(0, 1, &m_BoxGeo.VertexBufferView());
	commandList->IASetIndexBuffer(&m_BoxGeo.IndexBufferView());
	commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set Rasterizer Stage
	commandList->RSSetScissorRects(1, &m_ScissorRect);
	commandList->RSSetViewports(1, &m_ViewPort);

	// Set Output Mergere Stage
	commandList->OMSetRenderTargets(1, &rtv, FALSE, &m_DSVDescHeap->GetCPUDescriptorHandleForHeapStart());

	// Draw vertexes by its indexes and primitive topology
	SubmeshGeometry submes = m_BoxGeo.DrawArgs["box"];
	commandList->DrawIndexedInstanced(
		submes.IndexCount,
		1,
		submes.StartIndexLocation,
		submes.BaseVertexLocation,
		0
	);

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
}

void SimpleGeoApp::OnKeyPressed(WPARAM wParam) {
	switch (wParam)
	{
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
	case 'W':
		// Forward
		m_FocusPos += 0.4 * m_CameraForwardDirection;
		break;
	case 'S':
		// Backward
		m_FocusPos -= 0.4 * m_CameraForwardDirection;
		break;
	case 'A':
		// Left
		m_FocusPos += 0.4 * m_CameraRightDirection;
		break;
	case 'D':
		// Right
		m_FocusPos -= 0.4 * m_CameraRightDirection;
		break;
	}
}

void SimpleGeoApp::OnMouseWheel(int wheelDelta) {
	m_FoV += wheelDelta / 10.0f;
	m_FoV = clamp(m_FoV, 12.0f, 90.0f);
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
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x -
			m_LastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y -
			m_LastMousePos.y));
		// Update angles based on input to orbit camera around box.
		m_Theta -= dx;
		m_Phi -= dy;
		// Restrict the angle mPhi.
		m_Phi = clamp(m_Phi, 0.1f, XM_PI - 0.1f);
	} else if ((wParam & MK_RBUTTON) != 0) {
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f * static_cast<float>(x - m_LastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - m_LastMousePos.y);
		// Update the camera radius based on input.
		m_Radius += dx - dy;
		// Restrict the radius.
		m_Radius = clamp(m_Radius, 3.0f, 15.0f);
	}

	m_LastMousePos.x = x;
	m_LastMousePos.y = y;
}

void SimpleGeoApp::BuildRootSignature() {
	CD3DX12_ROOT_PARAMETER1 rootParameters[1];

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRange;
	descriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	rootParameters[0].InitAsDescriptorTable(1, &descriptorRange);

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

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
		IID_PPV_ARGS(&m_RootSignature)
	));
}

void SimpleGeoApp::BuildBoxGeometry(ComPtr<ID3D12GraphicsCommandList> commandList) {
	std::array<VertexPosColor, 8> vertexes = {
	VertexPosColor({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }), // 0
	VertexPosColor({ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) }), // 1
	VertexPosColor({ XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f) }), // 2
	VertexPosColor({ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }), // 3
	VertexPosColor({ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) }), // 4
	VertexPosColor({ XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 1.0f) }), // 5
	VertexPosColor({ XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) }), // 6
	VertexPosColor({ XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 1.0f) })  // 7
	};

	std::array<uint16_t, 36> indexes =
	{
		0, 1, 2, 0, 2, 3,
		4, 6, 5, 4, 7, 6,
		4, 5, 1, 4, 1, 0,
		3, 2, 6, 3, 6, 7,
		1, 5, 6, 1, 6, 2,
		4, 0, 3, 4, 3, 7
	};

	UINT vbByteSize = vertexes.size() * sizeof(VertexPosColor);
	UINT ibByteSize = indexes.size() * sizeof(uint16_t);

	m_BoxGeo.VertexBufferGPU = CreateGPUResourceAndLoadData(
		commandList,
		m_BoxGeo.VertexBufferUploader,
		vertexes.data(),
		vbByteSize
	);

	m_BoxGeo.IndexBufferGPU = CreateGPUResourceAndLoadData(
		commandList,
		m_BoxGeo.IndexBufferUploader,
		indexes.data(),
		ibByteSize
	);

	m_BoxGeo.name = "BoxGeo";
	m_BoxGeo.VertexBufferByteSize = vbByteSize;
	m_BoxGeo.VertexByteStride = sizeof(VertexPosColor);
	m_BoxGeo.IndexBufferByteSize = ibByteSize;
	m_BoxGeo.IndexBufferFormat = DXGI_FORMAT_R16_UINT;

	SubmeshGeometry submesh;
	submesh.IndexCount = indexes.size();
	submesh.BaseVertexLocation = 0;
	submesh.StartIndexLocation = 0;

	m_BoxGeo.DrawArgs["box"] = submesh;
}

void SimpleGeoApp::BuildBoxConstantBuffer() {
	m_BoxCBSize = (sizeof(ObjectConstants) + 255) & ~255;
	m_BoxConstBuffer = CreateConstantBuffer(m_BoxCBSize);
	LoadDataToCB<ObjectConstants>(m_BoxConstBuffer, m_BoxMVP, m_BoxCBSize);

	m_BoxCBDescHeap = CreateDescriptorHeap(
		1,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	);

	UpdateCBView(m_BoxConstBuffer, m_BoxCBSize, m_BoxCBDescHeap);
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

	CD3DX12_DEPTH_STENCIL_DESC inverseDepthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	inverseDepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

	psoDesc.DepthStencilState = inverseDepthStencilDesc;

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&inverseDepthPSO)));

	m_PSOs["straightDepth"] = straightDepthPSO;
	m_PSOs["inverseDepth"] = inverseDepthPSO;
}

XMMATRIX SimpleGeoApp::GetProjectionMatrix() {
	float aspectRatio = m_ClientHeight / static_cast<float>(m_ClientWidth);
	float focalLengh = 1 / tan(XMConvertToRadians(m_FoV) / 2);

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