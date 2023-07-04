#include <MyD3D12Lib/MeshGeometry.h>

D3D12_INDEX_BUFFER_VIEW MeshGeometry::IndexBufferView() const {
	D3D12_INDEX_BUFFER_VIEW ibv;

	ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
	ibv.SizeInBytes = IndexBufferByteSize;
	ibv.Format = IndexBufferFormat;

	return ibv;
}


D3D12_VERTEX_BUFFER_VIEW MeshGeometry::VertexBufferView() const {
	D3D12_VERTEX_BUFFER_VIEW vbv;

	vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
	vbv.SizeInBytes = VertexBufferByteSize;
	vbv.StrideInBytes = VertexByteStride;

	return vbv;
}

void MeshGeometry::DisposeUploaders() {
	VertexBufferUploader = nullptr;
	IndexBufferUploader = nullptr;
}