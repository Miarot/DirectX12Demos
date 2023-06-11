#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#if defined(CreateWindow)
#undef CreateWindow
#endif // CreateWindow

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

#include <helpers.h>

// window parameters
HWND g_windowHandl;
uint32_t g_Width = 1080;
uint32_t g_Height = 720;

// render parameters
bool g_UseWarp = false;
UINT g_BufferCount = 3;
bool g_AllowTearing = false;

// DirectX objects
ComPtr<IDXGIAdapter4> g_Adapter;
ComPtr<ID3D12Device2> g_Device;
ComPtr<ID3D12CommandQueue> g_CommandQueue;
ComPtr<IDXGISwapChain4> g_SwapChain;

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

	HWND windowHandl = ::CreateWindowExW(
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

	assert(windowHandl && "Cant`t create window");

	return windowHandl;
}

void EnableDebugLayer() {
#ifdef _DEBUG
	ComPtr<ID3D12Debug> debugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();
#endif // DEBUG
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
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter4)));
	} else {
		UINT i = 0;
		SIZE_T maxDedicatedMemory = 0;

		while (factory->EnumAdapters1(i, adapter1.GetAddressOf()) != DXGI_ERROR_NOT_FOUND) {
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

	ThrowIfFailed(swapChain1.As(&swapChain4));

	return swapChain4;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message)
	{
	case WM_DESTROY:
		::PostQuitMessage(0);
		break;
	default:
		return ::DefWindowProcW(hwnd, message, wParam, lParam);
	}
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdSho) {
	// allow DPI awareness
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// init and create window
	const wchar_t * className = L"MainWindowClass";
	RegisterWindowClass(hInstance, className);
	g_windowHandl = CreateWindow(hInstance, className, L"Empty window", g_Width, g_Height);

	// before any DirectX call enable debug
	EnableDebugLayer();

	// create DXGI and D3D objects
	// g_UseWarp = true;
	g_Adapter = CreateAdapter(g_UseWarp);
	g_Device = CreateDevice(g_Adapter);
	g_CommandQueue = CreateCommandQueue(g_Device);
	g_SwapChain = CreateSwapChain(g_CommandQueue, g_windowHandl, g_Width, g_Height, g_AllowTearing);

	::ShowWindow(g_windowHandl, SW_SHOW);

	MSG msg{};

	while (msg.message != WM_QUIT) {
		if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}

	return 0;
}