// Renderer/FrameScheduler.cpp
// PCHを使っているなら
// #include "pch.h"

#include "Renderer/FrameScheduler.h"

#include <windows.h>
#include <d3d12.h>
#include <algorithm>
#include <utility>
#include <cassert>

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

    // 既に他所でSignalされてる可能性を考慮して completed+1 から開始
    const std::uint64_t completed = (m_fence ? m_fence->GetCompletedValue() : 0);
    m_nextFence = completed + 1;

    // インフライトのフレームインデックス初期化
    m_inFlightFrameIndex = 0;
}

FrameScheduler::BeginInfo FrameScheduler::BeginFrame()
{
    // ★ この時点のバックバッファインデックスを記録（EndFrameで再取得しない）
    const unsigned fi = m_dev->GetCurrentBackBufferIndex();
    m_inFlightFrameIndex = fi;

    auto& fr = m_frames->Get(fi);

    // 前フレーム完了待ち
    if (fr.fenceValue != 0 && m_fence->GetCompletedValue() < fr.fenceValue) {
        m_fence->SetEventOnCompletion(fr.fenceValue, static_cast<HANDLE>(m_fenceEvent));
        WaitForSingleObject(static_cast<HANDLE>(m_fenceEvent), INFINITE);
    }

    // Reset
    fr.cmdAlloc->Reset();

    // 初回のみコマンドリスト生成
    if (!m_cmd) {
        ID3D12Device* dev = m_dev->GetDevice();
        HRESULT hr = dev->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, fr.cmdAlloc.Get(),
            nullptr, IID_PPV_ARGS(&m_cmd));
        if (SUCCEEDED(hr)) {
            m_cmd->Close(); // Reset前提に揃える
        }
    }

    // PSOは呼び出し側でSetする前提（ここではnullptr）
    m_cmd->Reset(fr.cmdAlloc.Get(), nullptr);

    return { fi, m_cmd };
}

void FrameScheduler::EndFrame(RenderTargetHandles* toDispose)
{
    // ★ BeginFrameで記録したインデックスを使う（Present後に取り直さない）
    const unsigned fi = m_inFlightFrameIndex;
    auto& fr = m_frames->Get(fi);

    // Submit & Present
    m_cmd->Close();
    ID3D12CommandList* lists[] = { m_cmd };
    m_dev->GetQueue()->ExecuteCommandLists(1, lists);
    m_dev->Present(1);

    // 外部Signalがあっても単調増加を維持
    const std::uint64_t completed = m_fence->GetCompletedValue();
    m_nextFence = std::max<std::uint64_t>(m_nextFence, completed + 1);

    const std::uint64_t sig = m_nextFence++;
    m_dev->GetQueue()->Signal(m_fence, sig);

    // ★ このフレームのFrameResourceに紐づける
    fr.fenceValue = sig;

    // このフレーム完了後に旧RTを破棄
    if (toDispose && (toDispose->color || toDispose->depth || toDispose->rtvHeap || toDispose->dsvHeap)) {
        if (m_garbage) EnqueueRenderTarget(*m_garbage, sig, std::move(*toDispose));
        *toDispose = {}; // 二重破棄防止
    }
    if (m_garbage) m_garbage->Collect(m_fence);
}

FrameScheduler::~FrameScheduler()
{
    if (m_cmd) { m_cmd->Release(); m_cmd = nullptr; }
}
