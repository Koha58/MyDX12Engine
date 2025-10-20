// Renderer/Viewports.h
#pragma once
#include <DirectXMath.h>
#include <utility>             // std::move
#include "Core/RenderTarget.h" // RenderTarget / RenderTargetHandles

// fwd
struct ID3D12Device;
struct ID3D12GraphicsCommandList;
class   SceneRenderer;
class   CameraComponent;
class   Scene;
class   ImGuiLayer;
struct  EditorContext;

/*
    Viewports
    ----------------------------------------------------------------------------
    役割：
      - Scene / Game 用のオフスクリーン RenderTarget を管理（生成・再生成・解放移譲）
      - ImGui へ表示するための SRV (ImTextureID) を供給
      - UI 側の「希望サイズ」を受け取り、十分に安定したと判断できた時だけ再作成（デバウンス）
      - Scene は都度カメラ行列を使用、Game は「最初に Scene と同期した固定 View/Proj」を使用

    キー設計：
      - RequestSceneResize() で「希望サイズ」を受け取り、一定時間（kRequiredStableTime）
        変動が無ければ ApplyPendingResizeIfNeeded() で実サイズに反映
      - 再作成時、古い RT は Detach() でパッケージ化して返す（GPU 完了後に遅延破棄）
        * 同一フレームに 2 つ以上の古い RT が発生した場合は m_carryOverDead にプール
      - Game のカメラは「View=固定 / Proj=アスペクトに応じて更新」の方針
        * 初回のみ Scene のカメラ姿勢と“横 FOV を保つ投影”で同期
*/

class Viewports {
public:
    // 初期化：Scene/Game の RT を (w,h) で作成
    void Initialize(ID3D12Device* dev, unsigned w, unsigned h);

    // ImGui へ SRV を供給し、EditorContext に実RTサイズ等を反映
    void FeedToUI(EditorContext& ctx, ImGuiLayer* imgui,
        unsigned frameIndex, unsigned sceneSrvBase, unsigned gameSrvBase);

    // UI から渡された Scene ウィンドウの「利用可能サイズ」を受け取り、デバウンス処理に回す
    void RequestSceneResize(unsigned w, unsigned h, float dt);

    // ペンディングされていたリサイズを即時適用
    //  - 新しい RT を作成
    //  - 古い RT を Detach() して戻す（最大1個）
    //  - 2個目は内部の m_carryOverDead へ持ち越し
    RenderTargetHandles ApplyPendingResizeIfNeeded(ID3D12Device* dev);

    // 内部の“持ち越し死骸”を受け取り、呼び出し側（FrameScheduler::EndFrame 等）で遅延破棄登録する
    RenderTargetHandles TakeCarryOverDead() {
        RenderTargetHandles out = std::move(m_carryOverDead);
        m_carryOverDead = {};
        return out;
    }

    // Scene パスの記録（Scene カメラに基づき、横FOV固定で縦FOVを再計算）
    void RenderScene(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
        const CameraComponent* cam, const Scene* scene,
        unsigned frameIndex, unsigned maxObjects);

    // Game パスの記録（最初に Scene と同期した固定 View/Proj を使う）
    void RenderGame(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
        const Scene* scene, unsigned frameIndex, unsigned maxObjects);

    // 透視投影：元の投影 P0 の near/far と“横 FOV”を保ちつつ、newAspect に合わせて縦 FOV を再計算
    static DirectX::XMMATRIX MakeProjConstHFov(DirectX::XMMATRIX P0, float newAspect);

    // 実 RT サイズ（RTが未作成なら 0）
    unsigned SceneWidth()  const noexcept;
    unsigned SceneHeight() const noexcept;
    unsigned GameWidth()   const noexcept;
    unsigned GameHeight()  const noexcept;

    // 直接アクセスが必要な場合の参照（ImGui SRV 作成など）
    RenderTarget& SceneRT() { return m_scene; }
    RenderTarget& GameRT() { return m_game; }

private:
    // オフスクリーンRT（カラー/深度、RTV/DSV、遷移ヘルパ等を内包）
    RenderTarget m_scene;
    RenderTarget m_game;

    // ---- Scene 側：投影行列の基準をキャプチャ（初回のみ） ----
    DirectX::XMFLOAT4X4 m_sceneProjInit{};  // 初回の投影を保存
    bool                m_sceneProjCaptured = false;

    // ---- Game 側：固定カメラ（初回だけ Scene と同期） ----
    bool                m_gameFrozen = false;      // 初回同期が済んだら true
    DirectX::XMFLOAT4X4 m_gameViewInit{};          // 固定 View
    DirectX::XMFLOAT4X4 m_gameProjInit{};          // 固定 Proj（アスペクト変化時は再計算して更新）

    // ---- デバウンス状態（希望サイズの安定化） ----
    unsigned m_desiredW = 0, m_desiredH = 0; // 最新の「希望サイズ（スナップ後）」を保持
    float    m_desiredStableTime = 0.0f;     // 同一希望が継続した累積時間

    // ---- ペンディング ----
    unsigned m_pendingW = 0;                 // 実際に適用予定のサイズ（安定判定後）
    unsigned m_pendingH = 0;

    // 2個目の“死骸”置き場（今フレームで流せない分を持ち越す）
    RenderTargetHandles m_carryOverDead;

    // デバウンス・スナップのパラメータ
    static constexpr float    kRequiredStableTime = 0.10f; // 一定時間ブレなければ確定（秒）
    static constexpr unsigned kMinDeltaPx = 4;      // 微小変化無視のしきい（未使用なら 0）
    static constexpr unsigned kSnapStep = 16;     // サイズ変動時のステップスナップ（px）
};
