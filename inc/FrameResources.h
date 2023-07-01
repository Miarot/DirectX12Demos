#pragma once

#include <memory>

#include <SimpleGeoApp.h>
#include <UploadBuffer.h>

class SimpleGeoApp::FrameResources {
public:
	FrameResources(
		ComPtr<ID3D12Device> device, 
		UINT numPassConstants, 
		UINT numObjectConstants,
		UINT numMaterialConstants
	);

	FrameResources(const FrameResources& other) = delete;
	FrameResources& operator=(const FrameResources& other) = delete;

	~FrameResources();

	std::unique_ptr<UploadBuffer<PassConstants>> m_PassConstantsBuffer;
	std::unique_ptr<UploadBuffer<ObjectConstants>> m_ObjectsConstantsBuffer;
	std::unique_ptr<UploadBuffer<MaterialConstants>> m_MaterialsConstantsBuffer;
};