#include <CommandQueue.h>

#include <cassert>

CommandQueue::CommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type) :
	m_Device(device),
	m_CommandListType(type),
	m_FenceValue(0)
{
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc;

	commandQueueDesc.Type = type;
	commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	commandQueueDesc.NodeMask = 0;

	ThrowIfFailed(device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_CommandQueue)));
	ThrowIfFailed(device->CreateFence(m_FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)));
	m_EventHandle = ::CreateEvent(NULL, FALSE, FALSE, NULL);

	assert(m_EventHandle && "Can`t create event handle");
}

CommandQueue::~CommandQueue() {}

ComPtr<ID3D12GraphicsCommandList> CommandQueue::GetCommandList() {
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> commandList;

	if (!m_CommandAllocators.empty() && IsFenceComplite(m_CommandAllocators.front().fenceValue)) {
		commandAllocator = m_CommandAllocators.front().CommandAllocator;
		m_CommandAllocators.pop();
		ThrowIfFailed(commandAllocator->Reset());
	}
	else {
		commandAllocator = CreateCommandAllocator();
	}

	if (!m_CommandLists.empty()) {
		commandList = m_CommandLists.front();
		m_CommandLists.pop();
		ThrowIfFailed(commandList->Reset(commandAllocator.Get(), NULL));
	}
	else {
		commandList = CreateCommandList(commandAllocator);
	}

	commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get());

	return commandList;
}

ComPtr<ID3D12CommandQueue> CommandQueue::GetCommandQueue() const {
	return m_CommandQueue;
}

uint64_t CommandQueue::ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList> commandList) {
	commandList->Close();

	ID3D12CommandAllocator* commandAllocator;
	UINT dataSize = sizeof(commandAllocator);

	ThrowIfFailed(commandList->GetPrivateData(
		__uuidof(ID3D12CommandAllocator),
		&dataSize,
		&commandAllocator
	));

	ID3D12CommandList* const pCommandLists[] = { commandList.Get() };
	m_CommandQueue->ExecuteCommandLists(1, pCommandLists);
	uint64_t fenceValue = Signal();


	m_CommandAllocators.emplace(CommandAllocatorEntry{ commandAllocator, fenceValue });
	m_CommandLists.push(commandList);

	commandAllocator->Release();

	return fenceValue;
}

bool CommandQueue::IsFenceComplite(uint64_t fenceValue) const {
	return m_Fence->GetCompletedValue() >= fenceValue;
}

uint64_t CommandQueue::Signal() {
	uint64_t signalValue = ++m_FenceValue;
	m_CommandQueue->Signal(m_Fence.Get(), signalValue);
	return signalValue;
}

void CommandQueue::WaitForFenceValue(uint64_t fenceValue) {
	if (m_Fence->GetCompletedValue() < fenceValue) {
		ThrowIfFailed(m_Fence->SetEventOnCompletion(fenceValue, m_EventHandle));
		::WaitForSingleObject(m_EventHandle, DWORD_MAX);
	}
}

void CommandQueue::Flush() {
	WaitForFenceValue(Signal());
}

void CommandQueue::CloseHandle() {
	::CloseHandle(m_EventHandle);
}

ComPtr<ID3D12CommandAllocator> CommandQueue::CreateCommandAllocator() {
	ComPtr<ID3D12CommandAllocator> commandAllocator;

	ThrowIfFailed(m_Device->CreateCommandAllocator(
		m_CommandListType,
		IID_PPV_ARGS(&commandAllocator)
	));

	return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList> CommandQueue::CreateCommandList(ComPtr<ID3D12CommandAllocator> commandAllocator) {
	ComPtr<ID3D12GraphicsCommandList> commandList;

	ThrowIfFailed(m_Device->CreateCommandList(
		0,
		m_CommandListType,
		commandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&commandList)
	));

	return commandList;
}