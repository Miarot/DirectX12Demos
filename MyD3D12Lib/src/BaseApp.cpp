#include <MyD3D12Lib/BaseApp.h>
#include <MyD3D12Lib/D3D12Utils.h>

#include <windowsx.h>

#include <cassert>

#if defined(max)
#undef max
#endif

#if defined(min)
#undef min
#endif

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	return BaseApp::GetApp()->MsgProc(hwnd, message, wParam, lParam);
}

BaseApp::BaseApp(HINSTANCE hInstance) : m_hInstance(hInstance) {
	assert(m_App == nullptr);
	m_App = this;

	Initialize();
}

BaseApp::~BaseApp() {}

BaseApp* BaseApp::m_App = nullptr;
BaseApp* BaseApp::GetApp() {
	return m_App;
}

void BaseApp::Run() {
	::ShowWindow(m_WindowHandle, SW_SHOW);

	MSG msg{};

	while (msg.message != WM_QUIT) {
		if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}

	m_DirectCommandQueue->Flush();
	m_DirectCommandQueue->CloseHandle();
}

LRESULT BaseApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg)
	{
	case WM_PAINT:
		OnUpdate();
		OnRender();
		break;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	{
		bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

		switch (wParam)
		{
		case 'V':
			m_Vsync = !m_Vsync;
			break;
		case VK_ESCAPE:
			::PostQuitMessage(0);
			break;
		case VK_RETURN:
			if (alt)
			{
		case VK_F11:
			FullScreen(!m_FullScreen);
			}
			break;
		default:
			OnKeyPressed(wParam);
		}
		break;
	}
	case WM_SYSCHAR:
		break;
	case WM_SIZE:
	{
		OnResize();
		break;
	}
	case WM_DESTROY:
		::PostQuitMessage(0);
		break;
	case WM_MOUSEWHEEL:
		OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
		break;
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	default:
		return ::DefWindowProcW(hwnd, msg, wParam, lParam);
	}
	
	return 0;
}

bool BaseApp::Initialize() {
	// allow DPI awareness
	::SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// init and create window
	const wchar_t* className = L"MainWindowClass";
	RegisterWindowClass(className);
	m_WindowHandle = CreateWindow(className, L"Simple geometry");
	::GetWindowRect(m_WindowHandle, &m_WindowRect);

	// before any DirectX call enable debug
	EnableDebugLayer();

	// check if taring suported
	m_AllowTearing = CheckTearingSupport();

	// create D3D and DXGI objects
	m_Adapter = CreateAdapter();
	m_Device = CreateDevice(m_Adapter);

	m_DirectCommandQueue = std::make_shared<CommandQueue>(
		CommandQueue(m_Device, D3D12_COMMAND_LIST_TYPE_DIRECT)
	);

	// create swap chaine and init back buffers and it`s objects
	m_SwapChain = CreateSwapChain(
		m_DirectCommandQueue->GetCommandQueue(), 
		m_WindowHandle, 
		m_NumBackBuffers,
		m_ClientWidth, m_ClientHeight,
		m_BackBuffersFormat,
		m_AllowTearing
	);

	m_RTVDescHeap = CreateDescriptorHeap(
		m_Device,
		m_NumBackBuffers, 
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);

	m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

	for (UINT i = 0; i < m_NumBackBuffers; ++i) {
		m_BackBuffersFenceValues[i] = 0;
	}
	
	// init descriptors size
	m_RTVDescSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_CBV_SRV_UAVDescSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_DSVDescSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// create ds buffer and init it`s objects
	m_DSBuffer = CreateDepthStencilBuffer(
		m_Device, 
		m_ClientWidth, m_ClientHeight, 
		m_DepthSencilBufferFormat, m_DepthSencilViewFormat,
		m_DepthClearValue,
		m_SteniclClearValue
	);

	m_DSVDescHeap = CreateDescriptorHeap(
		m_Device,
		1,
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);

	UpdateBackBuffersView();
	UpdateDSView();

	return true;
}

void BaseApp::OnUpdate() {}
void BaseApp::OnRender() {}

void BaseApp::OnResize() {
	RECT clientRect;
	::GetClientRect(m_WindowHandle, &clientRect);

	LONG width = clientRect.right - clientRect.left;
	LONG height = clientRect.bottom - clientRect.top;

	if (m_ClientWidth != width || m_ClientHeight != height) {
		m_ClientWidth = std::max(1l, width);
		m_ClientHeight = std::max(1l, height);

		m_DirectCommandQueue->Flush();

		m_ViewPort = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_ClientWidth), static_cast<float>(m_ClientHeight));

		ResizeBackBuffers();
		ResizeDSBuffer();
	}
}

void BaseApp::OnKeyPressed(WPARAM wParam) {}
void BaseApp::OnMouseWheel(int wheelDelta) {}
void BaseApp::OnMouseDown(WPARAM wParam, int x, int y) {}
void BaseApp::OnMouseUp(WPARAM wParam, int x, int y) {}
void BaseApp::OnMouseMove(WPARAM wParam, int x, int y) {}

void BaseApp::RegisterWindowClass(const wchar_t* className) {
	WNDCLASSEXW windowDesc = {};

	windowDesc.cbSize = sizeof(WNDCLASSEXW);
	windowDesc.style = CS_HREDRAW | CS_VREDRAW;
	windowDesc.lpfnWndProc = &WndProc;
	windowDesc.cbClsExtra = 0;
	windowDesc.cbWndExtra = 0;
	windowDesc.hInstance = m_hInstance;
	windowDesc.hIcon = ::LoadIcon(m_hInstance, NULL);
	windowDesc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	windowDesc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	windowDesc.lpszMenuName = NULL;
	windowDesc.lpszClassName = className;
	windowDesc.hIconSm = ::LoadIcon(m_hInstance, NULL);

	ATOM atom = ::RegisterClassExW(&windowDesc);

	assert(atom > 0 && "Can`t register window class");
}

HWND BaseApp::CreateWindow(const wchar_t* className, const wchar_t* windowName) {
	RECT windowRect{ 0, 0, static_cast<LONG>(m_ClientWidth), static_cast<LONG>(m_ClientHeight) };
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
		m_hInstance,
		NULL
	);

	assert(windowHandle && "Cant`t create window");

	return windowHandle;
}

void BaseApp::FullScreen(bool fullScreen) {
	if (m_FullScreen != fullScreen) {
		m_FullScreen = fullScreen;

		if (m_FullScreen) {
			::GetWindowRect(m_WindowHandle, &m_WindowRect);

			UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
			::SetWindowLongW(m_WindowHandle, GWL_STYLE, windowStyle);

			HMONITOR hMonitor = ::MonitorFromWindow(m_WindowHandle, MONITOR_DEFAULTTONEAREST);
			MONITORINFOEX monitorInfo = {};
			monitorInfo.cbSize = sizeof(MONITORINFOEX);
			::GetMonitorInfo(hMonitor, &monitorInfo);
			::SetWindowPos(
				m_WindowHandle,
				HWND_TOP,
				monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.top,
				monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE
			);

			::ShowWindow(m_WindowHandle, SW_MAXIMIZE);
		}
		else {
			::SetWindowLongW(m_WindowHandle, GWL_STYLE, WS_OVERLAPPEDWINDOW);

			::SetWindowPos(
				m_WindowHandle,
				HWND_NOTOPMOST,
				m_WindowRect.left,
				m_WindowRect.top,
				m_WindowRect.right - m_WindowRect.left,
				m_WindowRect.bottom - m_WindowRect.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE
			);

			::ShowWindow(m_WindowHandle, SW_NORMAL);
		}
	}
}

void BaseApp::UpdateBackBuffersView()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandle(m_RTVDescHeap->GetCPUDescriptorHandleForHeapStart());
	ComPtr<ID3D12Resource> currentResource;

	for (uint32_t i = 0; i < m_NumBackBuffers; ++i) {
		ThrowIfFailed(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&currentResource)));
		m_BackBuffers[i] = currentResource;
		m_Device->CreateRenderTargetView(currentResource.Get(), nullptr, descriptorHandle);
		descriptorHandle.Offset(m_RTVDescSize);
	}
}

void BaseApp::UpdateDSView() {
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};

	dsvDesc.Format = m_DepthSencilViewFormat;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	m_Device->CreateDepthStencilView(m_DSBuffer.Get(), &dsvDesc, m_DSVDescHeap->GetCPUDescriptorHandleForHeapStart());
}

void BaseApp::ResizeDSBuffer() {
	m_DSBuffer.Reset();

	m_DSBuffer = CreateDepthStencilBuffer(
		m_Device,
		m_ClientWidth, m_ClientHeight,
		m_DepthSencilBufferFormat, m_DepthSencilViewFormat,
		m_DepthClearValue,
		m_SteniclClearValue
	);

	UpdateDSView();
}

void BaseApp::ResizeBackBuffers() {
	for (uint32_t i = 0; i < m_NumBackBuffers; ++i) {
		m_BackBuffers[i].Reset();
		m_BackBuffersFenceValues[i] = m_BackBuffersFenceValues[m_CurrentBackBufferIndex];
	}

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	ThrowIfFailed(m_SwapChain->GetDesc(&swapChainDesc));

	ThrowIfFailed(m_SwapChain->ResizeBuffers(
		m_NumBackBuffers,
		m_ClientWidth, m_ClientHeight,
		swapChainDesc.BufferDesc.Format,
		swapChainDesc.Flags
	));

	m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
	UpdateBackBuffersView();
}

