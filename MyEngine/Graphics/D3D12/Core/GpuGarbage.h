#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <deque>
#include <memory>
#include <cstdint>

// RenderTarget.h は include しない（前方宣言のみ）
struct RenderTargetHandles;

/** GPU フェンスにぶら下げる遅延破棄キュー */
class GpuGarbageQueue {
public:
    // RenderTarget をフェンス到達まで保持して解放
    void EnqueueRT(UINT64 fenceValue, RenderTargetHandles&& rt);
    // フェンス completedValue に達したものだけ順に解放
    void Collect(ID3D12Fence* fenceObj);
    // 即全解放（終了時など）
    void FlushAll();

    size_t PendingCount() const noexcept { return m_rts.size(); }

private:
    struct Item {
        UINT64 fence = 0;
        // 未完成型をメンバにできないため unique_ptr で保持
        std::unique_ptr<RenderTargetHandles> rt;
    };
    std::deque<Item> m_rts;
};

/** 互換ヘルパ（既存呼び出し名を残したい場合に） */
void EnqueueRenderTarget(GpuGarbageQueue& q, UINT64 fenceValue, RenderTargetHandles&& rt);
