#pragma once

#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>

#include<wrl.h>
using Microsoft::WRL::ComPtr;

#include <memory>
#include <cstdint>

#include <CommandQueue.h>

#if defined(CreateWindow)
#undef CreateWindow
#endif

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

class BaseApp {
protected:
	explicit BaseApp(HINSTANCE hInstance);

	BaseApp(const BaseApp& other) = delete;
	BaseApp& operator= (const BaseApp& other) = delete;

	virtual ~BaseApp();

public:
	static BaseApp* GetApp();

	void Run();

	virtual bool Initialize();
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	void RegisterWindowClass(const wchar_t* className);
	HWND CreateWindow(const wchar_t* className, const wchar_t* windowName);

	void EnableDebugLayer();
	bool CheckTearingSupport();

	ComPtr<IDXGIAdapter4> CreateAdapter();
	ComPtr<ID3D12Device2> CreateDevice();
	ComPtr<IDXGISwapChain4> CreateSwapChain();
	ComPtr<ID3D12Resource> CreateDepthStencilBuffer();
	ComPtr<ID3D12Resource> CreateConstantBuffer(uint32_t bufferSize);

	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
		UINT numDescriptors,
		D3D12_DESCRIPTOR_HEAP_TYPE type,
		D3D12_DESCRIPTOR_HEAP_FLAGS flags
	);

	ComPtr<ID3D12Resource> CreateGPUResourceAndLoadData(
		ComPtr<ID3D12GraphicsCommandList> commandList,
		ComPtr<ID3D12Resource>& intermediateResource,
		const void* pData,
		size_t dataSize
	);

	ComPtr<ID3DBlob> CompileShader(
		const std::wstring& filename,
		const std::string& entrypoint,
		const std::string& target
	);

	void UpdateBackBuffersView();
	void UpdateDSView();

	void UpdateCBViews(
		ComPtr<ID3D12Resource> constantBuffer, 
		uint32_t bufferSize,
		uint32_t numBuffers,
		ComPtr<ID3D12DescriptorHeap> constantBufferDescHeap
	);

	template<class T>
	void LoadDataToCB(ComPtr<ID3D12Resource> constantBuffer, uint32_t bufferIndex, const T& data, size_t dataSize);

	void ResizeBackBuffers();
	void ResizeDSBuffer();

	void FullScreen(bool fullScreen);

	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnResize();
	virtual void OnKeyPressed(WPARAM wParam);
	virtual void OnMouseWheel(int wheelDelta);
	virtual void OnMouseDown(WPARAM wParam, int x, int y);
	virtual void OnMouseUp(WPARAM wParam, int x, int y);
	virtual void OnMouseMove(WPARAM wParam, int x, int y);


protected:
	static BaseApp * m_App;
	HINSTANCE m_hInstance = NULL;
	HWND m_WindowHandle = NULL;

	static const uint32_t m_NumBackBuffers = 3;
	bool m_AllowTearing = false;
	bool m_UseWarp = false;
	bool m_Vsync = true;
	bool m_FullScreen = false;
	uint32_t m_ClientWidth = 1280;
	uint32_t m_ClientHeight = 720;
	float m_DepthClearValue = 1.0f;

	ComPtr<IDXGIAdapter4> m_Adapter;
	ComPtr<ID3D12Device2> m_Device;
	ComPtr<IDXGISwapChain4> m_SwapChain;
	ComPtr<ID3D12Resource> m_BackBuffers[m_NumBackBuffers];
	uint32_t m_BackBuffersFenceValues[m_NumBackBuffers];
	uint32_t m_CurrentBackBufferIndex;
	ComPtr<ID3D12DescriptorHeap> m_BackBuffersDescHeap;
	uint32_t m_RTVDescSize;
	uint32_t m_CBDescSize;
	ComPtr<ID3D12DescriptorHeap> m_DSVDescHeap;
	ComPtr<ID3D12Resource> m_DSBuffer;

	RECT m_WindowRect;
	D3D12_RECT m_ScissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
	D3D12_VIEWPORT m_ViewPort = CD3DX12_VIEWPORT(
		0.0f, 0.0f, static_cast<float>(m_ClientWidth), static_cast<float>(m_ClientHeight)
	);

	std::shared_ptr<CommandQueue> m_DirectCommandQueue;
};

template<class T>
void BaseApp::LoadDataToCB(
	ComPtr<ID3D12Resource> constantBuffer, 
	uint32_t bufferIndex,
	const T& data, 
	size_t dataSize) 
{
	BYTE* pMappedData;

	ThrowIfFailed(constantBuffer->Map(0, NULL, reinterpret_cast<void**>(&pMappedData)));
	memcpy(pMappedData + bufferIndex * dataSize, &data, dataSize);

	constantBuffer->Unmap(0, NULL);
}