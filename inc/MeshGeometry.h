#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d12.h>

#include <wrl.h>
using Microsoft::WRL::ComPtr;

#include <string>
#include <unordered_map>

struct SubmeshGeometry {
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;
};

struct MeshGeometry {
	std::string name;

	ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	UINT VertexByteStride = 0;
	UINT VertexBufferByteSize = 0;
	DXGI_FORMAT IndexBufferFormat = DXGI_FORMAT_R16_UINT;
	UINT IndexBufferByteSize = 0;

	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const {
		D3D12_VERTEX_BUFFER_VIEW vbv;

		vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		vbv.SizeInBytes = VertexBufferByteSize;
		vbv.StrideInBytes = VertexByteStride;

		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView() const {
		D3D12_INDEX_BUFFER_VIEW ibv;

		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.SizeInBytes = IndexBufferByteSize;
		ibv.Format = IndexBufferFormat;

		return ibv;
	}

	void DisposeUploaders() {
		VertexBufferUploader = nullptr;
		IndexBufferUploader = nullptr;
	}
};