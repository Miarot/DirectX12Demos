#pragma once

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <d3dx12.h>

#include <wrl.h>
using Microsoft::WRL::ComPtr;

#include <string>

// compile shader from file
ComPtr<ID3DBlob> CompileShader(
	const std::wstring& filename,
	const std::string& entrypoint,
	const std::string& target
);

// DirectX 12 initialization functions
void EnableDebugLayer();
bool CheckTearingSupport();
ComPtr<IDXGIAdapter4> CreateAdapter(bool useWarp = false);
ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter);

ComPtr<IDXGISwapChain4> CreateSwapChain(
	ComPtr<ID3D12CommandQueue> commandQueue, 
	HWND windowHandle,
	uint32_t numBackBuffers,
	uint32_t width, uint32_t height,
	bool allowTearing
);

ComPtr<ID3D12Resource> CreateDepthStencilBuffer(
	ComPtr<ID3D12Device2> device,
	uint32_t width, uint32_t height,
	DXGI_FORMAT format,
	float depthClearValue,
	uint8_t stencilClearValue
);

ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
	ComPtr<ID3D12Device2> device,
	UINT numDescriptors,
	D3D12_DESCRIPTOR_HEAP_TYPE type,
	D3D12_DESCRIPTOR_HEAP_FLAGS flags
);

ComPtr<ID3D12Resource> CreateGPUResourceAndLoadData(
	ComPtr<ID3D12Device2> device,
	ComPtr<ID3D12GraphicsCommandList> commandList,
	ComPtr<ID3D12Resource>& intermediateResource,
	const void* pData,
	size_t dataSize
);