#include <windowsx.h>

#include <d3dx12.h>
#include <d3dcompiler.h>

#include <cassert>

#include <MyD3D12Lib/BaseApp.h>

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
	m_Device = CreateDevice();

	m_DirectCommandQueue = std::make_shared<CommandQueue>(
		CommandQueue(m_Device, D3D12_COMMAND_LIST_TYPE_DIRECT)
	);

		// create swap chaine and init back buffers and it`s objects
	m_SwapChain = CreateSwapChain();

	m_BackBuffersDescHeap = CreateDescriptorHeap(
		m_NumBackBuffers, 
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);

	m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

	for (UINT i = 0; i < m_NumBackBuffers; ++i) {
		m_BackBuffersFenceValues[i] = 0;
	}

	m_RTVDescSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_CBV_SRV_UAVDescSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	
	UpdateBackBuffersView();

		// create ds buffer and init it`s objects
	m_DSBuffer = CreateDepthStencilBuffer();

	m_DSVDescHeap = CreateDescriptorHeap(
		1,
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	);

	UpdateDSView();

	return true;
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
}

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

void BaseApp::EnableDebugLayer() {
#ifdef _DEBUG
	ComPtr<ID3D12Debug> debugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();
#endif // DEBUG
}

bool BaseApp::CheckTearingSupport() {
	BOOL allowTearing = FALSE;
	ComPtr<IDXGIFactory4> factory4;
	ComPtr<IDXGIFactory5> factory5;

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory4)));
	ThrowIfFailed(factory4.As(&factory5));

	ThrowIfFailed(factory5->CheckFeatureSupport(
		DXGI_FEATURE_PRESENT_ALLOW_TEARING,
		&allowTearing, sizeof(allowTearing)
	));

#ifdef _DEBUG
	if (allowTearing == TRUE) {
		::OutputDebugStringW(L"Allow tearing true\n");
	}
	else {
		::OutputDebugStringW(L"Allow tearing false\n");
	}
#endif // _DEBUG

	return allowTearing == TRUE;
}

ComPtr<IDXGIAdapter4> BaseApp::CreateAdapter() {
	ComPtr<IDXGIFactory4> factory;
	UINT factorFlags = 0;
#ifdef _DEBUG
	factorFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif // DEBUG

	ThrowIfFailed(CreateDXGIFactory2(factorFlags, IID_PPV_ARGS(&factory)));

	ComPtr<IDXGIAdapter1> adapter1;
	ComPtr<IDXGIAdapter4> adapter4;

	if (m_UseWarp) {
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter1)));
	}
	else {
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

ComPtr<ID3D12Device2> BaseApp::CreateDevice() {
	ComPtr<ID3D12Device2> device;
	ThrowIfFailed(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

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

ComPtr<IDXGISwapChain4> BaseApp::CreateSwapChain() {
	ComPtr<IDXGISwapChain1> swapChain1;
	ComPtr<IDXGISwapChain4> swapChain4;

	ComPtr<IDXGIFactory4> factory;
	UINT factorFlags = 0;
#ifdef _DEBUG
	factorFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif // DEBUG
	ThrowIfFailed(CreateDXGIFactory2(factorFlags, IID_PPV_ARGS(&factory)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc;

	swapChainDesc.Width = m_ClientWidth;
	swapChainDesc.Height = m_ClientHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = DXGI_SAMPLE_DESC{ 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = m_NumBackBuffers;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = m_AllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		m_DirectCommandQueue->GetCommandQueue().Get(),
		m_WindowHandle,
		&swapChainDesc,
		NULL,
		NULL,
		&swapChain1
	));

	ThrowIfFailed(factory->MakeWindowAssociation(m_WindowHandle, DXGI_MWA_NO_ALT_ENTER));
	ThrowIfFailed(swapChain1.As(&swapChain4));

	return swapChain4;
}

ComPtr<ID3D12Resource> BaseApp::CreateDepthStencilBuffer() {
	ComPtr<ID3D12Resource> depthStencilBuffer;

	D3D12_RESOURCE_DESC dsBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_D32_FLOAT,
		m_ClientWidth, m_ClientHeight,
		1, 0, 1, 0,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		D3D12_TEXTURE_LAYOUT_UNKNOWN,
		0
	);

	D3D12_CLEAR_VALUE dsClearValue{};
	dsClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	dsClearValue.DepthStencil = { m_DepthClearValue, 0 };

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&dsBufferDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&dsClearValue,
		IID_PPV_ARGS(&depthStencilBuffer)
	));

	return depthStencilBuffer;
}

ComPtr<ID3D12DescriptorHeap> BaseApp::CreateDescriptorHeap(
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

	ThrowIfFailed(m_Device->CreateDescriptorHeap(&descriptroHeapDesc, IID_PPV_ARGS(&descriptorHeap)));

	return descriptorHeap;
}

ComPtr<ID3D12Resource> BaseApp::CreateGPUResourceAndLoadData(
	ComPtr<ID3D12GraphicsCommandList> commandList,
	ComPtr<ID3D12Resource>& intermediateResource,
	const void* pData,
	size_t dataSize)
{
	ComPtr<ID3D12Resource> destinationResource;

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(dataSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		NULL,
		IID_PPV_ARGS(&destinationResource)
	));

	ThrowIfFailed(m_Device->CreateCommittedResource(
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

ComPtr<ID3DBlob> BaseApp::CompileShader(
	const std::wstring& filename,
	const std::string& entrypoint,
	const std::string& target)
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

void BaseApp::UpdateBackBuffersView()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandle(m_BackBuffersDescHeap->GetCPUDescriptorHandleForHeapStart());
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

	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	m_Device->CreateDepthStencilView(m_DSBuffer.Get(), &dsvDesc, m_DSVDescHeap->GetCPUDescriptorHandleForHeapStart());
}

void BaseApp::ResizeDSBuffer() {
	m_DSBuffer.Reset();
	m_DSBuffer = CreateDepthStencilBuffer();
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