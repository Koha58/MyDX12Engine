#include "Core/GpuGarbage.h"
#include "Core/RenderTarget.h"      // RenderTargetHandles ‚Ì’†g‚ğg‚¤

void GpuGarbageQueue::Enqueue(UINT64 fenceValue, std::function<void()> deleter) {
    m_items.push_back({ fenceValue, std::move(deleter) });
}

void GpuGarbageQueue::Collect(ID3D12Fence* fenceObj) {
    if (!fenceObj) return;
    const UINT64 done = fenceObj->GetCompletedValue();
    while (!m_items.empty() && m_items.front().fence <= done) {
        if (m_items.front().deleter) m_items.front().deleter();
        m_items.pop_front();
    }
}

void GpuGarbageQueue::FlushAll() {
    for (auto& it : m_items) if (it.deleter) it.deleter();
    m_items.clear();
}

void EnqueueRenderTarget(GpuGarbageQueue& q, UINT64 fenceValue, RenderTargetHandles&& rt) {
    q.Enqueue(fenceValue, [h = std::move(rt)]() mutable { (void)h; });
}
