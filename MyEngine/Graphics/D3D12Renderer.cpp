// PCH を使っているなら
// #include "pch.h"

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

// RT/遅延破棄ハンドル型（ローカルで使うだけなので cpp にて include）
#include "Core/RenderTarget.h"

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

    // Fence
    HRESULT hr = dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr)) return false;
    m_nextFence = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) return false;

    // PSO（Lambert）
    if (!BuildLambertPipeline(dev, m_dev->GetRTVFormat(), m_dev->GetDSVFormat(), m_pipe))
        return false;

    // SceneRenderer を結線
    m_sceneRenderer.Initialize(dev, m_pipe, &m_frames);

    // ImGui
    m_imgui = std::make_unique<ImGuiLayer>();
    if (!m_imgui->Initialize(hwnd, dev, m_dev->GetQueue(),
        m_dev->GetRTVFormat(), m_dev->GetDSVFormat(), FrameCount))
        return false;

    // Viewports を初期化（Scene/Game RT 内部管理）
    m_viewports.Initialize(dev, width, height);

    // フレームスケジューラ（フレーム待ち/Present/Signal/遅延破棄）
    m_scheduler.Initialize(m_dev.get(), m_fence.Get(), m_fenceEvent, &m_frames, &m_garbage);

    ImGui::GetIO().IniFilename = "EditorLayout.ini";

    if (m_Camera)
        m_Camera->SetAspect(static_cast<float>(width) / static_cast<float>(height));

    return true;
}

//==============================================================================
// Render
//==============================================================================
void D3D12Renderer::Render()
{
    // ---- フレーム開始（待ち & Reset & cmd準備）----
    auto begin = m_scheduler.BeginFrame();
    const UINT fi = begin.frameIndex;
    ID3D12GraphicsCommandList* cmd = begin.cmd;

    // Reset 直後なので PSO を明示セット
    cmd->SetPipelineState(m_pipe.pso.Get());

    // ① フレーム冒頭で「前フレームの希望サイズ」を適用（旧RT を遅延破棄へ）
    RenderTargetHandles deadScene = m_viewports.ApplyPendingResizeIfNeeded(m_dev->GetDevice());

    // =====================================================================
    // ② Scene 用オフスクリーンへ描画（HFOV固定/アスペクト追従は Viewports 側）
    // =====================================================================
    if (m_Camera) {
        m_viewports.RenderScene(cmd, m_sceneRenderer, m_Camera.get(), m_CurrentScene.get(),
            /*frameIndex=*/fi, /*maxObjects=*/MaxObjects);
    }

    // =====================================================================
    // 1.5) Game 用オフスクリーンへ “固定カメラ” で描画（Scene 同期は Viewports 側）
    // =====================================================================
    m_viewports.RenderGame(cmd, m_sceneRenderer, m_CurrentScene.get(),
        /*frameIndex=*/fi, /*maxObjects=*/MaxObjects);

    // =====================================================================
    // ③ バックバッファに UI（ImGui）
    // =====================================================================
    ID3D12Resource* bb = m_dev->GetBackBuffer(fi);
    auto toRT_bb = CD3DX12_RESOURCE_BARRIER::Transition(
        bb, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmd->ResourceBarrier(1, &toRT_bb);

    auto rtv = m_dev->GetRTVHandle(fi);
    cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    const float clearBB[4] = { 0.2f, 0.2f, 0.4f, 1.0f };
    cmd->ClearRenderTargetView(rtv, clearBB, 0, nullptr);

    D3D12_VIEWPORT vpBB{ 0.f, 0.f, (float)m_dev->GetWidth(), (float)m_dev->GetHeight(), 0.f, 1.f };
    D3D12_RECT     scBB{ 0, 0, (LONG)m_dev->GetWidth(), (LONG)m_dev->GetHeight() };
    cmd->RSSetViewports(1, &vpBB);
    cmd->RSSetScissorRects(1, &scBB);

    static bool s_resetLayout = false;
    static bool s_autoRelayout = false;

    EditorContext ctx{};
    ctx.pEnableEditor = &m_IsEditor;
    ctx.rtWidth = m_dev->GetWidth();
    ctx.rtHeight = m_dev->GetHeight();
    ctx.fps = ImGui::GetIO().Framerate;
    ctx.pRequestResetLayout = &s_resetLayout;
    ctx.pAutoRelayout = &s_autoRelayout;

    // 現在のRTのSRVをUIに渡す（このフレームで使うのは“すでに描いたRT”）
    m_viewports.FeedToUI(ctx, m_imgui.get(), fi, kSceneSrvBase, kGameSrvBase);

    // UI は EditorPanels に委譲（Renderer から排除）
    ctx.DrawInspector = [&]() { EditorPanels::DrawInspector(m_Selected); };
    ctx.DrawHierarchy = [&]() { EditorPanels::DrawHierarchy(m_CurrentScene.get(), m_Selected); };

    m_imgui->NewFrame();
    m_imgui->BuildDockAndWindows(ctx);

    // ④ UI が出した希望サイズは「記録だけ」（このフレームでは作り直さない）
    const float dt = ImGui::GetIO().DeltaTime;
    const UINT wantW = (UINT)std::lroundf(ctx.sceneViewportSize.x);
    const UINT wantH = (UINT)std::lroundf(ctx.sceneViewportSize.y);
    if (wantW > 0 && wantH > 0) {
        // ヒステリシスと16pxスナップは Viewports 側で処理
        //m_viewports.RequestSceneResize(wantW, wantH, dt);
    }

    m_imgui->Render(cmd);

    // ---- RT → PRESENT ----
    auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        bb, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    cmd->ResourceBarrier(1, &toPresent);

    // ---- Submit & Present & Signal & 遅延破棄回収 ----
    m_scheduler.EndFrame(&deadScene);

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

    // SceneRT は UI ビューポートに追従（ここでは何もしない）
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

    // SceneRenderer の結線を先に切る（この後でフレーム等を破棄するため）
    m_sceneRenderer.Initialize(nullptr, PipelineSet{}, nullptr);

    // 念のため完全待機してから遅延破棄を全回収
    WaitForGPU();
    m_garbage.FlushAll();

    m_frames.Destroy();

    if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
    m_fence.Reset();

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
    ID3D12GraphicsCommandList* cmd = m_scheduler.GetCmd();
    if (!cmd) return;

    cmd->IASetVertexBuffers(0, 1, &mr->VertexBufferView);
    cmd->IASetIndexBuffer(&mr->IndexBufferView);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->DrawIndexedInstanced(mr->IndexCount, 1, 0, 0, 0);
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
