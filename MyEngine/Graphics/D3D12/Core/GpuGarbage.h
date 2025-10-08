#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <deque>
#include <functional>
#include <cstdint>

// RenderTarget.h ���Œ�`�i�����ł͑O���錾�����j
struct RenderTargetHandles;

/** GPU �t�F���X�ɂԂ牺����x���j���L���[ */
class GpuGarbageQueue {
public:
    void Enqueue(UINT64 fenceValue, std::function<void()> deleter);
    void Collect(ID3D12Fence* fenceObj);
    void FlushAll();
    size_t PendingCount() const noexcept { return m_items.size(); }
private:
    struct Item { UINT64 fence = 0; std::function<void()> deleter; };
    std::deque<Item> m_items;
};

/** RenderTarget �̋��n���h����x���j���ɐςރw���p */
void EnqueueRenderTarget(GpuGarbageQueue& q, UINT64 fenceValue, RenderTargetHandles&& rt);

