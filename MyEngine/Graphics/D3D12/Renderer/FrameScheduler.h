#pragma once
// ============================================================================
// FrameScheduler.h
// 目的：
//   - 1フレームのライフサイクル（Begin → コマンド記録 → End/Present）を集約する小さな司令塔。
//   - 複数フレーム同時進行（FrameCount 個の FrameResources をリングで回す）時の
//     フェンス待ち・コマンドアロケータの Reset・Present/Signal・遅延破棄の実行を一括管理。
// 設計ポイント：
//   - BeginFrame() で「このフレームで使う FrameResources/コマンドリスト」を返す。
//   - EndFrame() で Present + フェンス Signal、RenderTarget の遅延破棄登録（任意）。
//   - 「Begin 時に取得したバックバッファインデックス」を保持して End でもそれを使う。
//     （Present 後に取り直すとインデックスが進んでしまい、異なる FrameResources を
//      参照してしまう事故を防止）
// 使い方：
//   auto b = scheduler.BeginFrame();
//   ID3D12GraphicsCommandList* cmd = b.cmd;
//   // …cmd へ記録…
//   scheduler.EndFrame(&rtToDispose); // 破棄が無ければ nullptr かデフォルト引数でOK
// ============================================================================

#include <cstdint>

// --- fwd declares（重依存を避ける） ---
struct ID3D12Fence;
struct ID3D12GraphicsCommandList;
class  DeviceResources;
class  FrameResources;
class  GpuGarbageQueue;
struct RenderTargetHandles; // 前方宣言のままでOK（実体は Core/RenderTarget.h 側）

class FrameScheduler {
public:
    // BeginFrame() が返す最小セット
    struct BeginInfo {
        unsigned frameIndex;                 // このフレームで使用するフレームインデックス
        ID3D12GraphicsCommandList* cmd;      // 記録に使うコマンドリスト
    };

    // ----------------------------------------------------------------------------
    // Initialize
    // 役割：
    //   - 必要な共有オブジェクト（デバイス/フェンス/イベント/フレーム群/遅延破棄キュー）を結線
    //   - フェンス値の初期化（既に Signal 済みの可能性を考慮し、completed+1 から開始）
    // 前提：
    //   - dev/fence/frames は有効
    // ----------------------------------------------------------------------------
    void Initialize(DeviceResources* dev,
        ID3D12Fence* fence,
        void* fenceEvent,
        FrameResources* frames,
        GpuGarbageQueue* garbage);

    // ----------------------------------------------------------------------------
    // BeginFrame
    // 役割：
    //   - カレントのバックバッファインデックスを取得し、対応する FrameResources を選択
    //   - 必要ならその Frame のフェンス完了を待ってコマンドアロケータを Reset
    //   - コマンドリストを（初回作成 or）Reset して返す
    // 戻り値：
    //   - BeginInfo（frameIndex と cmd）
    // ----------------------------------------------------------------------------
    BeginInfo BeginFrame();

    // ----------------------------------------------------------------------------
    // EndFrame
    // 役割：
    //   - コマンドリストを Close → Execute → Present
    //   - フェンスを Signal し、当該フレームの fenceValue を記録
    //   - 渡された RenderTargetHandles があれば、その fence にぶら下げて遅延破棄登録
    // 引数：
    //   - toDispose: 今フレームの終了時に破棄したい RT セット（nullptr なら破棄なし）
    // 備考：
    //   - GpuGarbageQueue::Collect を呼んで完了済み分を都度回収
    // ----------------------------------------------------------------------------
    void EndFrame(RenderTargetHandles* toDispose = nullptr);

    // 現在のフレームで使っているコマンドリストを外に渡したい時に
    ID3D12GraphicsCommandList* GetCmd() const { return m_cmd; }

    // 終了時の後始末（コマンドリストの Release など）
    ~FrameScheduler();

private:
    // 外部から供給される共有オブジェクト群（借用）
    DeviceResources* m_dev = nullptr; // デバイス/スワップチェイン/キュー
    FrameResources* m_frames = nullptr; // フレームごとのアロケータ/CB
    GpuGarbageQueue* m_garbage = nullptr; // 遅延破棄キュー
    ID3D12GraphicsCommandList* m_cmd = nullptr; // 使い回す 1 本のコマンドリスト
    ID3D12Fence* m_fence = nullptr; // GPU 同期フェンス（外部所有）
    void* m_fenceEvent = nullptr; // フェンス待機イベント（HANDLE を void* で保持）

    std::uint64_t               m_nextFence = 0;       // 次に Signal するフェンス値

    // ★ BeginFrame で取得したバックバッファインデックスを保存し、
    //   EndFrame で Present 後に「再取得しない」ためのキャッシュ
    unsigned                    m_inFlightFrameIndex = 0;
};
