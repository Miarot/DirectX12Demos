#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#if defined(CreateWindow)
#undef CreateWindow
#endif

#if defined(max)
#undef max
#endif

#if defined(min)
#undef min
#endif

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dx12.h>
#include <D3DCompiler.h>
#include <DirectXMath.h>
using namespace DirectX;

#include <wrl.h>
using Microsoft::WRL::ComPtr;

#include <cassert>
#include <algorithm>
#include <array>
#include <chrono>

#include <helpers.h>
#include <MeshGeometry.h>

// window parameters
HWND g_windowHandle;
uint32_t g_Width = 1280;
uint32_t g_Height = 720;

// render parameters
bool g_UseWarp = false;
const uint32_t g_BufferCount = 3;
bool g_AllowTearing = false;
bool g_Vsync = true;
uint32_t g_CurrentBackBuffer;
bool g_IsInit = false;
bool g_FullScreen = false;
RECT g_WindowRect;
D3D12_RECT g_ScissorRect;
D3D12_VIEWPORT g_ViewPort;

// DirectX objects
ComPtr<IDXGIAdapter4> g_Adapter;
ComPtr<ID3D12Device2> g_Device;
ComPtr<ID3D12CommandQueue> g_CommandQueue;
ComPtr<IDXGISwapChain4> g_SwapChain;
ComPtr<ID3D12CommandAllocator> g_CommandAllocators[g_BufferCount];
ComPtr<ID3D12GraphicsCommandList> g_CommandList;
ComPtr<ID3D12Resource> g_BackBuffers[g_BufferCount];
ComPtr<ID3D12DescriptorHeap> g_RTVDescHeap;
uint32_t g_RTVDescSize;
ComPtr<ID3D12Fence> g_Fence;
UINT64 g_FenceValue;
HANDLE g_FenceEvent;
UINT64 g_BuffersFenceValues[g_BufferCount];
ComPtr<ID3D12Resource> g_DSBuffer;
ComPtr<ID3D12DescriptorHeap> g_DSVDescHeap;
ComPtr<ID3DBlob> g_PixelShaderBlob;
ComPtr<ID3DBlob> g_VertexShaderBlob;
ComPtr<ID3D12Resource> g_ConstantBuffer;
UINT g_CBSize;
ComPtr<ID3D12DescriptorHeap> g_CBDescHeap;
ComPtr<ID3D12RootSignature> g_RootSignature;
ComPtr<ID3D12PipelineState> g_PSO;


// Game objects and structures
struct VertexPosColor {
	XMFLOAT3 Position;
	XMFLOAT3 Color;
};

struct ObjectConstants {
	XMMATRIX MVP = XMMatrixIdentity();
};

ObjectConstants g_ObjectConstants;



MeshGeometry g_BoxGeo;

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void RegisterWindowClass(HINSTANCE hInstance, const wchar_t * className) {
	WNDCLASSEXW windowDesc = {};

	windowDesc.cbSize = sizeof(WNDCLASSEXW);
	windowDesc.style = CS_HREDRAW | CS_VREDRAW;
	windowDesc.lpfnWndProc = &WndProc;
	windowDesc.cbClsExtra = 0;
	windowDesc.cbWndExtra = 0;
	windowDesc.hInstance = hInstance;
	windowDesc.hIcon = ::LoadIcon(hInstance, NULL);
	windowDesc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	windowDesc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	windowDesc.lpszMenuName = NULL;
	windowDesc.lpszClassName = className;
	windowDesc.hIconSm = ::LoadIcon(hInstance, NULL);

	ATOM atom = ::RegisterClassExW(&windowDesc);

	assert(atom > 0 && "Can`t register window class");
}

HWND CreateWindow(
	HINSTANCE hInstance, 
	const wchar_t * className, 
	const wchar_t * windowName,
	uint32_t width, 
	uint32_t height) 
{
	RECT windowRect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
	::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	int windowWidth = windowRect.right - windowRect.left;
	int windowHeight = windowRect.bottom - windowRect.top;

	int systemWidth = ::GetSystemMetrics(SM_CXSCREEN);
	int systemHeight = ::GetSystemMetrics(SM_CYSCREEN);
	
	int windowX = std::max(0, (systemWidth - windowWidth) / 2);
	int windowY = std::max(0, (systemHeight - windowHeight) / 2);

	HWND windowHandle = ::CreateWindowExW(
		NULL,
		className,
		windowName,
		WS_OVERLAPPEDWINDOW,
		windowX,
		windowY,
		windowWidth,
		windowHeight,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	assert(windowHandle && "Cant`t create window");

	return windowHandle;
}

void EnableDebugLayer() {
#ifdef _DEBUG
	ComPtr<ID3D12Debug> debugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();
#endif // DEBUG
}

bool CheckTearingSupport() {
	BOOL allowTearing = FALSE;
	ComPtr<IDXGIFactory4> factory4;
	ComPtr<IDXGIFactory5> factory5;

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory4)));
	ThrowIfFailed(factory4.As(&factory5));

	ThrowIfFailed(factory5->CheckFeatureSupport(
		DXGI_FEATURE_PRESENT_ALLOW_TEARING, 
		&allowTearing, sizeof(allowTearing)
	));

	return allowTearing == TRUE;
}

ComPtr<IDXGIAdapter4> CreateAdapter(bool useWarp) {
	ComPtr<IDXGIFactory4> factory;
	UINT factorFlags = 0;
#ifdef _DEBUG
	factorFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif // DEBUG

	ThrowIfFailed(CreateDXGIFactory2(factorFlags, IID_PPV_ARGS(&factory)));

	ComPtr<IDXGIAdapter1> adapter1;
	ComPtr<IDXGIAdapter4> adapter4;

	if (useWarp) {
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter1)));
	} else {
		UINT i = 0;
		SIZE_T maxDedicatedMemory = 0;
		
		while (factory->EnumAdapters1(i, &adapter1) != DXGI_ERROR_NOT_FOUND) {
			DXGI_ADAPTER_DESC1 adapterDesc;
			adapter1->GetDesc1(&adapterDesc);

#ifdef _DEBUG
			OutputDebugStringW(adapterDesc.Description);
			OutputDebugStringW(L"\n");
#endif // DEBUG
			
			if ((adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(adapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device2), NULL)) &&
				adapterDesc.DedicatedVideoMemory > maxDedicatedMemory)
			{
				ThrowIfFailed(adapter1.As(&adapter4));
				maxDedicatedMemory = adapterDesc.DedicatedVideoMemory;
			}

			++i;
		}
	}
	
	return adapter4;
}

ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter) {
	ComPtr<ID3D12Device2> device;
	ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

#ifdef _DEBUG
	ComPtr<ID3D12InfoQueue> infoQueue;
	ThrowIfFailed(device.As(&infoQueue));

	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

	//D3D12_MESSAGE_CATEGORY categories[] = {};

	//D3D12_MESSAGE_SEVERITY severities[] = {
	//	D3D12_MESSAGE_SEVERITY_INFO
	//};

	//D3D12_MESSAGE_ID denyIDs[]{
	//	D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
	//	D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
	//	D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
	//};

	//D3D12_INFO_QUEUE_FILTER filter{};
	//filter.DenyList.NumSeverities = _countof(severities);
	//filter.DenyList.pSeverityList = severities;
	//filter.DenyList.NumIDs = _countof(denyIDs);
	//filter.DenyList.pIDList = denyIDs;
	//
	//ThrowIfFailed(infoQueue->PushStorageFilter(&filter));
#endif // _DEBUG

	return device;
}

ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device2> device) {
	ComPtr<ID3D12CommandQueue> commandQueue;

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc;

	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	commandQueueDesc.NodeMask = 0;

	ThrowIfFailed(device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue)));

	return commandQueue;
}

ComPtr<IDXGISwapChain4> CreateSwapChain(
	ComPtr<ID3D12CommandQueue> commandQueue,
	HWND windowHandl,
	UINT width, UINT height, bool allowTearing)
{
	ComPtr<IDXGISwapChain1> swapChain1;
	ComPtr<IDXGISwapChain4> swapChain4;

	ComPtr<IDXGIFactory4> factory;
	UINT factorFlags = 0;
#ifdef _DEBUG
	factorFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif // DEBUG
	ThrowIfFailed(CreateDXGIFactory2(factorFlags, IID_PPV_ARGS(&factory)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc;

	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = DXGI_SAMPLE_DESC{ 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = g_BufferCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		commandQueue.Get(), 
		windowHandl, 
		&swapChainDesc, 
		NULL, 
		NULL, 
		&swapChain1
	));

	ThrowIfFailed(factory->MakeWindowAssociation(windowHandl, DXGI_MWA_NO_ALT_ENTER));
	ThrowIfFailed(swapChain1.As(&swapChain4));

	return swapChain4;
}

ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device2> device) {
	ComPtr<ID3D12CommandAllocator> commandAllocator;

	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT, 
		IID_PPV_ARGS(&commandAllocator)
	));
	
	return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList> CreateCommandList(
	ComPtr<ID3D12Device2> device, 
	ComPtr<ID3D12CommandAllocator> commandAllocator) 
{
	ComPtr<ID3D12GraphicsCommandList> commandList;

	ThrowIfFailed(device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&commandList)
	));

	commandList->Close();

	return commandList;
}

ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
	ComPtr<ID3D12Device2> device,
	UINT numDescriptors,
	D3D12_DESCRIPTOR_HEAP_TYPE type,
	D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	D3D12_DESCRIPTOR_HEAP_DESC descriptroHeapDesc;
	descriptroHeapDesc.Type = type;
	descriptroHeapDesc.NumDescriptors = numDescriptors;
	descriptroHeapDesc.Flags = flags;
	descriptroHeapDesc.NodeMask = 0;

	ThrowIfFailed(device->CreateDescriptorHeap(&descriptroHeapDesc, IID_PPV_ARGS(&descriptorHeap)));

	return descriptorHeap;
}

void UpdateRTV(
	ComPtr<ID3D12Device2> device,
	ComPtr<IDXGISwapChain> swapChain,
	ComPtr<ID3D12DescriptorHeap> descriptorHeap,
	ComPtr<ID3D12Resource> * resoureces,
	uint32_t numDescriptors,
	uint32_t descriptorSize)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());
	ComPtr<ID3D12Resource> currentResource;

	for (uint32_t i = 0; i < numDescriptors; ++i) {
		ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&currentResource)));
		resoureces[i] = currentResource;
		device->CreateRenderTargetView(currentResource.Get(), nullptr, descriptorHandle);
		descriptorHandle.Offset(descriptorSize);
	}
}

ComPtr<ID3D12Resource> CreateDepthStencilBuffer(ComPtr<ID3D12Device2> device, UINT width, UINT height) {
	ComPtr<ID3D12Resource> depthStencilBuffer;

	D3D12_RESOURCE_DESC dsBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_D32_FLOAT,
		width, height,
		1, 0, 1, 0,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		D3D12_TEXTURE_LAYOUT_UNKNOWN,
		0
	);

	D3D12_CLEAR_VALUE dsClearValue{};
	dsClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	dsClearValue.DepthStencil = { 1.0f, 0 };

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&dsBufferDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&dsClearValue,
		IID_PPV_ARGS(&depthStencilBuffer)
	));

	return depthStencilBuffer;
}

ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> device, UINT64 initValue) {
	ComPtr<ID3D12Fence> fence;
	ThrowIfFailed(device->CreateFence(initValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	return fence;
}

UINT64 Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, UINT64 & value) {
	UINT64 signalValue = ++value;
	commandQueue->Signal(fence.Get(), signalValue);
	return signalValue;
}

HANDLE CreateEventHandle() {
	HANDLE event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(event && "Can`t create event handle");
	return event;
}

void WaitForFenceValue(
	ComPtr<ID3D12Fence> fence,
	UINT64 value, HANDLE event, 
	std::chrono::milliseconds time = std::chrono::milliseconds::max())
{
	if (fence->GetCompletedValue() < value) {
		ThrowIfFailed(fence->SetEventOnCompletion(value, event));
		::WaitForSingleObject(event, time.count());
	}
}

void Flush(
	ComPtr<ID3D12CommandQueue> commandQueue,
	ComPtr<ID3D12Fence> fence, 
	HANDLE event, UINT64 & fenceValue)
{
	UINT64 valueToWait = Signal(commandQueue, fence, fenceValue);
	WaitForFenceValue(fence, valueToWait, event);
}

void LoadDataToCB(ComPtr<ID3D12Resource> cb, const ObjectConstants& data, UINT size) {
	BYTE* pMappedData;

	ThrowIfFailed(cb->Map(0, NULL, reinterpret_cast<void**>(&pMappedData)));
	memcpy(pMappedData, &data, size);

	cb->Unmap(0, NULL);
}

void Update() {
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

	float angle = static_cast<float>(totalTime * 90.0);
	const XMVECTOR rotationAxis = XMVectorSet(0, 1, 1, 0);
	XMMATRIX modelMatrix = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(angle));

	const XMVECTOR eyePosition = XMVectorSet(0, 0, -10, 1);
	const XMVECTOR focusPoint = XMVectorSet(0, 0, 0, 1);
	const XMVECTOR upDirection = XMVectorSet(0, 1, 0, 0);
	XMMATRIX viewMatrix = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

	float aspectRatio = g_Width / static_cast<float>(g_Height);
	XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), aspectRatio, 0.1f, 100.0f);

	g_ObjectConstants.MVP = XMMatrixMultiply(modelMatrix, viewMatrix);
	g_ObjectConstants.MVP = XMMatrixMultiply(g_ObjectConstants.MVP, projectionMatrix);

	// To view components
	//XMFLOAT4X4 matrix;
	//XMStoreFloat4x4(&matrix, g_ObjectConstants.MVP);
	
	LoadDataToCB(g_ConstantBuffer, g_ObjectConstants, g_CBSize);
}

void Render() {
	auto commandAllocator = g_CommandAllocators[g_CurrentBackBuffer];
	auto backBuffer = g_BackBuffers[g_CurrentBackBuffer];

	ThrowIfFailed(commandAllocator->Reset());
	ThrowIfFailed(g_CommandList->Reset(commandAllocator.Get(), NULL));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(
		g_RTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
		g_CurrentBackBuffer, g_RTVDescSize
	);

	// Clear RTV
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.Get(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);

		g_CommandList->ResourceBarrier(1, &barrier);

		FLOAT backgroundColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
		g_CommandList->ClearRenderTargetView(rtv, backgroundColor, 0, NULL);

		g_CommandList->ClearDepthStencilView(
			g_DSVDescHeap->GetCPUDescriptorHandleForHeapStart(),
			D3D12_CLEAR_FLAG_DEPTH,
			1.0f, 0, 0, NULL 
		);
	}

	// Set root signature and its parameters
	g_CommandList->SetGraphicsRootSignature(g_RootSignature.Get());
	ID3D12DescriptorHeap* descriptorHeaps[] = { g_CBDescHeap.Get() };
	g_CommandList->SetDescriptorHeaps(1, descriptorHeaps);
	g_CommandList->SetGraphicsRootDescriptorTable(0, g_CBDescHeap->GetGPUDescriptorHandleForHeapStart());

	// Set PSO
	g_CommandList->SetPipelineState(g_PSO.Get());

	// Set Input Asembler Stage
	g_CommandList->IASetVertexBuffers(0, 1, &g_BoxGeo.VertexBufferView());
	g_CommandList->IASetIndexBuffer(&g_BoxGeo.IndexBufferView());
	g_CommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set Rasterizer Stage
	g_ScissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
	g_CommandList->RSSetScissorRects(1, &g_ScissorRect);
	g_ViewPort = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(g_Width), static_cast<float>(g_Height));
	g_CommandList->RSSetViewports(1, &g_ViewPort);

	// Set Output Mergere Stage
	g_CommandList->OMSetRenderTargets(1, &rtv, FALSE, &g_DSVDescHeap->GetCPUDescriptorHandleForHeapStart());

	// Draw vertexes by its indexes and primitive topology
	SubmeshGeometry submes = g_BoxGeo.DrawArgs["box"];
	g_CommandList->DrawIndexedInstanced(
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

		g_CommandList->ResourceBarrier(1, &barrier);
		ThrowIfFailed(g_CommandList->Close());

		ID3D12CommandList * const commandLists[] = {
			g_CommandList.Get()
		};

		g_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
		g_BuffersFenceValues[g_CurrentBackBuffer] = Signal(g_CommandQueue, g_Fence, g_FenceValue);

		UINT syncInterval = g_Vsync ? 1 : 0;
		UINT flags = g_AllowTearing && !g_Vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		ThrowIfFailed(g_SwapChain->Present(syncInterval, flags));

		g_CurrentBackBuffer = g_SwapChain->GetCurrentBackBufferIndex();
		WaitForFenceValue(g_Fence, g_BuffersFenceValues[g_CurrentBackBuffer], g_FenceEvent);
	}
}

void ResizeBackBuffers(uint32_t width, uint32_t height) {
	for (uint32_t i = 0; i < g_BufferCount; ++i) {
		g_BackBuffers[i].Reset();
		g_BuffersFenceValues[i] = g_BuffersFenceValues[g_CurrentBackBuffer];
	}

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	ThrowIfFailed(g_SwapChain->GetDesc(&swapChainDesc));

	ThrowIfFailed(g_SwapChain->ResizeBuffers(
		g_BufferCount,
		g_Width, g_Height,
		swapChainDesc.BufferDesc.Format,
		swapChainDesc.Flags
	));

	g_CurrentBackBuffer = g_SwapChain->GetCurrentBackBufferIndex();
	UpdateRTV(g_Device, g_SwapChain, g_RTVDescHeap, g_BackBuffers, g_BufferCount, g_RTVDescSize);
}

void ResizeDSBuffer(UINT width, UINT height) {
	g_DSBuffer.Reset();
	g_DSBuffer = CreateDepthStencilBuffer(g_Device, width, height);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};

	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	g_Device->CreateDepthStencilView(g_DSBuffer.Get(), &dsvDesc, g_DSVDescHeap->GetCPUDescriptorHandleForHeapStart());
}

void Resize(uint32_t width, uint32_t height) {
	if (g_Width != width || g_Height != height) {
		g_Width = std::max(1u, width);
		g_Height = std::max(1u, height);

		Flush(g_CommandQueue, g_Fence, g_FenceEvent, g_FenceValue);

		g_ViewPort = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(g_Width), static_cast<float>(g_Height));

		ResizeBackBuffers(width, height);
		ResizeDSBuffer(width, height);
	}
}

void FullScreen(bool fullScreen) {
	if (g_FullScreen != fullScreen) {
		g_FullScreen = fullScreen;

		if (g_FullScreen) {
			::GetWindowRect(g_windowHandle, &g_WindowRect);

			UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
			::SetWindowLongW(g_windowHandle, GWL_STYLE, windowStyle);

			HMONITOR hMonitor = ::MonitorFromWindow(g_windowHandle, MONITOR_DEFAULTTONEAREST);
			MONITORINFOEX monitorInfo = {};
			monitorInfo.cbSize = sizeof(MONITORINFOEX);
			::GetMonitorInfo(hMonitor, &monitorInfo);
			::SetWindowPos(
				g_windowHandle,
				HWND_TOP,
				monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.top,
				monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE
			);

			::ShowWindow(g_windowHandle, SW_MAXIMIZE);
		}
		else {
			::SetWindowLongW(g_windowHandle, GWL_STYLE, WS_OVERLAPPEDWINDOW);

			::SetWindowPos(
				g_windowHandle,
				HWND_NOTOPMOST,
				g_WindowRect.left,
				g_WindowRect.top,
				g_WindowRect.right - g_WindowRect.left,
				g_WindowRect.bottom - g_WindowRect.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE
			);

			::ShowWindow(g_windowHandle, SW_NORMAL);
		}
	}
}

ComPtr<ID3D12Resource> CreateBufferResource(
	ComPtr<ID3D12Device2> device,
	ComPtr<ID3D12GraphicsCommandList> commandList,
	ComPtr<ID3D12Resource>& intermediateResource,
	const void* pData, size_t numElements, size_t sizeElement
)
{
	ComPtr<ID3D12Resource> destinationResource;
	size_t dataSize = numElements * sizeElement;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(dataSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		NULL,
		IID_PPV_ARGS(&destinationResource)
	));

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(dataSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,
		IID_PPV_ARGS(&intermediateResource)
	));

	D3D12_SUBRESOURCE_DATA subresourceData;
	subresourceData.pData = pData;
	subresourceData.RowPitch = dataSize;
	subresourceData.SlicePitch = dataSize;

	UpdateSubresources(
		commandList.Get(),
		destinationResource.Get(),
		intermediateResource.Get(),
		0, 0, 1, &subresourceData
	);

	return destinationResource;
}

ComPtr<ID3DBlob> CompileShader(
	const std::wstring & filename, 
	const std::string & entrypoint, 
	const std::string & target)
{
	ComPtr<ID3DBlob> shaderBlob;
	ComPtr<ID3DBlob> error;
	HRESULT hr = S_OK;
	UINT flags = 0;
#ifdef _DEBUG
	flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif // _DEBUG

	hr = D3DCompileFromFile(
		filename.c_str(),
		NULL,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entrypoint.c_str(),
		target.c_str(),
		flags,
		0,
		&shaderBlob,
		&error
	);

	if (error != nullptr) {
		OutputDebugString((char*)error->GetBufferPointer());
		OutputDebugString("\n");
	}

	ThrowIfFailed(hr);

	return shaderBlob;
}

ComPtr<ID3D12Resource> CreateConstantBuffer(ComPtr<ID3D12Device2> device, UINT size) {
	ComPtr<ID3D12Resource> constantBuffer;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,
		IID_PPV_ARGS(&constantBuffer)
	));

	return constantBuffer;
}

void CreateConstantBufferView(
	ComPtr<ID3D12Device2> device,
	ComPtr<ID3D12Resource> cb, 
	UINT size, 
	ComPtr<ID3D12DescriptorHeap> cbDescHeap) 
{
	D3D12_CONSTANT_BUFFER_VIEW_DESC CBViewDesc;
	CBViewDesc.BufferLocation = cb->GetGPUVirtualAddress();
	CBViewDesc.SizeInBytes = size;

	device->CreateConstantBufferView(
		&CBViewDesc,
		cbDescHeap->GetCPUDescriptorHandleForHeapStart()
	);
}

ComPtr<ID3D12RootSignature> CreateRootSignature(ComPtr<ID3D12Device2> device) {
	ComPtr<ID3D12RootSignature> rootSignature;
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

	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rsVersion, sizeof(rsVersion)))) {
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

	ThrowIfFailed(g_Device->CreateRootSignature(
		0,
		rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature)
	));

	return rootSignature;
}

void BuildBoxGeometry() {
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

	g_BoxGeo.VertexBufferGPU = CreateBufferResource(
		g_Device,
		g_CommandList,
		g_BoxGeo.VertexBufferUploader,
		vertexes.data(),
		vertexes.size(),
		sizeof(VertexPosColor)
	);

	g_BoxGeo.IndexBufferGPU = CreateBufferResource(
		g_Device,
		g_CommandList,
		g_BoxGeo.IndexBufferUploader,
		indexes.data(),
		indexes.size(),
		sizeof(uint16_t)
	);

	g_BoxGeo.name = "BoxGeo";
	g_BoxGeo.VertexBufferByteSize = vbByteSize;
	g_BoxGeo.VertexByteStride = sizeof(VertexPosColor);
	g_BoxGeo.IndexBufferByteSize = ibByteSize;
	g_BoxGeo.IndexBufferFormat = DXGI_FORMAT_R16_UINT;

	SubmeshGeometry submesh;
	submesh.IndexCount = indexes.size();
	submesh.BaseVertexLocation = 0;
	submesh.StartIndexLocation = 0;

	g_BoxGeo.DrawArgs["box"] = submesh;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (g_IsInit) {
		switch (message)
		{
		case WM_PAINT:
			Update();
			Render();
			break;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		{
			bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

			switch (wParam)
			{
			case 'V':
				g_Vsync = !g_Vsync;
				break;
			case VK_ESCAPE:
				::PostQuitMessage(0);
				break;
			case VK_RETURN:
				if (alt)
				{
			case VK_F11:
				FullScreen(!g_FullScreen);
				}
				break;
			}
			break;
		}
		case WM_SYSCHAR:
			break;
		case WM_SIZE:
		{
			RECT clientRect;
			::GetClientRect(g_windowHandle, &clientRect);

			LONG width = clientRect.right - clientRect.left;
			LONG height = clientRect.bottom - clientRect.top;

			Resize(width, height);
			break;
		}
		case WM_DESTROY:
			::PostQuitMessage(0);
			break;
		default:
			return ::DefWindowProcW(hwnd, message, wParam, lParam);
		}
	} else {
		return ::DefWindowProcW(hwnd, message, wParam, lParam);
	}

}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdSho) {
	// allow DPI awareness
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// init and create window
	const wchar_t* className = L"MainWindowClass";
	RegisterWindowClass(hInstance, className);
	g_windowHandle = CreateWindow(hInstance, className, L"Empty window", g_Width, g_Height);
	::GetWindowRect(g_windowHandle, &g_WindowRect);

	// before any DirectX call enable debug
	EnableDebugLayer();

	g_AllowTearing = CheckTearingSupport();

#ifdef _DEBUG
	if (g_AllowTearing) {
		::OutputDebugStringW(L"Allow tearing true\n");
	}
	else {
		::OutputDebugStringW(L"Allow tearing false\n");
	}
#endif // _DEBUG

	// create DXGI and D3D objects
	// g_UseWarp = true;
	g_Adapter = CreateAdapter(g_UseWarp);
	g_Device = CreateDevice(g_Adapter);

	g_CommandQueue = CreateCommandQueue(g_Device);
	g_SwapChain = CreateSwapChain(g_CommandQueue, g_windowHandle, g_Width, g_Height, g_AllowTearing);
	g_CurrentBackBuffer = g_SwapChain->GetCurrentBackBufferIndex();

	for (uint32_t i = 0; i < g_BufferCount; ++i) {
		g_CommandAllocators[i] = CreateCommandAllocator(g_Device);
	}

	g_CommandList = CreateCommandList(g_Device, g_CommandAllocators[g_CurrentBackBuffer]);

	g_RTVDescHeap = CreateDescriptorHeap(g_Device, g_BufferCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
	g_RTVDescSize = g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	UpdateRTV(g_Device, g_SwapChain, g_RTVDescHeap, g_BackBuffers, g_BufferCount, g_RTVDescSize);

	g_DSVDescHeap = CreateDescriptorHeap(g_Device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
	ResizeDSBuffer(g_Width, g_Height);

	g_FenceValue = 0;
	g_Fence = CreateFence(g_Device, g_FenceValue);
	g_FenceEvent = CreateEventHandle();

	for (UINT i = 0; i < g_BufferCount; ++i) {
		g_BuffersFenceValues[i] = g_FenceValue;
	}

	// Create input layout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[]{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	D3D12_INPUT_LAYOUT_DESC inpuitLayout{ inputElementDescs, _countof(inputElementDescs) };

	// Create and load vertexes buffers
	g_CommandList->Reset(g_CommandAllocators[0].Get(), NULL);

	BuildBoxGeometry();

	// Compile shaders	
	g_VertexShaderBlob = CompileShader(L"..\\shaders\\VertexShader.hlsl", "main", "vs_5_1");
	g_PixelShaderBlob = CompileShader(L"..\\shaders\\PixelShader.hlsl", "main", "ps_5_1");

	// Create constant buffer
	ObjectConstants objConst;
	// align size to 256 bytes
	g_CBSize = (sizeof(ObjectConstants) + 255) & ~255;
	g_ConstantBuffer = CreateConstantBuffer(g_Device, g_CBSize);
	LoadDataToCB(g_ConstantBuffer, objConst, g_CBSize);

	// Create cb descritor heap and view
	g_CBDescHeap = CreateDescriptorHeap(g_Device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
	CreateConstantBufferView(g_Device, g_ConstantBuffer, g_CBSize, g_CBDescHeap);

	// Create root signature
	g_RootSignature = CreateRootSignature(g_Device);

	// Create rasterizer state description
	CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
	//rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;

	// Create pipeline state object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.pRootSignature = g_RootSignature.Get();
	psoDesc.VS = {
		reinterpret_cast<BYTE*>(g_VertexShaderBlob->GetBufferPointer()),
		g_VertexShaderBlob->GetBufferSize()
	};
	psoDesc.PS = {
	reinterpret_cast<BYTE*>(g_PixelShaderBlob->GetBufferPointer()),
	g_PixelShaderBlob->GetBufferSize()
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

	ThrowIfFailed(g_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_PSO)));

	// Wait while loading ends
	g_CommandList->Close();
	ID3D12CommandList* const commandLists[] = {
		g_CommandList.Get()
	};
	g_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
	Flush(g_CommandQueue, g_Fence, g_FenceEvent, g_FenceValue);

	g_BoxGeo.DisposeUploaders();

	// Initialization ends
	g_IsInit = true;

	::ShowWindow(g_windowHandle, SW_SHOW);

	MSG msg{};

	while (msg.message != WM_QUIT) {
		if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}

	Flush(g_CommandQueue, g_Fence, g_FenceEvent, g_FenceValue);
	::CloseHandle(g_FenceEvent);

	return 0;
}