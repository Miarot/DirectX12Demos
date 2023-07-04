#pragma once

#include <d3d12.h>

#include <wrl.h>
using Microsoft::WRL::ComPtr;

#include <queue>

#include <MyD3D12Lib/Helpers.h>

class CommandQueue {
public:
	CommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);

	~CommandQueue();

	ComPtr<ID3D12GraphicsCommandList> GetCommandList();

	ComPtr<ID3D12CommandQueue> GetCommandQueue() const;

	uint64_t ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList> commandList);

	bool IsFenceComplite(uint64_t fenceValue) const;

	uint64_t Signal();

	void WaitForFenceValue(uint64_t fenceValue);

	void Flush();
	
	void CloseHandle();

private:
	ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();

	ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12CommandAllocator> commandAllocator);

	struct CommandAllocatorEntry {
		ComPtr<ID3D12CommandAllocator> CommandAllocator;
		uint64_t fenceValue;
	};

	using CommandAllocatorsQueue = std::queue<CommandAllocatorEntry>;
	using CommandListQueue = std::queue < ComPtr<ID3D12GraphicsCommandList> >;

	CommandAllocatorsQueue m_CommandAllocators;
	CommandListQueue m_CommandLists;
	D3D12_COMMAND_LIST_TYPE m_CommandListType;
	ComPtr<ID3D12Device2> m_Device;
	ComPtr<ID3D12CommandQueue> m_CommandQueue;
	ComPtr<ID3D12Fence> m_Fence;
	uint64_t m_FenceValue;
	HANDLE m_EventHandle;
};