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

    // UI の希望サイズに応じて SceneRT を差し替え（旧RTを返す）
    RenderTargetHandles HandleSceneResizeIfNeeded(ID3D12Device* dev, unsigned wantW, unsigned wantH);

    // 描画（1カメラ → 1RT）
    void RenderScene(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
        const CameraComponent* cam, const Scene* scene,
        unsigned frameIndex, unsigned maxObjects);

    // 固定カメラの Game 描画
    void RenderGame(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
        const Scene* scene, unsigned frameIndex, unsigned maxObjects);

    // HFOV 固定の再投影（テスト/再利用用に public）
    static DirectX::XMMATRIX MakeProjConstHFov(DirectX::XMMATRIX P0, float newAspect);

    // 必要なら明示解放したい場合に使う
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
};
