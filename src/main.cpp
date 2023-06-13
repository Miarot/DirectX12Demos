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

#include <wrl.h>
using Microsoft::WRL::ComPtr;

#include <cassert>
#include <algorithm>
#include <chrono>

#include <helpers.h>

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

#ifdef _DEBUG
	DXGI_ADAPTER_DESC1 adapterDesc;
	adapter4->GetDesc1(&adapterDesc);
	OutputDebugStringW(adapterDesc.Description);
#endif // DEBUG
	
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
	swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
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

void Update() {
	static double elapsedTime = 0.0;
	static uint64_t frameCounter = 0;
	static std::chrono::high_resolution_clock clock;
	static auto prevTime = clock.now();
	
	auto currentTime = clock.now();
	auto deltaTime = currentTime - prevTime;
	prevTime = currentTime;
	elapsedTime += deltaTime.count() * 1e-9;
	++frameCounter;

	if (elapsedTime >= 1.0) {
		char buffer[500];
		auto fps = frameCounter / elapsedTime;
		::sprintf_s(buffer, 500, "FPS: %f\n", fps);
		::OutputDebugString(buffer);

		frameCounter = 0;
		elapsedTime = 0.0;
	}
}

void Render() {
	auto commandAllocator = g_CommandAllocators[g_CurrentBackBuffer];
	auto backBuffer = g_BackBuffers[g_CurrentBackBuffer];

	ThrowIfFailed(commandAllocator->Reset());
	ThrowIfFailed(g_CommandList->Reset(commandAllocator.Get(), NULL));

	// Clear RTV
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.Get(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);

		g_CommandList->ResourceBarrier(1, &barrier);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(
			g_RTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
			g_CurrentBackBuffer, g_RTVDescSize
		);

		FLOAT backgroundColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
		g_CommandList->ClearRenderTargetView(rtv, backgroundColor, 0, NULL);
	}

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

void Resize(uint32_t width, uint32_t height) {
	if (g_Width != width || g_Height != height) {
		g_Width = std::max(1u, width);
		g_Height = std::max(1u, height);

		Flush(g_CommandQueue, g_Fence, g_FenceEvent, g_FenceValue);

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
		} else {
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

ComPtr<ID3D12Resource> CreateDepthStencilBuffer() {
	ComPtr<ID3D12Resource> depthStencilBuffer;

	return depthStencilBuffer;
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
	const wchar_t * className = L"MainWindowClass";
	RegisterWindowClass(hInstance, className);
	g_windowHandle = CreateWindow(hInstance, className, L"Empty window", g_Width, g_Height);
	::GetWindowRect(g_windowHandle, &g_WindowRect);

	// before any DirectX call enable debug
	EnableDebugLayer();
	g_AllowTearing = CheckTearingSupport();

#ifdef _DEBUG
	if (g_AllowTearing) {
		::OutputDebugString("Allow tearing true");
	} else {
		::OutputDebugString("Allow tearing false");
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
	g_FenceValue = 0;
	g_Fence = CreateFence(g_Device, g_FenceValue);
	g_FenceEvent = CreateEventHandle();

	for (UINT i = 0; i < g_BufferCount; ++i) {
		g_BuffersFenceValues[i] = g_FenceValue;
	}

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