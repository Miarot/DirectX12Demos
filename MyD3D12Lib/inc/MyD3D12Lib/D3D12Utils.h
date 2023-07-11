#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>

#include <wrl.h>
using Microsoft::WRL::ComPtr;

#include <string>

// compile shader from file
ComPtr<ID3DBlob> CompileShader(
	const std::wstring& filename,
	const std::string& entrypoint,
	const std::string& target,
	const D3D_SHADER_MACRO* defines = NULL
);

// texture loading
void CreateDDSTextureFromFile(
	ComPtr<ID3D12Device2> device,
	ComPtr<ID3D12GraphicsCommandList> commandList,
	std::wstring fileName,
	ComPtr<ID3D12Resource> & resource,
	ComPtr<ID3D12Resource> & uploadResource
);

void CreateWICTextureFromFile(
	ComPtr<ID3D12Device2> device,
	ComPtr<ID3D12GraphicsCommandList> commandList,
	std::wstring fileName,
	ComPtr<ID3D12Resource>& resource,
	ComPtr<ID3D12Resource>& uploadResource
);

// compute projection matrix
DirectX::XMMATRIX GetProjectionMatrix(
	bool isInverseDepht,
	float fov, float aspectRatio,
	float nearPlain = 0.1f,
	float farPlain = 100.0f
);

// DirectX 12 initialization functions
void EnableDebugLayer();
bool CheckTearingSupport();
D3D_ROOT_SIGNATURE_VERSION GetRootSignatureVersion(ComPtr<ID3D12Device2> device);

ComPtr<IDXGIAdapter4> CreateAdapter(bool useWarp = false);
ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter);

ComPtr<IDXGISwapChain4> CreateSwapChain(
	ComPtr<ID3D12CommandQueue> commandQueue, 
	HWND windowHandle,
	uint32_t numBackBuffers,
	uint32_t width, uint32_t height,
	DXGI_FORMAT format,
	bool allowTearing
);

ComPtr<ID3D12Resource> CreateDepthStencilBuffer(
	ComPtr<ID3D12Device2> device,
	uint32_t width, uint32_t height,
	DXGI_FORMAT bufferFormat, DXGI_FORMAT viewFormat,
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