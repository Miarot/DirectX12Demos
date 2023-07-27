#include <FrameResources.h>

FrameResources::FrameResources(
	ComPtr<ID3D12Device> device, 
	size_t numPassConstants, 
	size_t numObjectConstants,
	size_t numMaterialConstants)
{
	m_PassConstantsBuffer = std::make_unique<UploadBuffer<PassConstants>>(device, numPassConstants, true);
	m_ObjectsConstantsBuffer = std::make_unique<UploadBuffer<ObjectConstants>>(device, numObjectConstants, true);
	m_MaterialsConstantsBuffer = std::make_unique <UploadBuffer<MaterialConstants>>(device, numMaterialConstants, true);
}

FrameResources::~FrameResources() {}