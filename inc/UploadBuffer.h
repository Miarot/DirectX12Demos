#pragma once

#include <d3d12.h>

#include <wrl.h>
using Microsoft::WRL::ComPtr;

template<class T>
class UploadBuffer {
public:
	UploadBuffer(ComPtr<ID3D12Device> device, UINT numElements, bool isConstantBuffer) :
		m_IsConstantBuffer(isConstantBuffer)
	{
		if (m_IsConstantBuffer) {
			m_ElementByteSize = (sizeof(T) + 255) & ~255;
		} else { 
			m_ElementByteSize = sizeof(T);
		}

		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(m_ElementByteSize * numElements),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			NULL,
			IID_PPV_ARGS(&m_Resource)
		));

		ThrowIfFailed(m_Resource->Map(0, NULL, reinterpret_cast<void**>(&m_MappedData)));
	}

	UploadBuffer(const UploadBuffer& other) = delete;
	UploadBuffer & operator=(const UploadBuffer& other) = delete;

	~UploadBuffer() {
		if (m_MappedData != nullptr) {
			m_Resource->Unmap(0, NULL);
		}

		m_MappedData = nullptr;
	}

	ID3D12Resource* Get() {
		return m_Resource.Get();
	}

	void CopyData(UINT elementIndex, const T& data) {
		memcpy(m_MappedData + elementIndex * m_ElementByteSize, &data, sizeof(T));
	}

	UINT GetElementByteSize() const {
		return m_ElementByteSize;
	}

private:
	bool m_IsConstantBuffer;
	UINT m_ElementByteSize;
	ComPtr<ID3D12Resource> m_Resource;
	BYTE* m_MappedData;
};