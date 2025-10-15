#include "Core/GpuGarbage.h"
#include "Core/RenderTarget.h" // RenderTargetHandles の定義が必要
#include <utility>

static bool IsEmpty(const RenderTargetHandles& h) {
    return !h.color && !h.depth && !h.rtvHeap && !h.dsvHeap;
}

void GpuGarbageQueue::EnqueueRT(UINT64 fenceValue, RenderTargetHandles&& rt)
{
    if (IsEmpty(rt)) return;

    Item item;
    item.fence = fenceValue;
    item.rt = std::make_unique<RenderTargetHandles>(std::move(rt)); // 所有権を移す
    m_rts.push_back(std::move(item));
}

void GpuGarbageQueue::Collect(ID3D12Fence* fenceObj)
{
    if (!fenceObj) return;
    const UINT64 done = fenceObj->GetCompletedValue();
    while (!m_rts.empty() && m_rts.front().fence <= done) {
        // unique_ptr がスコープアウトして ComPtr が Release される
        m_rts.pop_front();
    }
}

void GpuGarbageQueue::FlushAll()
{
    m_rts.clear(); // unique_ptr の解放で全部 Release
}

void EnqueueRenderTarget(GpuGarbageQueue& q, UINT64 fenceValue, RenderTargetHandles&& rt)
{
    q.EnqueueRT(fenceValue, std::move(rt));
}
