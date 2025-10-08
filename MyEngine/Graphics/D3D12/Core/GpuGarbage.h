#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <deque>
#include <functional>
#include <cstdint>

// RenderTarget.h 側で定義（ここでは前方宣言だけ）
struct RenderTargetHandles;

/** GPU フェンスにぶら下げる遅延破棄キュー */
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

/** RenderTarget の旧ハンドルを遅延破棄に積むヘルパ */
void EnqueueRenderTarget(GpuGarbageQueue& q, UINT64 fenceValue, RenderTargetHandles&& rt);

