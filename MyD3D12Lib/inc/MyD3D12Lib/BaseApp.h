#pragma once

#include <MyD3D12Lib/CommandQueue.h>

#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>

#include<wrl.h>
using Microsoft::WRL::ComPtr;

#include <memory>
#include <cstdint>

#if defined(CreateWindow)
#undef CreateWindow
#endif

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

constexpr int32_t m_NumBackBuffers = 3;

class BaseApp {
protected:
	explicit BaseApp(HINSTANCE hInstance);

	BaseApp(const BaseApp& other) = delete;
	BaseApp& operator= (const BaseApp& other) = delete;

	virtual ~BaseApp();

public:
	static BaseApp* GetApp();

	void Run();

	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	virtual bool Initialize();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnResize();
	virtual void OnKeyPressed(WPARAM wParam);
	virtual void OnMouseWheel(int wheelDelta);
	virtual void OnMouseDown(WPARAM wParam, int x, int y);
	virtual void OnMouseUp(WPARAM wParam, int x, int y);
	virtual void OnMouseMove(WPARAM wParam, int x, int y);

	void RegisterWindowClass(const wchar_t* className);
	HWND CreateWindow(const wchar_t* className, const wchar_t* windowName);
	void FullScreen(bool fullScreen);

	void UpdateBackBuffersView();
	void UpdateDSView();

	void ResizeBackBuffers();
	void ResizeDSBuffer();

protected:
	static BaseApp * m_App;

	HINSTANCE m_hInstance = NULL;
	HWND m_WindowHandle = NULL;

	bool m_AllowTearing = false;
	bool m_UseWarp = false;
	bool m_Vsync = true;
	bool m_FullScreen = false;
	uint32_t m_ClientWidth = 1280;
	uint32_t m_ClientHeight = 720;
	DXGI_FORMAT m_DepthSencilBufferFormat = DXGI_FORMAT_R32_TYPELESS;
	DXGI_FORMAT m_DepthSencilViewFormat = DXGI_FORMAT_D32_FLOAT;
	float m_DepthClearValue = 1.0f;
	uint8_t m_SteniclClearValue = 0;

	ComPtr<IDXGIAdapter4> m_Adapter;
	ComPtr<ID3D12Device2> m_Device;
	std::shared_ptr<CommandQueue> m_DirectCommandQueue;
	ComPtr<IDXGISwapChain4> m_SwapChain;
	ComPtr<ID3D12Resource> m_BackBuffers[m_NumBackBuffers];
	uint32_t m_BackBuffersFenceValues[m_NumBackBuffers];
	uint32_t m_CurrentBackBufferIndex;
	ComPtr<ID3D12DescriptorHeap> m_RTVDescHeap;
	uint32_t m_RTVDescSize;
	uint32_t m_CBV_SRV_UAVDescSize;
	ComPtr<ID3D12Resource> m_DSBuffer;
	ComPtr<ID3D12DescriptorHeap> m_DSVDescHeap;

	RECT m_WindowRect;
	D3D12_RECT m_ScissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
	D3D12_VIEWPORT m_ViewPort = CD3DX12_VIEWPORT(
		0.0f, 0.0f, static_cast<float>(m_ClientWidth), static_cast<float>(m_ClientHeight)
	);
};