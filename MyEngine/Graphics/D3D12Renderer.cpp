// D3D12Renderer.cpp
#include "D3D12Renderer.h"
#include "d3dx12.h"

#include <stdexcept>
#include <functional>
#include <cmath>
#include <cstring>
#include <algorithm> // std::max

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "imgui_internal.h"

// UI は EditorPanels に移譲
#include "Editor/EditorPanels.h"

using Microsoft::WRL::ComPtr;

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

//==============================================================================
// Ctor / Dtor
//==============================================================================
D3D12Renderer::D3D12Renderer() {}
D3D12Renderer::~D3D12Renderer() { Cleanup(); }

//==============================================================================
// Initialize
//==============================================================================
bool D3D12Renderer::Initialize(HWND hwnd, UINT width, UINT height)
{
    // Device/SwapChain
    m_dev = std::make_unique<DeviceResources>();
    if (!m_dev->Initialize(hwnd, width, height, FrameCount))
        return false;

    ID3D12Device* dev = m_dev->GetDevice();

    // FrameResources：Upload CB（MaxObjects×2 スロット * FrameCount）
    if (!m_frames.Initialize(dev, FrameCount, sizeof(SceneConstantBuffer), MaxObjects * 2))
        return false;

    // CommandList（初回は frame0 の allocator）
    HRESULT hr = dev->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_frames.Get(0).cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&m_cmd));
    if (FAILED(hr)) return false;
    m_cmd->Close();

    // Fence
    hr = dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr)) return false;
    m_nextFence = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) return false;

    // PSO（Lambert）
    if (!BuildLambertPipeline(dev, m_dev->GetRTVFormat(), m_dev->GetDSVFormat(), m_pipe))
        return false;

    // ImGui
    m_imgui = std::make_unique<ImGuiLayer>();
    if (!m_imgui->Initialize(hwnd, dev, m_dev->GetQueue(),
        m_dev->GetRTVFormat(), m_dev->GetDSVFormat(), FrameCount))
        return false;

    // オフスクリーン（RenderTarget）作成
    {
        RenderTargetDesc s{};
        s.width = width;
        s.height = height;
        s.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // 必要なら SRGBへ
        s.depthFormat = DXGI_FORMAT_D32_FLOAT;
        s.clearColor[0] = 0.10f; s.clearColor[1] = 0.10f; s.clearColor[2] = 0.10f; s.clearColor[3] = 1.0f;
        s.clearDepth = 1.0f;
        m_sceneRT.Create(dev, s);
    }
    {
        RenderTargetDesc g{};
        g.width = width;
        g.height = height;
        g.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        g.depthFormat = DXGI_FORMAT_D32_FLOAT;
        g.clearColor[0] = 0.12f; g.clearColor[1] = 0.12f; g.clearColor[2] = 0.12f; g.clearColor[3] = 1.0f;
        g.clearDepth = 1.0f;
        m_gameRT.Create(dev, g);
    }

    ImGui::GetIO().IniFilename = "EditorLayout.ini";

    if (m_Camera)
        m_Camera->SetAspect(static_cast<float>(width) / static_cast<float>(height));

    // 初期状態では未キャプチャ/未フリーズ
    m_sceneProjCaptured = false;
    m_gameCamFrozen = false;

    return true;
}

//==============================================================================
// Render
//==============================================================================
void D3D12Renderer::Render()
{
    const UINT fi = m_dev->GetCurrentBackBufferIndex();
    auto& fr = m_frames.Get(fi);

    // ---- フレーム完了待ち ----
    if (fr.fenceValue != 0 && m_fence->GetCompletedValue() < fr.fenceValue) {
        m_fence->SetEventOnCompletion(fr.fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // ---- SceneRT の保留リサイズ（遅延破棄版：このフレームの Signal 後に捨てる） ----
    RenderTargetHandles deadScene{}; // このフレーム末にキューへ積む
    if (m_pendingSceneRTW && (m_pendingSceneRTW != m_sceneRT.Width() || m_pendingSceneRTH != m_sceneRT.Height())) {
        // ★ SRVの上書き競合を避けるため、直前までに送ったコマンドの完了を待つ
        const UINT64 lastSubmitted = (m_nextFence > 0) ? (m_nextFence - 1) : 0;
        if (lastSubmitted && m_fence->GetCompletedValue() < lastSubmitted) {
            m_fence->SetEventOnCompletion(lastSubmitted, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }

        // 旧RTをデタッチ（ComPtr群を抜き取るだけでGPU待ちはしない）
        deadScene = m_sceneRT.Detach();

        // 新規作成
        RenderTargetDesc s{};
        s.width = m_pendingSceneRTW;  s.height = m_pendingSceneRTH;
        s.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        s.depthFormat = DXGI_FORMAT_D32_FLOAT;
        s.clearColor[0] = 0.10f; s.clearColor[1] = 0.10f; s.clearColor[2] = 0.10f; s.clearColor[3] = 1.0f;
        s.clearDepth = 1.0f;

        m_sceneRT.Create(m_dev->GetDevice(), s);

        // このフレームのImGuiセット時にフレーム別スロットでSRVを保証する
        m_pendingSceneRTW = m_pendingSceneRTH = 0;

        // ★ サイズが変わったので次フレームで Scene を取り直し & Game を再同期
        m_sceneProjCaptured = false;
        m_gameCamFrozen = false;
    }

    // ---- Reset ----
    fr.cmdAlloc->Reset();
    m_cmd->Reset(fr.cmdAlloc.Get(), m_pipe.pso.Get());

    // 水平FOV固定の再投影ヘルパ
    auto MakeProjConstHFov = [](DirectX::XMMATRIX P0, float newAspect)
        {
            using namespace DirectX;
            XMFLOAT4X4 M; XMStoreFloat4x4(&M, P0);

            // XMMatrixPerspectiveFovLH の係数から near/far 復元
            const float A = M._33;  // = far/(far - near)
            const float B = M._43;  // = -near*far/(far - near)
            const float nearZ = -B / A;
            const float farZ = (A * nearZ) / (A - 1.0f);

            // m00 = 1 / (tan(vFov/2) * aspect) → 1/m00 = tan(vFov/2) * aspect = tan(hFov/2)
            const float tanHalfH = 1.0f / M._11;

            const float tanHalfV_new = tanHalfH / newAspect;
            const float vFovNew = 2.0f * std::atan(tanHalfV_new);

            return XMMatrixPerspectiveFovLH(vFovNew, newAspect, nearZ, farZ);
        };

    // =====================================================================
    // ① Scene 用オフスクリーンへ描画（水平FOV固定でアスペクト追従）
    // =====================================================================
    if (m_sceneRT.Color() && m_Camera) {
        using namespace DirectX;

        // 初回に Scene の“基準射影”をキャプチャ（HFOVの基準）
        if (!m_sceneProjCaptured) {
            XMStoreFloat4x4(&m_sceneProjInit, m_Camera->GetProjectionMatrix());
            m_sceneProjCaptured = true;
        }

        const float sceneAspect = (m_sceneRT.Height() > 0)
            ? (float)m_sceneRT.Width() / (float)m_sceneRT.Height()
            : 1.0f;

        const XMMATRIX projSceneHFOV =
            MakeProjConstHFov(XMLoadFloat4x4(&m_sceneProjInit), sceneAspect);

        CameraMatrices camScene{ m_Camera->GetViewMatrix(), projSceneHFOV };
        m_sceneRenderer.Record(m_cmd.Get(),m_sceneRT, camScene, m_CurrentScene.get(), /*cbBase=*/0, /*frameIndex=*/fi, /*maxObjects=*/MaxObjects);

        // ===== Game を一度だけ Scene に同期して固定 =====
        if (!m_gameCamFrozen &&
            m_pendingSceneRTW == 0 &&
            m_sceneRT.Width() > 0 && m_sceneRT.Height() > 0 &&
            m_gameRT.Width() > 0 && m_gameRT.Height() > 0)
        {
            const float gameAspect = (float)m_gameRT.Width() / (float)m_gameRT.Height();
            const XMMATRIX projGameHFOV =
                MakeProjConstHFov(XMLoadFloat4x4(&m_sceneProjInit), gameAspect);

            XMStoreFloat4x4(&m_gameViewInit, m_Camera->GetViewMatrix()); // View は Scene と同じ
            XMStoreFloat4x4(&m_gameProjInit, projGameHFOV);

            m_gameFrozenAspect = gameAspect;
            m_gameCamFrozen = true;
        }
    }

    // =====================================================================
    // 1.5) Game 用オフスクリーンへ “固定カメラ” で描画
    // =====================================================================
    if (m_gameRT.Color() && m_gameCamFrozen) {
        using namespace DirectX;
        CameraMatrices cam{ XMLoadFloat4x4(&m_gameViewInit), XMLoadFloat4x4(&m_gameProjInit) };
        m_sceneRenderer.Record(m_cmd.Get(), m_gameRT, cam, m_CurrentScene.get(), /*cbBase=*/MaxObjects, /*frameIndex=*/fi, /*maxObjects=*/MaxObjects);
    }

    // =====================================================================
    // ② バックバッファに UI（ImGui）
    // =====================================================================
    ID3D12Resource* bb = m_dev->GetBackBuffer(fi);
    auto toRT_bb = CD3DX12_RESOURCE_BARRIER::Transition(
        bb, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_cmd->ResourceBarrier(1, &toRT_bb);

    auto rtv = m_dev->GetRTVHandle(fi);
    m_cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    const float clearBB[4] = { 0.2f, 0.2f, 0.4f, 1.0f };
    m_cmd->ClearRenderTargetView(rtv, clearBB, 0, nullptr);

    D3D12_VIEWPORT vpBB{ 0.f, 0.f, (float)m_dev->GetWidth(), (float)m_dev->GetHeight(), 0.f, 1.f };
    D3D12_RECT     scBB{ 0, 0, (LONG)m_dev->GetWidth(), (LONG)m_dev->GetHeight() };
    m_cmd->RSSetViewports(1, &vpBB);
    m_cmd->RSSetScissorRects(1, &scBB);

    {
        static bool s_resetLayout = false;
        static bool s_autoRelayout = false;

        EditorContext ctx{};
        ctx.pEnableEditor = &m_IsEditor;
        ctx.rtWidth = m_dev->GetWidth();
        ctx.rtHeight = m_dev->GetHeight();
        ctx.fps = ImGui::GetIO().Framerate;

        ctx.pRequestResetLayout = &s_resetLayout;
        ctx.pAutoRelayout = &s_autoRelayout;

        // RenderTarget から毎フレーム SRV を保証（フレーム別スロット）
        const UINT sceneSrvSlot = kSceneSrvBase + fi;
        const UINT gameSrvSlot = kGameSrvBase + fi;

        ctx.sceneTexId = m_sceneRT.EnsureImGuiSRV(m_imgui.get(), sceneSrvSlot);
        ctx.sceneRTWidth = m_sceneRT.Width();
        ctx.sceneRTHeight = m_sceneRT.Height();

        ctx.gameTexId = m_gameRT.EnsureImGuiSRV(m_imgui.get(), gameSrvSlot);
        ctx.gameRTWidth = m_gameRT.Width();
        ctx.gameRTHeight = m_gameRT.Height();

        // UI は EditorPanels に委譲（Renderer から排除）
        ctx.DrawInspector = [&]() { EditorPanels::DrawInspector(m_Selected); };
        ctx.DrawHierarchy = [&]() { EditorPanels::DrawHierarchy(m_CurrentScene.get(), m_Selected); };

        m_imgui->NewFrame();
        m_imgui->BuildDockAndWindows(ctx);

        // 次フレーム用に Scene RT のリサイズ要求（即時追従・常に実行）
        {
            auto snap_even = [](UINT v) { return std::max(1u, (v + 1u) & ~1u); };
            UINT wantW = snap_even((UINT)std::lroundf(ctx.sceneViewportSize.x));
            UINT wantH = snap_even((UINT)std::lroundf(ctx.sceneViewportSize.y));

            if (wantW > 0 && wantH > 0) {
                if (wantW != m_sceneRT.Width() || wantH != m_sceneRT.Height()) {
                    // ★ サイズ変更時は Game の固定を解除して次フレームで再同期
                    m_gameCamFrozen = false;
                    m_sceneProjCaptured = false; // Scene の基準射影も取り直す
                    RequestSceneRTResize(wantW, wantH);
                }
            }
            m_wantSceneW = wantW;
            m_wantSceneH = wantH;
            m_sceneSizeStable = 0;
        }

        m_imgui->Render(m_cmd.Get());
    }

    // ---- RT → PRESENT ----
    auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        bb, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_cmd->ResourceBarrier(1, &toPresent);

    // ---- Submit & Present & Signal ----
    m_cmd->Close();
    ID3D12CommandList* lists[] = { m_cmd.Get() };
    m_dev->GetQueue()->ExecuteCommandLists(1, lists);
    m_dev->Present(1);

    const UINT64 sig = m_nextFence++;
    m_dev->GetQueue()->Signal(m_fence.Get(), sig);
    fr.fenceValue = sig;

    // ★ このフレームが完了したら旧RTを捨てる
    if (deadScene.color || deadScene.depth || deadScene.rtvHeap || deadScene.dsvHeap) {
        EnqueueRenderTarget(m_garbage, sig, std::move(deadScene));
    }

    // ★ 遅延破棄の回収（完了分を捨てる）
    m_garbage.Collect(m_fence.Get());

    ++m_frameCount;
}

//==============================================================================
// Resize
//==============================================================================
// D3D12Renderer::Resize
void D3D12Renderer::Resize(UINT width, UINT height) noexcept
{
    if (width == 0 || height == 0) return;

    // 全フレーム待機（スワップチェイン再作成のため）
    for (UINT i = 0; i < m_frames.GetCount(); ++i) {
        auto& fr = m_frames.Get(i);
        if (fr.fenceValue != 0 && m_fence->GetCompletedValue() < fr.fenceValue) {
            m_fence->SetEventOnCompletion(fr.fenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }

    m_dev->Resize(width, height);

    // ★ここは作り直さない（SceneRT は ImGui のビューポートに追従）
    // RequestSceneRTResize(width, height);

    // 再同期フラグだけ落としておくと安全
    m_sceneSizeStable = 0;
    m_wantSceneW = m_wantSceneH = 0;
    m_sceneProjCaptured = false;
    m_gameCamFrozen = false;

    if (m_Camera)
        m_Camera->SetAspect(static_cast<float>(width) / static_cast<float>(height));
}

//==============================================================================
// Cleanup
//==============================================================================
void D3D12Renderer::Cleanup()
{
    if (!m_dev) return;

    // 全フレーム待機
    for (UINT i = 0; i < m_frames.GetCount(); ++i) {
        auto& fr = m_frames.Get(i);
        if (fr.fenceValue != 0 && m_fence->GetCompletedValue() < fr.fenceValue) {
            m_fence->SetEventOnCompletion(fr.fenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }

    if (m_imgui) { m_imgui->Shutdown(); m_imgui.reset(); }

    ReleaseSceneResources();

    // 念のため完全待機してから遅延破棄を全回収
    WaitForGPU();
    m_garbage.FlushAll();

    m_sceneRT.Release();
    m_gameRT.Release();

    m_frames.Destroy();

    if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
    m_fence.Reset();

    m_cmd.Reset();
    m_dev.reset();
}

//==============================================================================
// Utilities
//==============================================================================
void D3D12Renderer::WaitForGPU() noexcept
{
    if (!m_dev || !m_fence) return;
    const UINT64 v = ++m_nextFence;
    if (FAILED(m_dev->GetQueue()->Signal(m_fence.Get(), v))) return;
    if (m_fence->GetCompletedValue() < v) {
        if (SUCCEEDED(m_fence->SetEventOnCompletion(v, m_fenceEvent)))
            WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

void D3D12Renderer::DrawMesh(MeshRendererComponent* mr)
{
    if (!mr) return;
    m_cmd->IASetVertexBuffers(0, 1, &mr->VertexBufferView);
    m_cmd->IASetIndexBuffer(&mr->IndexBufferView);
    m_cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_cmd->DrawIndexedInstanced(mr->IndexCount, 1, 0, 0, 0);
}

bool D3D12Renderer::CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> mr)
{
    if (!mr) return false;
    const MeshData& md = mr->GetMeshData();
    if (md.Vertices.empty() || md.Indices.empty()) return false;

    ID3D12Device* dev = m_dev->GetDevice();
    HRESULT hr;
    D3D12_HEAP_PROPERTIES heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // VB
    const UINT vbSize = static_cast<UINT>(md.Vertices.size() * sizeof(Vertex));
    D3D12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);
    hr = dev->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &vbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mr->VertexBuffer));
    if (FAILED(hr)) return false;
    {
        UINT8* dst = nullptr; CD3DX12_RANGE rr(0, 0);
        hr = mr->VertexBuffer->Map(0, &rr, reinterpret_cast<void**>(&dst));
        if (FAILED(hr)) return false;
        std::memcpy(dst, md.Vertices.data(), vbSize);
        mr->VertexBuffer->Unmap(0, nullptr);
    }
    mr->VertexBufferView.BufferLocation = mr->VertexBuffer->GetGPUVirtualAddress();
    mr->VertexBufferView.StrideInBytes = sizeof(Vertex);
    mr->VertexBufferView.SizeInBytes = vbSize;

    // IB
    const UINT ibSize = static_cast<UINT>(md.Indices.size() * sizeof(uint32_t));
    D3D12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);
    hr = dev->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &ibDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mr->IndexBuffer));
    if (FAILED(hr)) return false;
    {
        UINT8* dst = nullptr; CD3DX12_RANGE rr(0, 0);
        hr = mr->IndexBuffer->Map(0, &rr, reinterpret_cast<void**>(&dst));
        if (FAILED(hr)) return false;
        std::memcpy(dst, md.Indices.data(), ibSize);
        mr->IndexBuffer->Unmap(0, nullptr);
    }
    mr->IndexBufferView.BufferLocation = mr->IndexBuffer->GetGPUVirtualAddress();
    mr->IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
    mr->IndexBufferView.SizeInBytes = ibSize;

    mr->IndexCount = static_cast<UINT>(md.Indices.size());
    return true;
}

void D3D12Renderer::ReleaseSceneResources()
{
    if (!m_CurrentScene) return;
    for (const auto& root : m_CurrentScene->GetRootGameObjects()) {
        std::function<void(std::shared_ptr<GameObject>)> walk =
            [&](std::shared_ptr<GameObject> go)
            {
                if (!go) return;
                if (auto mr = go->GetComponent<MeshRendererComponent>()) {
                    mr->IndexBuffer.Reset();
                    mr->VertexBuffer.Reset();
                }
                for (auto& ch : go->GetChildren()) walk(ch);
            };
        walk(root);
    }
    m_Camera.reset();
    m_CurrentScene.reset();
}

////==============================================================================
//// 共通描画パス（1カメラ→1RT）
////==============================================================================
//void D3D12Renderer::DrawSceneToRT(RenderTarget& rt, const CameraMatrices& cam, UINT cbBase)
//{
//    if (!rt.Color()) return;
//
//    // RT セット
//    rt.TransitionToRT(m_cmd.Get());
//    rt.Bind(m_cmd.Get());
//    rt.Clear(m_cmd.Get());
//
//    // VP/SC/RS
//    D3D12_VIEWPORT vp{ 0.f, 0.f, (float)rt.Width(), (float)rt.Height(), 0.f, 1.f };
//    D3D12_RECT     sc{ 0, 0, (LONG)rt.Width(), (LONG)rt.Height() };
//    m_cmd->RSSetViewports(1, &vp);
//    m_cmd->RSSetScissorRects(1, &sc);
//    m_cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//    m_cmd->SetGraphicsRootSignature(m_pipe.root.Get());
//
//    if (m_CurrentScene) {
//        auto& fr = m_frames.Get(m_dev->GetCurrentBackBufferIndex());
//        UINT8* cbCPU = fr.cpu;
//        D3D12_GPU_VIRTUAL_ADDRESS cbGPU = fr.resource->GetGPUVirtualAddress();
//        const UINT cbStride = m_frames.GetCBStride();
//        UINT slot = 0;
//
//        using namespace DirectX;
//        std::function<void(std::shared_ptr<GameObject>)> draw =
//            [&](std::shared_ptr<GameObject> go)
//            {
//                if (!go || slot >= MaxObjects) return;
//                auto mr = go->GetComponent<MeshRendererComponent>();
//                if (mr && mr->VertexBuffer && mr->IndexBuffer && mr->IndexCount > 0)
//                {
//                    XMMATRIX world = go->Transform->GetWorldMatrix();
//                    XMMATRIX mvp = world * cam.view * cam.proj;
//
//                    XMVECTOR det;
//                    XMMATRIX inv = XMMatrixInverse(&det, world);
//                    float detScalar = XMVectorGetX(det);
//                    if (!std::isfinite(detScalar) || std::fabs(detScalar) < 1e-8f) inv = XMMatrixIdentity();
//                    XMMATRIX worldIT = XMMatrixTranspose(inv);
//
//                    SceneConstantBuffer cb{};
//                    XMStoreFloat4x4(&cb.mvp, mvp);
//                    XMStoreFloat4x4(&cb.world, world);
//                    XMStoreFloat4x4(&cb.worldIT, worldIT);
//                    XMStoreFloat3(&cb.lightDir, XMVector3Normalize(XMVectorSet(0.0f, -1.0f, -1.0f, 0.0f)));
//                    cb.pad = 0.0f;
//
//                    const UINT dst = cbBase + slot;
//                    std::memcpy(cbCPU + (UINT64)dst * cbStride, &cb, sizeof(cb));
//                    m_cmd->SetGraphicsRootConstantBufferView(0, cbGPU + (UINT64)dst * cbStride);
//
//                    m_cmd->IASetVertexBuffers(0, 1, &mr->VertexBufferView);
//                    m_cmd->IASetIndexBuffer(&mr->IndexBufferView);
//                    m_cmd->DrawIndexedInstanced(mr->IndexCount, 1, 0, 0, 0);
//                    ++slot;
//                }
//                for (auto& ch : go->GetChildren()) draw(ch);
//            };
//
//        for (auto& root : m_CurrentScene->GetRootGameObjects()) draw(root);
//    }
//
//    rt.TransitionToSRV(m_cmd.Get());
//}

