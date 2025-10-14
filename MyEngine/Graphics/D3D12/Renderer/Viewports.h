#pragma once
#include <DirectXMath.h>
#include "Core/RenderTarget.h" // 値メンバで保持するため必要

// ---- forward declarations ----
struct ID3D12Device;
struct ID3D12GraphicsCommandList;
class SceneRenderer;
class CameraComponent;
class Scene;
class ImGuiLayer;
struct EditorContext;

class Viewports {
public:
    void Initialize(ID3D12Device* dev, unsigned w, unsigned h);

    // ImGui 用 SRV を確保し EditorContext に流し込む
    void FeedToUI(EditorContext& ctx, ImGuiLayer* imgui,
        unsigned frameIndex, unsigned sceneSrvBase, unsigned gameSrvBase);

    // ★ UI からの希望サイズを記録（このフレームでは作り直さない）
    //    dt を受け取り時間安定判定（ヒステリシス）を行う
    void RequestSceneResize(unsigned w, unsigned h, float dt);

    // ★ 次フレーム冒頭で保留リサイズを適用（旧RTを返す）
    RenderTargetHandles ApplyPendingResizeIfNeeded(ID3D12Device* dev);

    // 描画（1カメラ → 1RT）
    void RenderScene(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
        const CameraComponent* cam, const Scene* scene,
        unsigned frameIndex, unsigned maxObjects);

    // 固定カメラの Game 描画
    void RenderGame(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
        const Scene* scene, unsigned frameIndex, unsigned maxObjects);

    // HFOV 固定の再投影（テスト/再利用用に public）
    static DirectX::XMMATRIX MakeProjConstHFov(DirectX::XMMATRIX P0, float newAspect);

    // 現在の Scene / Game RT のサイズ（UI 側の比較用）
    unsigned SceneWidth()  const noexcept;
    unsigned SceneHeight() const noexcept;
    unsigned GameWidth()   const noexcept;
    unsigned GameHeight()  const noexcept;

    // 必要なら明示解放したい場合に使う（直接操作したいとき）
    RenderTarget& SceneRT() { return m_scene; }
    RenderTarget& GameRT() { return m_game; }

private:
    RenderTarget m_scene;
    RenderTarget m_game;

    DirectX::XMFLOAT4X4 m_sceneProjInit{};
    bool m_sceneProjCaptured = false;

    bool m_gameFrozen = false;
    DirectX::XMFLOAT4X4 m_gameViewInit{};
    DirectX::XMFLOAT4X4 m_gameProjInit{};

    // ★ ヒステリシス用バッファ
    unsigned m_desiredW = 0, m_desiredH = 0; // UI から来た直近の希望サイズ
    float    m_desiredStableTime = 0.0f;     // 希望が連続で維持された時間

    // ★ 次フレームに適用するペンディングサイズ（確定済み）
    unsigned m_pendingW = 0;
    unsigned m_pendingH = 0;

    // ★ 調整パラメータ
    static constexpr float   kRequiredStableTime = 0.10f; // 100ms以上安定で確定
    static constexpr unsigned kMinDeltaPx = 4;     // 4px 未満は無視
    static constexpr unsigned kSnapStep = 16;    // 16px スナップ
};
