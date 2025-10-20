// Renderer/SceneLayer.h
#pragma once
#include <d3d12.h>
#include "Renderer/Viewports.h"
#include "Renderer/SceneRenderer.h"
#include "Editor/EditorContext.h"
#include "Editor/ImGuiLayer.h"

/*
    SceneLayer
    ----------------------------------------------------------------------------
    役割：
      - Scene(RenderTarget) と Game(RenderTarget) の二面ビューをまとめて管理。
      - Viewports（RT生成/リサイズ/ImGui表示用SRVの供給）と
        SceneRenderer（メッシュ描画/CB更新）の橋渡しを行う薄いファサード。

    フレームの基本フロー（Renderer 側からの呼び出し順）：
      1) BeginFrame(dev)
         - 前フレームで UI が要求したリサイズ（ペンディング）を確定して RT を作り直す
         - 古い RT（最大1セット）を RenderTargetHandles として返す
           → Renderer::EndFrame で GpuGarbageQueue に登録して遅延破棄する
      2) Record(args, maxObjects)
         - Scene RT / Game RT の両方に描画コマンドを記録
         - Scene は毎回カメラに追従、Game は「最初の1回だけ」Scene と同期し、その後は固定
      3) FeedToUI(ctx, imgui, ...)
         - 両 RT の SRV（ImTextureID）を確保して EditorContext に流し込む
      4) RequestResize(w, h, dt)
         - UI 側が希望する表示ピクセルサイズをデバウンス付きで受付（ただしここでは確定しない）
         - 実際の RT 再生成は次フレームの BeginFrame で行う

    ポイント：
      - リサイズ即時反映だとドラッグ中に RT 再作成が連発してカクつくため、
        Viewports 側で StepSnap + 一定時間の安定を待ってから確定する。
      - 古い RT は GPU がまだ使っている可能性があるので、Detach→GpuGarbageQueue で安全に破棄。
      - Game ビューは「姿勢(View)は固定、投影(Proj)だけアスペクトに合わせて更新」する仕様に対応済み。
*/

/// コマンド記録時に必要な入力束（毎フレ作って渡す）
struct SceneLayerBeginArgs {
    ID3D12Device* device = nullptr;  // デバイス（必要に応じて Viewports で使用）
    ID3D12GraphicsCommandList* cmd = nullptr; // 記録先のコマンドリスト（必須）
    unsigned                  frameIndex = 0;    // フレームリングバッファのインデックス
    class Scene* scene = nullptr;   // 描画対象シーン（ルートから再帰）
    class CameraComponent* camera = nullptr;  // Scene ビュー用の参照カメラ
};

class SceneLayer {
public:
    // ------------------------------------------------------------------------
    // Initialize（推奨：初期ウィンドウサイズを渡す版）
    //  - 初回の RT をウィンドウ実サイズで作成して「拡大時の一時ボケ」を抑える。
    //  - frames : フレーム別のアップロード CB など（SceneRenderer が使用）
    //  - pipe   : ルートシグネチャ/PSO（SceneRenderer が使用）
    // ------------------------------------------------------------------------
    void Initialize(ID3D12Device* dev, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
        FrameResources* frames, const PipelineSet& pipe,
        unsigned initW, unsigned initH);

    // 互換：旧 5 引数版（サイズ指定なし → 小さめの仮サイズで作成）
    void Initialize(ID3D12Device* dev, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
        FrameResources* frames, const PipelineSet& pipe)
    {
        Initialize(dev, rtvFmt, dsvFmt, frames, pipe, 64, 64);
    }

    // ------------------------------------------------------------------------
    // BeginFrame
    //  - Viewports が保持している「保留中のリサイズ要求」をここで確定・適用。
    //  - 新しい RT を作り直し、古い RT（最大1個ぶん）を呼び出し側へ返す。
    //    → 呼び出し側は FrameScheduler::EndFrame で遅延破棄登録すること。
    // ------------------------------------------------------------------------
    RenderTargetHandles BeginFrame(ID3D12Device* dev);

    // ------------------------------------------------------------------------
    // Record
    //  - Scene / Game の両レンダーターゲットへ描画。
    //  - maxObjects：1フレームに積める定数バッファスロット数（Scene と Game で分割使用）。
    // ------------------------------------------------------------------------
    void Record(const SceneLayerBeginArgs& args, unsigned maxObjects);

    // ------------------------------------------------------------------------
    // FeedToUI
    //  - ImGui 表示用に SRV（ImTextureID）を確保して EditorContext に書き出す。
    //  - sceneSrvBase / gameSrvBase：フレーム別に SRV スロットが衝突しないようベースをずらす。
    // ------------------------------------------------------------------------
    void FeedToUI(EditorContext& ctx, ImGuiLayer* imgui,
        unsigned frameIndex, unsigned sceneSrvBase, unsigned gameSrvBase);

    // ------------------------------------------------------------------------
    // RequestResize
    //  - UI から渡された「Scene ビューポートの希望サイズ」を記録するだけ（確定は次フレ）。
    //  - dt を積算して一定時間安定したらペンディングを確定（Viewports 側の実装）。
    // ------------------------------------------------------------------------
    void RequestResize(unsigned wantW, unsigned wantH, float dt);

    // ------------------------------------------------------------------------
    // SyncStatsTo
    //  - Viewports が持つ「実際の RT の現在サイズ」を EditorContext に反映。
    //  - UI 側の表示やデバッグに使用（ウィンドウ/スワップチェインのサイズとは区別）。
    // ------------------------------------------------------------------------
    void SyncStatsTo(EditorContext& ctx) const;

    // ------------------------------------------------------------------------
    // Shutdown
    //  - 内部の RenderTarget 参照を切り、必要に応じて遅延破棄キューへ載せる前提のデタッチ。
    // ------------------------------------------------------------------------
    void Shutdown();

    // ------------------------------------------------------------------------
    // TakeCarryOverDead
    //  - BeginFrame で返しきれなかった「もう1組の古い RT（持ち越し）」を引き取る。
    //    Renderer 側で “今フレで捨て物がなかった場合のみ” 回収する用途を想定。
    // ------------------------------------------------------------------------
    RenderTargetHandles TakeCarryOverDead();

    // （オプション）現在の RT 実サイズ
    unsigned SceneWidth()  const { return m_viewports.SceneWidth(); }
    unsigned SceneHeight() const { return m_viewports.SceneHeight(); }
    unsigned GameWidth()   const { return m_viewports.GameWidth(); }
    unsigned GameHeight()  const { return m_viewports.GameHeight(); }

private:
    Viewports     m_viewports;     // RT の生成/リサイズ/ImGui SRV 供給・Gameカメラ保持など
    SceneRenderer m_sceneRenderer; // メッシュ描画（CB 書き込み/PSO/RS 設定）
};
