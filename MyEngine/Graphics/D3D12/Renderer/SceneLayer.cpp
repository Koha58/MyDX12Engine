// Renderer/SceneLayer.cpp
#include "Renderer/SceneLayer.h"

void SceneLayer::Initialize(ID3D12Device* dev, DXGI_FORMAT /*rtvFmt*/, DXGI_FORMAT /*dsvFmt*/,
    FrameResources* frames, const PipelineSet& pipe,
    unsigned initW, unsigned initH)
{
    // SceneRenderer を結線
    m_sceneRenderer.Initialize(dev, pipe, frames);

    // Viewports（RT 管理）初期化：初期ウィンドウサイズで作成（ぼやけ防止）
    m_viewports.Initialize(dev, initW, initH);
}

RenderTargetHandles SceneLayer::BeginFrame(ID3D12Device* dev)
{
    // UI から確定したペンディング・リサイズがあればここで適用
    return m_viewports.ApplyPendingResizeIfNeeded(dev);
}

void SceneLayer::Record(const SceneLayerBeginArgs& a, unsigned maxObjects)
{
    if (!a.camera || !a.scene || !a.cmd) return;

    // Scene へ描画（HFOV 固定・アスペクト追従は Viewports 側）
    m_viewports.RenderScene(a.cmd, m_sceneRenderer, a.camera, a.scene, a.frameIndex, maxObjects);

    // Game へ描画（最初に Scene と同期した固定カメラを使う）
    m_viewports.RenderGame(a.cmd, m_sceneRenderer, a.scene, a.frameIndex, maxObjects);
}

void SceneLayer::FeedToUI(EditorContext& ctx, ImGuiLayer* imgui,
    unsigned frameIndex, unsigned sceneSrvBase, unsigned gameSrvBase)
{
    m_viewports.FeedToUI(ctx, imgui, frameIndex, sceneSrvBase, gameSrvBase);
}

void SceneLayer::RequestResize(unsigned wantW, unsigned wantH, float dt)
{
    // 偶数化やヒステリシスは Viewports 側の実装に委譲
    m_viewports.RequestSceneResize(wantW, wantH, dt);
}

void SceneLayer::Shutdown()
{
    // 古い RT を切り離して参照を解放（遅延破棄が必要ならここで回収キューに載せる）
    (void)m_viewports.SceneRT().Detach();
    (void)m_viewports.GameRT().Detach();

    // SceneRenderer の結線を切る（nullptr で無効化）
    m_sceneRenderer.Initialize(nullptr, PipelineSet{}, nullptr);
}

RenderTargetHandles SceneLayer::TakeCarryOverDead() {
    return m_viewports.TakeCarryOverDead();
}
