// PCH‚ðŽg‚Á‚Ä‚¢‚é‚È‚ç—LŒø‰»
#include "pch.h"

#include "Renderer/FrameScheduler.h"

#include <algorithm>       // std::max
#include <utility>         // std::move
#include <windows.h>       // HANDLE, WaitForSingleObject
#include <d3d12.h>

#include "Core/DeviceResources.h"
#include "Core/FrameResources.h"
#include "Core/GpuGarbage.h"
#include "Core/RenderTarget.h"

void FrameScheduler::Initialize(DeviceResources* dev,
    ID3D12Fence* fence,
    void* fenceEvent,
    FrameResources* frames,
    GpuGarbageQueue* garbage)
{
    m_dev = dev;
    m_fence = fence;
    m_fenceEvent = fenceEvent;
    m_frames = frames;
    m_garbage = garbage;

    std::uint64_t completed = (m_fence ? m_fence->GetCompletedValue() : 0);
    m_nextFence = completed + 1;
}

FrameScheduler::BeginInfo FrameScheduler::BeginFrame()
{
    unsigned fi = m_dev->GetCurrentBackBufferIndex();
    auto& fr = m_frames->Get(fi);

    if (fr.fenceValue != 0 && m_fence->GetCompletedValue() < fr.fenceValue) {
        m_fence->SetEventOnCompletion(fr.fenceValue, static_cast<HANDLE>(m_fenceEvent));
        WaitForSingleObject(static_cast<HANDLE>(m_fenceEvent), INFINITE);
    }

    fr.cmdAlloc->Reset();

    if (!m_cmd) {
        ID3D12Device* dev = m_dev->GetDevice();
        HRESULT hr = dev->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, fr.cmdAlloc.Get(),
            nullptr, IID_PPV_ARGS(&m_cmd));
        if (SUCCEEDED(hr)) m_cmd->Close(); // Reset ‘O’ñ‚É‘µ‚¦‚é
    }
    m_cmd->Reset(fr.cmdAlloc.Get(), nullptr);

    return { fi, m_cmd };
}

void FrameScheduler::EndFrame(RenderTargetHandles toDispose)
{
    unsigned fi = m_dev->GetCurrentBackBufferIndex();
    auto& fr = m_frames->Get(fi);

    m_cmd->Close();
    ID3D12CommandList* lists[] = { m_cmd };
    m_dev->GetQueue()->ExecuteCommandLists(1, lists);
    m_dev->Present(1);

    std::uint64_t completed = m_fence->GetCompletedValue();
    m_nextFence = std::max<std::uint64_t>(m_nextFence, completed + 1);

    std::uint64_t sig = m_nextFence++;
    m_dev->GetQueue()->Signal(m_fence, sig);
    fr.fenceValue = sig;

    if (toDispose.color || toDispose.depth || toDispose.rtvHeap || toDispose.dsvHeap) {
        if (m_garbage) EnqueueRenderTarget(*m_garbage, sig, std::move(toDispose));
    }
    if (m_garbage) m_garbage->Collect(m_fence);
}

FrameScheduler::~FrameScheduler()
{
    if (m_cmd) { m_cmd->Release(); m_cmd = nullptr; }
}
