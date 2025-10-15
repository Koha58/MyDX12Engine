#include "Core/GpuGarbage.h"
#include "Core/RenderTarget.h" // RenderTargetHandles �̒�`���K�v
#include <utility>

static bool IsEmpty(const RenderTargetHandles& h) {
    return !h.color && !h.depth && !h.rtvHeap && !h.dsvHeap;
}

void GpuGarbageQueue::EnqueueRT(UINT64 fenceValue, RenderTargetHandles&& rt)
{
    if (IsEmpty(rt)) return;

    Item item;
    item.fence = fenceValue;
    item.rt = std::make_unique<RenderTargetHandles>(std::move(rt)); // ���L�����ڂ�
    m_rts.push_back(std::move(item));
}

void GpuGarbageQueue::Collect(ID3D12Fence* fenceObj)
{
    if (!fenceObj) return;
    const UINT64 done = fenceObj->GetCompletedValue();
    while (!m_rts.empty() && m_rts.front().fence <= done) {
        // unique_ptr ���X�R�[�v�A�E�g���� ComPtr �� Release �����
        m_rts.pop_front();
    }
}

void GpuGarbageQueue::FlushAll()
{
    m_rts.clear(); // unique_ptr �̉���őS�� Release
}

void EnqueueRenderTarget(GpuGarbageQueue& q, UINT64 fenceValue, RenderTargetHandles&& rt)
{
    q.EnqueueRT(fenceValue, std::move(rt));
}
