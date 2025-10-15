// Renderer/SceneLayer.h
#pragma once
#include <d3d12.h>
#include "Renderer/Viewports.h"
#include "Renderer/SceneRenderer.h"
#include "Editor/EditorContext.h"
#include "Editor/ImGuiLayer.h"

struct SceneLayerBeginArgs {
    ID3D12Device* device = nullptr;
    ID3D12GraphicsCommandList* cmd = nullptr;
    unsigned frameIndex = 0;
    class Scene* scene = nullptr;
    class CameraComponent* camera = nullptr;
};

class SceneLayer {
public:
    // 初期サイズを渡せる版（ぼやけ防止）
    void Initialize(ID3D12Device* dev, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
        FrameResources* frames, const PipelineSet& pipe,
        unsigned initW, unsigned initH);

    // 旧5引数版も残す（互換）
    void Initialize(ID3D12Device* dev, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
        FrameResources* frames, const PipelineSet& pipe)
    {
        Initialize(dev, rtvFmt, dsvFmt, frames, pipe, 64, 64);
    }

    // フレーム先頭で：ペンディング・リサイズを適用
    RenderTargetHandles BeginFrame(ID3D12Device* dev);

    // Scene/Game をオフスクリーンに描く
    void Record(const SceneLayerBeginArgs& args, unsigned maxObjects);

    // ImGui へ SRV を供給（既存の呼び出しをそのまま置換できる）
    void FeedToUI(EditorContext& ctx, ImGuiLayer* imgui,
        unsigned frameIndex, unsigned sceneSrvBase, unsigned gameSrvBase);

    // UI からの希望サイズを記録（デバウンス対応）
    void RequestResize(unsigned wantW, unsigned wantH, float dt);

    // ★追加：明示破棄（Renderer::Cleanup から呼ばれる）
    void Shutdown();

    RenderTargetHandles TakeCarryOverDead();

    // アクセス系（必要なら）
    unsigned SceneWidth()  const { return m_viewports.SceneWidth(); }
    unsigned SceneHeight() const { return m_viewports.SceneHeight(); }
    unsigned GameWidth()   const { return m_viewports.GameWidth(); }
    unsigned GameHeight()  const { return m_viewports.GameHeight(); }

private:
    Viewports     m_viewports;
    SceneRenderer m_sceneRenderer;
};
