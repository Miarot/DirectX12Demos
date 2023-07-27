#pragma once

#include <AppStructures.h>
#include <MyD3D12Lib/UploadBuffer.h>

#include <memory>

class FrameResources {
public:
	FrameResources(
		ComPtr<ID3D12Device> device, 
		size_t numPassConstants, 
		size_t numObjectConstants,
		size_t numMaterialConstants
	);

	FrameResources(const FrameResources& other) = delete;
	FrameResources& operator=(const FrameResources& other) = delete;

	~FrameResources();

	std::unique_ptr<UploadBuffer<PassConstants>> m_PassConstantsBuffer;
	std::unique_ptr<UploadBuffer<ObjectConstants>> m_ObjectsConstantsBuffer;
	std::unique_ptr<UploadBuffer<MaterialConstants>> m_MaterialsConstantsBuffer;
};