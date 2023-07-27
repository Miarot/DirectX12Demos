#pragma once

#include <d3d12.h>

#include <wrl.h>

#include <cstdint>

class ShadowMap {
public:
	ShadowMap(ID3D12Device2* device, uint32_t width, uint32_t height);

	ShadowMap(const ShadowMap& other) = delete;
	ShadowMap& operator=(const ShadowMap& other) = delete;

	~ShadowMap() = default;

	ID3D12Resource* GetResource() const;

	D3D12_CPU_DESCRIPTOR_HANDLE GetDsv() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrv() const;

	D3D12_VIEWPORT GetViewPort() const;
	D3D12_RECT GetScissorRect() const;

	void BuildDescriptors(
		D3D12_CPU_DESCRIPTOR_HANDLE cpuDsv,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuSrv,
		D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv
	);

	void OnResize(uint32_t width, uint32_t height);
private:
	void BuildResource();
	void BuildDescriptors();

private:
	ID3D12Device2* m_Device;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_Resource;

	D3D12_CPU_DESCRIPTOR_HANDLE m_CpuDsv;
	D3D12_CPU_DESCRIPTOR_HANDLE m_CpuSrv;
	D3D12_GPU_DESCRIPTOR_HANDLE m_GpuSrv;

	uint32_t m_Width;
	uint32_t m_Height;
	DXGI_FORMAT m_BufferFormat = DXGI_FORMAT_R24G8_TYPELESS;
	DXGI_FORMAT m_DSVformat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DXGI_FORMAT m_SRVformat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

	D3D12_VIEWPORT m_ViewPort;
	D3D12_RECT m_ScissorRect;
};