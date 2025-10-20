// Renderer/Viewports.cpp
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Renderer/Viewports.h"

#include <d3d12.h>
#include <cmath>
#include <algorithm>
#include <cstdlib> // std::abs(int)
#include "Renderer/SceneRenderer.h"
#include "Components/CameraComponent.h"
#include "Scene/Scene.h"
#include "Editor/ImGuiLayer.h"
#include "Editor/EditorContext.h"

using namespace DirectX;

/*
    Viewports
    ----------------------------------------------------------------------------
    役割：
      - Scene 用 / Game 用のオフスクリーン RenderTarget を管理（生成/再生成/解放移譲）
      - ImGui へ表示するための SRV (ImTextureID) を供給
      - UI 側の「希望サイズ」をヒステリシス付きで受け取り、安定後に RT を再作成
      - Scene と Game の投影・ビューの初期同期（Game は固定カメラ、Scene は都度）

    重要なポリシー：
      - リサイズは「即時」ではなく「安定時間を超えたら確定」(デバウンス）
      - 再作成時、古い RT は Detach して呼び出し側の GpuGarbageQueue へ（遅延破棄）
      - SRV は ImGuiLayer 側のヒープに作る（DX12 の ImTextureID = GPU可視SRVハンドル）
      - Scene は “横 FOV 固定” でアスペクト追従（MakeProjConstHFov）

    よくある落とし穴：
      - ImGui の表示領域が高速に揺れる → 毎フレで RT 再生成 → 破棄渋滞/カクつき
        → StepSnap + 安定時間閾値で緩和
      - Detach した古い RT を即破棄してしまい GPU 使用中クラッシュ
        → 必ず GpuGarbageQueue に fence 値とともに登録（呼び出し側の責務）
*/

// =============================================================================
// Game 用：投影再構築ヘルパー（“Game の基準投影”から再計算する）
//   - 透視: 水平FOV一定（Const H-FOV）で newAspect に合わせて縦FOVを調整
//   - 直交: 横幅一定（Const Width）で newAspect に合わせて高さを調整
// =============================================================================

// 透視/直交の簡易判定（D3D 標準形：透視は _34 ≠ 0、直交は 0）
static inline bool VP_IsPerspective(const XMFLOAT4X4& M) {
    return std::fabs(M._34) > 1e-6f;
}

// 透視：水平FOV一定で newAspect に合わせて再構築
static inline XMMATRIX VP_RebuildPerspConstHFov(const XMFLOAT4X4& base, float newAspect)
{
    // near/far 抽出（左手系透視）
    const float A = base._33;  // far/(far - near)
    const float B = base._43;  // -near*far/(far - near)
    const float nearZ = -B / A;
    const float farZ = (A * nearZ) / (A - 1.0f);

    // tan(hFov/2) = 1 / m00,  tan(vFov/2) = tan(hFov/2) / aspect
    const float tanHalfH = 1.0f / base._11;
    const float tanHalfV = tanHalfH / std::max(newAspect, 1e-6f);
    const float vFovNew = 2.0f * std::atan(tanHalfV);

    return XMMatrixPerspectiveFovLH(vFovNew, newAspect, nearZ, farZ);
}

// 直交：横幅一定で newAspect に合わせて縦幅を再構築
static inline XMMATRIX VP_RebuildOrthoConstWidth(const XMFLOAT4X4& base, float newAspect)
{
    // 直交の標準形：m00 = 2/width, m11 = 2/height
    const float width = 2.0f / base._11;
    const float height0 = 2.0f / base._22;

    const float height = (newAspect > 1e-6f) ? (width / newAspect) : height0;

    // 近遠：m22 = 1/(far-near), m43 = -near/(far-near)
    const float m22 = base._33;
    const float m43 = base._43;
    const float nearZ = -m43 / m22;
    const float farZ = nearZ + 1.0f / m22;

    const float l = -width * 0.5f;
    const float r = width * 0.5f;
    const float b = -height * 0.5f;
    const float t = height * 0.5f;

    return XMMatrixOrthographicOffCenterLH(l, r, b, t, nearZ, farZ);
}

// ----------------------------------------------------------------------------
// 初期化：Scene/Game の RT をウィンドウ初期サイズで作成
//   初期を十分大きめに作っておくと、拡大時の一時的なぼやけを避けやすい
// ----------------------------------------------------------------------------
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

    // Scene 初回はカメラの投影を「キャプチャ前」
    m_sceneProjCaptured = false;

    // Game は「Scene と同期がまだ」状態
    m_gameFrozen = false;

    // リサイズデバウンスの内部状態をリセット
    m_desiredW = m_desiredH = 0;
    m_desiredStableTime = 0.0f;
    m_pendingW = m_pendingH = 0;
}

// ----------------------------------------------------------------------------
// ImGui へ渡す情報（テクスチャIDと実RTサイズ）を EditorContext に転記
//   - sceneSrvBase/gameSrvBase + frameIndex で SRV スロット衝突を回避
// ----------------------------------------------------------------------------
void Viewports::FeedToUI(EditorContext& ctx, ImGuiLayer* imgui,
    unsigned frameIndex, unsigned sceneSrvBase, unsigned gameSrvBase)
{
    const unsigned sceneSlot = sceneSrvBase + frameIndex;
    const unsigned gameSlot = gameSrvBase + frameIndex;

    // Scene
    ctx.sceneTexId = m_scene.EnsureImGuiSRV(imgui, sceneSlot);
    ctx.sceneRTWidth = m_scene.Width();
    ctx.sceneRTHeight = m_scene.Height();

    // Game
    ctx.gameTexId = m_game.EnsureImGuiSRV(imgui, gameSlot);
    ctx.gameRTWidth = m_game.Width();
    ctx.gameRTHeight = m_game.Height();
}

// ----------------------------------------------------------------------------
// UI 側から渡される「Scene ウィンドウの利用可能サイズ」を受け取り、
//  - StepSnap（小刻み変化抑制）
//  - 偶数化（テクスチャ互換・ピクセルずれ抑制）
//  - 安定時間が一定を超えたら pending に確定
// ----------------------------------------------------------------------------
void Viewports::RequestSceneResize(unsigned w, unsigned h, float dt)
{
    if (w == 0 || h == 0) return;

    // 1) ステップスナップ（ドラッグ中の微小変化で再生成しない）
    constexpr unsigned kStep = 8;
    auto StepSnap = [](unsigned v, unsigned step) {
        if (step == 0) return v;
        return (unsigned)(((v + step / 2) / step) * step);
        };
    auto Even = [](unsigned v) { return (v + 1u) & ~1u; }; // 偶数化

    unsigned wantW = Even(StepSnap(w, kStep));
    unsigned wantH = Even(StepSnap(h, kStep));

    // 2) 目標が変わったら安定時間をリセット
    if (wantW != m_desiredW || wantH != m_desiredH) {
        m_desiredW = wantW;
        m_desiredH = wantH;
        m_desiredStableTime = 0.0f;
        return;
    }

    // 3) 同じ目標が継続した時間を積算
    if (dt > 0.0f) m_desiredStableTime += dt;

    // 4) 一定時間ブレなかったら pending に確定
    constexpr float kStableSec = 0.12f; // 0.08～0.20 で調整可
    if (m_desiredStableTime >= kStableSec) {
        // 既に同サイズなら noop
        if (m_desiredW != m_scene.Width() || m_desiredH != m_scene.Height()) {
            m_pendingW = m_desiredW;
            m_pendingH = m_desiredH;
        }
        // 次回に備えてリセット
        m_desiredW = m_desiredH = 0;
        m_desiredStableTime = 0.0f;
    }
}

// ----------------------------------------------------------------------------
// ペンディングされたリサイズがあれば適用
//   - 新しい RT を作成
//   - 古い RT は Detach して 1 個だけ戻す（2 個目は m_carryOverDead にプール）
//   - 呼び出し側（Renderer）は戻り値を今フレームの EndFrame で遅延破棄登録
//   - ★Game の向き（View）は保持し、投影（Proj）のみ“Game の基準”から再構築★
// ----------------------------------------------------------------------------
RenderTargetHandles Viewports::ApplyPendingResizeIfNeeded(ID3D12Device* dev)
{
    RenderTargetHandles toDispose{}; // 今フレームの EndFrame に渡す分（最大1個）
    if (m_pendingW == 0 || m_pendingH == 0) return toDispose;

    const bool needScene = (m_pendingW != m_scene.Width() || m_pendingH != m_scene.Height());
    const bool needGame = (m_pendingW != m_game.Width() || m_pendingH != m_game.Height());

    // 旧を必ず Detach（即 Release はしない）※GPU で使っている可能性があるため
    if (needScene) {
        RenderTargetHandles old = m_scene.Detach();
        // 1個目は今フレームへ、2個目は持ち越し
        if (!toDispose.color && !toDispose.depth && !toDispose.rtvHeap && !toDispose.dsvHeap) {
            toDispose = std::move(old);
        }
        else if (!m_carryOverDead.color && !m_carryOverDead.depth
            && !m_carryOverDead.rtvHeap && !m_carryOverDead.dsvHeap) {
            m_carryOverDead = std::move(old);
        }
    }
    if (needGame) {
        RenderTargetHandles old = m_game.Detach();
        if (!toDispose.color && !toDispose.depth && !toDispose.rtvHeap && !toDispose.dsvHeap) {
            toDispose = std::move(old);
        }
        else if (!m_carryOverDead.color && !m_carryOverDead.depth
            && !m_carryOverDead.rtvHeap && !m_carryOverDead.dsvHeap) {
            m_carryOverDead = std::move(old);
        }
    }

    // 新規作成（Scene / Game とも）
    if (needScene) {
        RenderTargetDesc s{};
        s.width = m_pendingW; s.height = m_pendingH;
        s.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        s.depthFormat = DXGI_FORMAT_D32_FLOAT;
        s.clearColor[0] = 0.10f; s.clearColor[1] = 0.10f; s.clearColor[2] = 0.10f; s.clearColor[3] = 1.0f;
        s.clearDepth = 1.0f;
        m_scene.Create(dev, s);
        m_sceneProjCaptured = false; // 投影基準を取り直す（アスペクト変化に対応）
    }
    if (needGame) {
        RenderTargetDesc g{};
        g.width = m_pendingW; g.height = m_pendingH;
        g.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        g.depthFormat = DXGI_FORMAT_D32_FLOAT;
        g.clearColor[0] = 0.12f; g.clearColor[1] = 0.12f; g.clearColor[2] = 0.12f; g.clearColor[3] = 1.0f;
        g.clearDepth = 1.0f;
        m_game.Create(dev, g);

        // ★向き(View)は維持。投影(Proj)のみ“Game の基準投影”から再構築する。
        if (m_game.Width() > 0 && m_game.Height() > 0) {
            const float gaspect = float(m_game.Width()) / float(m_game.Height());

            // 再構築の基準となる投影行列
            XMFLOAT4X4 base{};
            if (m_gameFrozen) {
                // 既に初回同期済み → Game 自身の基準投影を使う（←重要：Scene に引っ張らない）
                base = m_gameProjInit;
            }
            else if (m_sceneProjCaptured) {
                // まだ同期前なら暫定的に Scene の投影を使う（初回のみ）
                base = m_sceneProjInit;
            }
            else {
                // 最終フォールバック値（60度, 0.1-1000）
                XMStoreFloat4x4(&base,
                    XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), gaspect, 0.1f, 1000.0f));
            }

            XMMATRIX P = VP_IsPerspective(base)
                ? VP_RebuildPerspConstHFov(base, gaspect)
                : VP_RebuildOrthoConstWidth(base, gaspect);
            XMStoreFloat4x4(&m_gameProjInit, P);
        }
        // m_gameFrozen は触らない（= View は固定のまま）
    }

    // 消費済み
    m_pendingW = m_pendingH = 0;
    return toDispose; // ★ EndFrame に渡すのは最大1個（2個目は内部持ち越し）
}

// ----------------------------------------------------------------------------
// Scene 描画（横 FOV 固定でアスペクト追従）
//   - 初回にカメラの投影をキャプチャ（基準）
//   - 現在の RT アスペクトに合わせて縦 FOV を再計算
//   - 描画後、Game の初回同期（固定カメラの仕込み）
// ----------------------------------------------------------------------------
void Viewports::RenderScene(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
    const CameraComponent* cam, const Scene* scene,
    unsigned frameIndex, unsigned maxObjects)
{
    if (!m_scene.Color() || !cam) return;

    // 投影の基準（初回のみキャプチャ）
    if (!m_sceneProjCaptured) {
        XMStoreFloat4x4(&m_sceneProjInit, cam->GetProjectionMatrix());
        m_sceneProjCaptured = true;
    }

    // 現在のアスペクト
    const float aspect = (m_scene.Height() > 0)
        ? float(m_scene.Width()) / float(m_scene.Height())
        : 1.0f;

    // 横FOV固定で縦FOVを再計算した投影行列
    const XMMATRIX proj = MakeProjConstHFov(XMLoadFloat4x4(&m_sceneProjInit), aspect);
    CameraMatrices C{ cam->GetViewMatrix(), proj };

    // Scene をレンダリング（cbBase=0..maxObjects-1）
    sr.Record(cmd, m_scene, C, scene, /*cbBase=*/0, frameIndex, maxObjects);

    // --- Game の初回同期（1回だけ） ---
    if (!m_gameFrozen && m_game.Width() > 0 && m_game.Height() > 0) {
        const float gaspect = float(m_game.Width()) / float(m_game.Height());
        const XMMATRIX gproj = MakeProjConstHFov(XMLoadFloat4x4(&m_sceneProjInit), gaspect);
        XMStoreFloat4x4(&m_gameViewInit, cam->GetViewMatrix());
        XMStoreFloat4x4(&m_gameProjInit, gproj);
        m_gameFrozen = true; // 以降 Game は固定カメラで描く（View は固定、Proj はアスペクト追従）
    }
}

// ----------------------------------------------------------------------------
// Game 描画（固定カメラ：最初に Scene と同期した View/Proj を使用）
//   - cbBase=maxObjects..(2*maxObjects-1) を使う前提で SceneRenderer に渡す
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// MakeProjConstHFov
// 目的：元の投影 P0 から near/far と “横FOV” を保ったまま、新しいアスペクト newAspect の
//       縦FOV を再計算した透視投影行列を返す。
// 数学：m00 = 1 / (tan(vFov/2) * aspect) → 1/m00 = tan(vFov/2) * aspect = tan(hFov/2)
//       よって tan(hFov/2) = 1/m00。newAspect が変わったら tan(vFov/2)=tan(hFov/2)/newAspect。
// ----------------------------------------------------------------------------
XMMATRIX Viewports::MakeProjConstHFov(XMMATRIX P0, float newAspect)
{
    XMFLOAT4X4 M; XMStoreFloat4x4(&M, P0);

    // 近遠を元行列から抽出（左手系透視投影の一般形）
    const float A = M._33;  // far/(far - near)
    const float B = M._43;  // -near*far/(far - near)
    const float nearZ = -B / A;
    const float farZ = (A * nearZ) / (A - 1.0f);

    // 横 FOV を変えずに縦 FOV を再計算
    const float tanHalfH = 1.0f / M._11;         // = tan(hFov/2)
    const float tanHalfV = tanHalfH / newAspect; // = tan(vFov/2)
    const float vFovNew = 2.0f * std::atan(tanHalfV);

    return XMMatrixPerspectiveFovLH(vFovNew, newAspect, nearZ, farZ);
}

// ----------------------------------------------------------------------------
// アクセス系：RT が存在しない場合は 0 を返す（安全側）
// ----------------------------------------------------------------------------
unsigned Viewports::SceneWidth()  const noexcept { return m_scene.Color() ? m_scene.Width() : 0; }
unsigned Viewports::SceneHeight() const noexcept { return m_scene.Color() ? m_scene.Height() : 0; }
unsigned Viewports::GameWidth()   const noexcept { return m_game.Color() ? m_game.Width() : 0; }
unsigned Viewports::GameHeight()  const noexcept { return m_game.Color() ? m_game.Height() : 0; }
