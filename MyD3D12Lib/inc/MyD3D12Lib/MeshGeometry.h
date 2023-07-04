#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d12.h>

#include <wrl.h>
using Microsoft::WRL::ComPtr;

#include <string>
#include <unordered_map>

struct SubmeshGeometry {
	uint32_t IndexCount = 0;
	uint32_t StartIndexLocation = 0;
	uint32_t BaseVertexLocation = 0;
};

struct MeshGeometry {
	std::string name;

	ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	uint32_t VertexByteStride = 0;
	uint32_t VertexBufferByteSize = 0;
	DXGI_FORMAT IndexBufferFormat = DXGI_FORMAT_R16_UINT;
	uint32_t IndexBufferByteSize = 0;

	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const;
	
	D3D12_INDEX_BUFFER_VIEW IndexBufferView() const;

	void DisposeUploaders();
};