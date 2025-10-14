// 必要ならプロジェクト設定に合わせて有効化
// #include "pch.h"

#include "Renderer/Viewports.h"

#include <d3d12.h>
#include <cmath>
#include "Renderer/SceneRenderer.h"
#include "Components/CameraComponent.h"
#include "Scene/Scene.h"
#include "Editor/ImGuiLayer.h"
#include "Editor/EditorContext.h"

using namespace DirectX;

void Viewports::Initialize(ID3D12Device* dev, unsigned w, unsigned h)
{
    // Scene
    {
        RenderTargetDesc s{};
        s.width = w; s.height = h;
        s.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        s.depthFormat = DXGI_FORMAT_D32_FLOAT;
        s.clearColor[0] = 0.10f; s.clearColor[1] = 0.10f; s.clearColor[2] = 0.10f; s.clearColor[3] = 1.0f;
        s.clearDepth = 1.0f;
        m_scene.Create(dev, s);
    }
    // Game
    {
        RenderTargetDesc g{};
        g.width = w; g.height = h;
        g.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        g.depthFormat = DXGI_FORMAT_D32_FLOAT;
        g.clearColor[0] = 0.12f; g.clearColor[1] = 0.12f; g.clearColor[2] = 0.12f; g.clearColor[3] = 1.0f;
        g.clearDepth = 1.0f;
        m_game.Create(dev, g);
    }

    m_sceneProjCaptured = false;
    m_gameFrozen = false;
}

void Viewports::FeedToUI(EditorContext& ctx, ImGuiLayer* imgui,
    unsigned frameIndex, unsigned sceneSrvBase, unsigned gameSrvBase)
{
    const unsigned sceneSlot = sceneSrvBase + frameIndex;
    const unsigned gameSlot = gameSrvBase + frameIndex;

    ctx.sceneTexId = m_scene.EnsureImGuiSRV(imgui, sceneSlot);
    ctx.sceneRTWidth = m_scene.Width();
    ctx.sceneRTHeight = m_scene.Height();

    ctx.gameTexId = m_game.EnsureImGuiSRV(imgui, gameSlot);
    ctx.gameRTWidth = m_game.Width();
    ctx.gameRTHeight = m_game.Height();
}

RenderTargetHandles Viewports::HandleSceneResizeIfNeeded(ID3D12Device* dev, unsigned wantW, unsigned wantH)
{
    RenderTargetHandles old{};
    if (wantW == 0 || wantH == 0) return old;
    if (wantW == m_scene.Width() && wantH == m_scene.Height()) return old;

    // 旧RTを切り離し（ComPtr を移動）
    old = m_scene.Detach();

    // 再作成
    RenderTargetDesc s{};
    s.width = wantW; s.height = wantH;
    s.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    s.depthFormat = DXGI_FORMAT_D32_FLOAT;
    s.clearColor[0] = 0.10f; s.clearColor[1] = 0.10f; s.clearColor[2] = 0.10f; s.clearColor[3] = 1.0f;
    s.clearDepth = 1.0f;
    m_scene.Create(dev, s);

    // サイズが変わったので Scene 基準と Game 固定を取り直す
    m_sceneProjCaptured = false;
    m_gameFrozen = false;

    return old;
}

void Viewports::RenderScene(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
    const CameraComponent* cam, const Scene* scene,
    unsigned frameIndex, unsigned maxObjects)
{
    if (!m_scene.Color() || !cam) return;

    if (!m_sceneProjCaptured) {
        XMStoreFloat4x4(&m_sceneProjInit, cam->GetProjectionMatrix());
        m_sceneProjCaptured = true;
    }

    const float aspect = (m_scene.Height() > 0)
        ? float(m_scene.Width()) / float(m_scene.Height())
        : 1.0f;

    const XMMATRIX proj = MakeProjConstHFov(XMLoadFloat4x4(&m_sceneProjInit), aspect);
    CameraMatrices C{ cam->GetViewMatrix(), proj };

    sr.Record(cmd, m_scene, C, scene, /*cbBase=*/0, frameIndex, maxObjects);

    // Game の初回同期（1回だけ）
    if (!m_gameFrozen && m_game.Width() > 0 && m_game.Height() > 0) {
        const float gaspect = float(m_game.Width()) / float(m_game.Height());
        const XMMATRIX gproj = MakeProjConstHFov(XMLoadFloat4x4(&m_sceneProjInit), gaspect);
        XMStoreFloat4x4(&m_gameViewInit, cam->GetViewMatrix());
        XMStoreFloat4x4(&m_gameProjInit, gproj);
        m_gameFrozen = true;
    }
}

void Viewports::RenderGame(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
    const Scene* scene, unsigned frameIndex, unsigned maxObjects)
{
    if (!m_gameFrozen || !m_game.Color()) return;

    CameraMatrices C{
        XMLoadFloat4x4(&m_gameViewInit),
        XMLoadFloat4x4(&m_gameProjInit)
    };
    sr.Record(cmd, m_game, C, scene, /*cbBase=*/maxObjects, frameIndex, maxObjects);
}

XMMATRIX Viewports::MakeProjConstHFov(XMMATRIX P0, float newAspect)
{
    XMFLOAT4X4 M; XMStoreFloat4x4(&M, P0);

    const float A = M._33;  // far/(far - near)
    const float B = M._43;  // -near*far/(far - near)
    const float nearZ = -B / A;
    const float farZ = (A * nearZ) / (A - 1.0f);

    // m00 = 1 / (tan(vFov/2) * aspect) → 1/m00 = tan(vFov/2) * aspect = tan(hFov/2)
    const float tanHalfH = 1.0f / M._11;           // = tan(hFov/2)
    const float tanHalfV = tanHalfH / newAspect;   // = tan(vFov/2)
    const float vFovNew = 2.0f * std::atan(tanHalfV);

    return XMMatrixPerspectiveFovLH(vFovNew, newAspect, nearZ, farZ);
}
