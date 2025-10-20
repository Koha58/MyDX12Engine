#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <deque>
#include <memory>
#include <cstdint>

// RenderTarget.h は include しない（依存の逆転・ビルド時間短縮のため前方宣言に留める）
struct RenderTargetHandles;

/*
    GpuGarbageQueue
    ----------------------------------------------------------------------------
    目的：
      - 「今フレーム切り離した GPU リソース」を、GPU の使用が完了するまで
        生かしておき、安全に解放するための遅延破棄キュー。
      - フレーム境界で Fence を Signal し、その値（fenceValue）にひも付けて
        破棄対象をエンキュー。以降、Collect() で completedValue に到達した
        ものから順に解放する。

    想定フロー（例：RenderTarget のリサイズ時）：
      1) 古い RT を Detach() して RenderTargetHandles に束ねる
      2) 今フレームの Signal 後に、EnqueueRT(signalValue, std::move(handles)) を呼ぶ
      3) 毎フレーム Collect(fenceObj) を呼び、フェンス到達済みのものを解放
      4) 終了時は FlushAll() で即時全解放（GPU 完了を待ってからが安全）

    設計メモ：
      - RenderTargetHandles は未完成型のため、直接メンバには持てない。
        → std::unique_ptr<RenderTargetHandles> で type-erasure 的に保持。
      - キューは FIFO。フェンス値は単調増加が前提。
      - スレッドセーフではない（呼び出しスレッドを 1 本に揃える想定）。
*/
class GpuGarbageQueue {
public:
    /*
        EnqueueRT
        ------------------------------------------------------------------------
        @param fenceValue : 「この値に到達したら破棄して良い」ことを示すフェンス値
        @param rt         : Detach して得た RenderTargetHandles（所有権を奪う）
        効果             : rt をフェンス値とともにキュー末尾へ積む
    */
    void EnqueueRT(UINT64 fenceValue, RenderTargetHandles&& rt);

    /*
        Collect
        ------------------------------------------------------------------------
        @param fenceObj : GPU 完了値を問い合わせる対象フェンス
        効果           : fenceObj->GetCompletedValue() に達した Item を
                         先頭から順に解放していく
        注意           : フェンス値は単調増加が前提。達していない Item は残す。
    */
    void Collect(ID3D12Fence* fenceObj);

    /*
        FlushAll
        ------------------------------------------------------------------------
        効果 : 保持しているすべての Item を即座に解放する（フェンス値に関係なく）。
               アプリ終了時や「すでに GPU は完全停止済み」と分かっている局面で使用。
        注意 : 実行中の GPU が参照している可能性がある状況では使わないこと。
    */
    void FlushAll();

    // 現在キューに溜まっている待機中アイテム数（デバッグ用途）
    size_t PendingCount() const noexcept { return m_rts.size(); }

private:
    struct Item {
        UINT64 fence = 0;                                   // 到達したら破棄して良いフェンス値
        std::unique_ptr<RenderTargetHandles> rt;            // 破棄待ちの RT セット
    };
    std::deque<Item> m_rts;                                 // 先頭＝最も古い（小さいフェンス値）
};

/*
    互換ヘルパ（レガシー呼び出し名の保持用）
    ------------------------------------------------------------------------
    既存コードに EnqueueRenderTarget(q, fence, std::move(rt)) が残っている場合に、
    新 API（GpuGarbageQueue::EnqueueRT）を呼び出す薄いラッパ。
*/
void EnqueueRenderTarget(GpuGarbageQueue& q, UINT64 fenceValue, RenderTargetHandles&& rt);
