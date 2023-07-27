#include <ShadowMap.h>
#include <MyD3D12Lib/Helpers.h>
#include <MyD3D12Lib/D3D12Utils.h>

#include <d3dx12.h>

ShadowMap::ShadowMap(ID3D12Device2* device, uint32_t width, uint32_t height) :
	m_Device(device), m_Width(width), m_Height(height)
{
	m_ViewPort = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height));
	m_ScissorRect = CD3DX12_RECT(0, 0, m_Width, m_Height);

	BuildResource();
}

void ShadowMap::BuildResource() {
	m_Resource = CreateDepthStencilBuffer(
		m_Device, 
		m_Width, m_Height, 
		m_BufferFormat, m_DSVformat, 
		1.0f, 0,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);
}

void ShadowMap::BuildDescriptors(
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDsv,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuSrv,
	D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv) 
{
	m_CpuDsv = cpuDsv;
	m_CpuSrv = cpuSrv;
	m_GpuSrv = gpuSrv;

	BuildDescriptors();
}

void ShadowMap::BuildDescriptors() {
	D3D12_DEPTH_STENCIL_VIEW_DESC dsViewDesc{};

	dsViewDesc.Format = m_DSVformat;
	dsViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsViewDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsViewDesc.Texture2D.MipSlice = 0;

	m_Device->CreateDepthStencilView(m_Resource.Get(), &dsViewDesc, m_CpuDsv);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvViewDesc{};
	
	srvViewDesc.Format = m_SRVformat;
	srvViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvViewDesc.Texture2D.MostDetailedMip = 0;
	srvViewDesc.Texture2D.MipLevels = 1;
	srvViewDesc.Texture2D.PlaneSlice = 0;
	srvViewDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	m_Device->CreateShaderResourceView(m_Resource.Get(), &srvViewDesc, m_CpuSrv);
}

ID3D12Resource* ShadowMap::GetResource() const {
	return m_Resource.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE ShadowMap::GetDsv() const {
	return m_CpuDsv;
}

D3D12_GPU_DESCRIPTOR_HANDLE ShadowMap::GetSrv() const {
	return m_GpuSrv;
}

D3D12_VIEWPORT ShadowMap::GetViewPort() const {
	return m_ViewPort;
}

D3D12_RECT ShadowMap::GetScissorRect() const {
	return m_ScissorRect;
}

void ShadowMap::OnResize(uint32_t width, uint32_t height) {
	if (m_Width != width || m_Height != height) {
		m_Width = width;
		m_Height = height;

		m_ViewPort = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height));
		m_ScissorRect = CD3DX12_RECT(0, 0, m_Width, m_Height);

		BuildResource();
		BuildDescriptors();
	}
}