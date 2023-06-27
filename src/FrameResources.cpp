#include <FrameResources.h>

SimpleGeoApp::FrameResources::FrameResources(
	ComPtr<ID3D12Device> device, 
	UINT numPassConstants, 
	UINT numObjectConstants) 
{
	m_PassConstantsBuffer = std::make_unique<UploadBuffer<PassConstants>>(device, numPassConstants, true);
	m_ObjectsConstantsBuffer = std::make_unique<UploadBuffer<ObjectConstants>>(device, numObjectConstants, true);
}

SimpleGeoApp::FrameResources::~FrameResources() {}