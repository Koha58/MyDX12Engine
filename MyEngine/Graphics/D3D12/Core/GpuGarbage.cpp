#include "Core/GpuGarbage.h"
#include "Core/RenderTarget.h" // RenderTargetHandles の定義が必要
#include <utility>

/*
    GpuGarbageQueue
    ----------------------------------------------------------------------------
    目的：
      ・「GPU がまだ使っているかもしれない D3D12 リソース」を、安全に解放するための
        遅延破棄キュー。Fence 値で寿命を管理し、完了済みになってから Release する。

    運用：
      1) リソースの所有権を RenderTarget などから「切り離す(Detach)」。
      2) その RenderTargetHandles を fenceValue(=このフレームの Signal 値)と一緒に Enqueue。
      3) 毎フレ Collect() を呼び、GetCompletedValue() が追いついた分だけ破棄。
      4) シャットダウン時は FlushAll() で強制解放（必ず GPU 完了待ち済みで呼ぶこと）。

    設計メモ：
      - RenderTargetHandles は ComPtr を持つ集約構造体。unique_ptr で包み
        コンテナ内のムーブ/ポップ時に確実に Release されるようにする。
      - キューは「Fence の昇順」を期待（先頭から条件を満たす限り破棄）。
      - Fence の wrap-around は 64bit なので実質非考慮（超長期稼働で必要なら比較を工夫）。
      - マルチスレッドで使うなら、呼び出し側で同期すること（この実装は非スレッドセーフ）。
*/

// 小物：空ハンドルの判定（全 ComPtr が空）
static bool IsEmpty(const RenderTargetHandles& h) {
    return !h.color && !h.depth && !h.rtvHeap && !h.dsvHeap;
}

void GpuGarbageQueue::EnqueueRT(UINT64 fenceValue, RenderTargetHandles&& rt)
{
    // 何も持っていないなら何もしない（余計なノードを積まない）
    if (IsEmpty(rt)) return;

    // Item を作って所有権を移す（rt の中身はここで“消費”される）
    Item item;
    item.fence = fenceValue;

    // unique_ptr で保持：コンテナから pop された瞬間に ComPtr が Release される
    item.rt = std::make_unique<RenderTargetHandles>(std::move(rt));

    // ★前提：呼び出し順は fenceValue の昇順（通常はフレーム順）で来る想定
    //        異なる順序で投入する場合は、m_rts を並べ替える or Collect のロジックを変更すること。
    m_rts.push_back(std::move(item));
}

void GpuGarbageQueue::Collect(ID3D12Fence* fenceObj)
{
    // Fence が無ければ何もできない
    if (!fenceObj) return;

    // 直近まで GPU が完了した fence 値を取得
    const UINT64 done = fenceObj->GetCompletedValue();

    // 先頭から「完了済み（<= done）」のものだけ順に破棄
    // ※ Item 内の unique_ptr がスコープアウト → RenderTargetHandles の ComPtr が Release
    while (!m_rts.empty() && m_rts.front().fence <= done) {
        m_rts.pop_front();
    }
}

void GpuGarbageQueue::FlushAll()
{
    // ★注意：FlushAll は“無条件に破棄”する。
    //   必ず呼び出し側で GPU 完了待ち（WaitForGPU）を済ませていること。
    //   そうでないと、まだ使用中のリソースを解放してしまう恐れがある。
    m_rts.clear(); // Item の unique_ptr が全て解放 → ComPtr が Release
}

// C-style のユーティリティ薄ラッパ（呼び出し元の include を最小化したい場合に便利）
void EnqueueRenderTarget(GpuGarbageQueue& q, UINT64 fenceValue, RenderTargetHandles&& rt)
{
    q.EnqueueRT(fenceValue, std::move(rt));
}
