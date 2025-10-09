// D3D12Renderer.cpp
#include "D3D12Renderer.h"
#include "d3dx12.h"

#include <stdexcept>
#include <functional>
#include <cmath>
#include <cstring>
#include <string> // DrawVec3Row で使用

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "imgui_internal.h"
#include "Editor/EditorPanels.h"

using Microsoft::WRL::ComPtr;

namespace {
    // Inspector 折り畳みヘッダの簡易ヘルパ
    static bool BeginComponent(const char* title, ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));
        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
        const bool open = ImGui::CollapsingHeader(title, flags);
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        if (!open) return false;
        ImGui::BeginGroup();
        return true;
    }
    static void EndComponent() { ImGui::EndGroup(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); }

    static bool DrawVec3Row(const char* label, float& x, float& y, float& z,
        float labelWidth = 90.0f, float spacing = 6.0f, float dragSpeed = 0.01f)
    {
        bool changed = false;
        ImGui::PushID(label);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));
        if (ImGui::BeginTable("t", 4, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, labelWidth);
            ImGui::TableSetupColumn("x", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("y", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("z", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);

            auto axisField = [&](const char* ax, float& v)
                {
                    ImGui::TableNextColumn();
                    ImGui::BeginGroup();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    ImGui::TextUnformatted(ax);
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-1.0f);
                    std::string id = std::string("##") + label + "_" + ax;
                    changed |= ImGui::DragFloat(id.c_str(), &v, dragSpeed, 0.0f, 0.0f, "%.3f");
                    ImGui::EndGroup();
                };
            axisField("X", x); axisField("Y", y); axisField("Z", z);
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
        ImGui::PopID();
        return changed;
    }
}

//------------------------------------------------------------------------------
// 小物
//------------------------------------------------------------------------------
const char* D3D12Renderer::GONameUTF8(const GameObject* go)
{
    return go ? go->Name.c_str() : "(null)";
}

void D3D12Renderer::DrawHierarchyNode(const std::shared_ptr<GameObject>& go)
{
    if (!go) return;
    ImGui::PushID(go.get());
    const bool isSelected = (!m_Selected.expired() && m_Selected.lock().get() == go.get());

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
        | ImGuiTreeNodeFlags_SpanFullWidth
        | (isSelected ? ImGuiTreeNodeFlags_Selected : 0);

    if (go->GetChildren().empty())
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    const bool open = ImGui::TreeNodeEx(GONameUTF8(go.get()), flags);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        m_Selected = go;

    if (open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
    {
        for (auto& ch : go->GetChildren())
            DrawHierarchyNode(ch);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

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
        m_sceneRT.EnsureImGuiSRV(m_imgui.get(), kSceneSrvSlot);
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
        m_gameRT.EnsureImGuiSRV(m_imgui.get(), kGameSrvSlot);
    }

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
        m_sceneRT.EnsureImGuiSRV(m_imgui.get(), kSceneSrvSlot);

        m_pendingSceneRTW = m_pendingSceneRTH = 0;
    }

    // ★ Sceneカメラのアスペクトは常に最新のRTに合わせる
    if (m_Camera && m_sceneRT.Width() > 0 && m_sceneRT.Height() > 0) {
        m_Camera->SetAspect((float)m_sceneRT.Width() / (float)m_sceneRT.Height());
    }

    // ---- Reset ----
    fr.cmdAlloc->Reset();
    m_cmd->Reset(fr.cmdAlloc.Get(), m_pipe.pso.Get());

    // ★ 初回だけ：Scene カメラの view/proj とアスペクトを保存し、Game用に固定
    if (!m_gameCamFrozen && m_Camera) {
        using namespace DirectX;
        XMStoreFloat4x4(&m_gameViewInit, m_Camera->GetViewMatrix());
        XMStoreFloat4x4(&m_gameProjInit, m_Camera->GetProjectionMatrix());
        m_gameFrozenAspect = (float)m_sceneRT.Width() / (float)m_sceneRT.Height();
        m_gameCamFrozen = true;
    }

    // =====================================================================
    // ① Scene 用オフスクリーンへ描画（CB: [0 .. MaxObjects-1]）
    // =====================================================================
    if (m_sceneRT.Color())
    {
        m_sceneRT.TransitionToRT(m_cmd.Get());
        m_sceneRT.Bind(m_cmd.Get());
        m_sceneRT.Clear(m_cmd.Get());

        const float w = (float)m_sceneRT.Width();
        const float h = (float)m_sceneRT.Height();
        D3D12_VIEWPORT vp{ 0.f, 0.f, w, h, 0.f, 1.f };
        D3D12_RECT     sc{ 0, 0, (LONG)m_sceneRT.Width(), (LONG)m_sceneRT.Height() };
        m_cmd->RSSetViewports(1, &vp);
        m_cmd->RSSetScissorRects(1, &sc);
        m_cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_cmd->SetGraphicsRootSignature(m_pipe.root.Get());

        if (m_CurrentScene && m_Camera)
        {
            using namespace DirectX;
            const XMMATRIX viewMatrix = m_Camera->GetViewMatrix();
            const XMMATRIX projMatrix = m_Camera->GetProjectionMatrix();

            const UINT baseScene = 0;   // 前半
            UINT slot = 0;

            UINT8* cbCPU = fr.cpu;
            D3D12_GPU_VIRTUAL_ADDRESS cbGPU = fr.resource->GetGPUVirtualAddress();
            const UINT cbStride = m_frames.GetCBStride();

            std::function<void(std::shared_ptr<GameObject>)> draw =
                [&](std::shared_ptr<GameObject> go)
                {
                    if (!go || slot >= MaxObjects) return;
                    auto mr = go->GetComponent<MeshRendererComponent>();
                    if (mr && mr->VertexBuffer && mr->IndexBuffer && mr->IndexCount > 0)
                    {
                        using namespace DirectX;
                        XMMATRIX world = go->Transform->GetWorldMatrix();
                        XMMATRIX mvp = world * viewMatrix * projMatrix;

                        XMVECTOR det;
                        XMMATRIX inv = XMMatrixInverse(&det, world);
                        float detScalar = XMVectorGetX(det);
                        if (!std::isfinite(detScalar) || std::fabs(detScalar) < 1e-8f) inv = XMMatrixIdentity();
                        XMMATRIX worldIT = XMMatrixTranspose(inv);

                        SceneConstantBuffer cb{};
                        XMStoreFloat4x4(&cb.mvp, mvp);
                        XMStoreFloat4x4(&cb.world, world);
                        XMStoreFloat4x4(&cb.worldIT, worldIT);
                        XMStoreFloat3(&cb.lightDir, XMVector3Normalize(XMVectorSet(0.0f, -1.0f, -1.0f, 0.0f)));
                        cb.pad = 0.0f;

                        const UINT dst = baseScene + slot;
                        std::memcpy(cbCPU + (UINT64)dst * cbStride, &cb, sizeof(cb));
                        m_cmd->SetGraphicsRootConstantBufferView(0, cbGPU + (UINT64)dst * cbStride);

                        m_cmd->IASetVertexBuffers(0, 1, &mr->VertexBufferView);
                        m_cmd->IASetIndexBuffer(&mr->IndexBufferView);
                        m_cmd->DrawIndexedInstanced(mr->IndexCount, 1, 0, 0, 0);
                        ++slot;
                    }
                    for (auto& ch : go->GetChildren()) draw(ch);
                };

            for (auto& root : m_CurrentScene->GetRootGameObjects()) draw(root);
        }

        m_sceneRT.TransitionToSRV(m_cmd.Get());
    }

    // =====================================================================
    // 1.5) Game 用オフスクリーンへ “固定カメラ” で描画（CB: [MaxObjects .. 2*MaxObjects-1]）
    // =====================================================================
    if (m_gameRT.Color() && m_gameCamFrozen)
    {
        m_gameRT.TransitionToRT(m_cmd.Get());
        m_gameRT.Bind(m_cmd.Get());
        m_gameRT.Clear(m_cmd.Get());

        D3D12_VIEWPORT vpG{ 0.f, 0.f, (float)m_gameRT.Width(), (float)m_gameRT.Height(), 0.f, 1.f };
        D3D12_RECT     scG{ 0, 0, (LONG)m_gameRT.Width(), (LONG)m_gameRT.Height() };
        m_cmd->RSSetViewports(1, &vpG);
        m_cmd->RSSetScissorRects(1, &scG);
        m_cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_cmd->SetGraphicsRootSignature(m_pipe.root.Get());

        using namespace DirectX;
        XMMATRIX viewMatrix = XMLoadFloat4x4(&m_gameViewInit);
        XMMATRIX projMatrix = XMLoadFloat4x4(&m_gameProjInit);

        const UINT baseGame = MaxObjects; // 後半
        UINT slot = 0;

        UINT8* cbCPU = fr.cpu;
        D3D12_GPU_VIRTUAL_ADDRESS cbGPU = fr.resource->GetGPUVirtualAddress();
        const UINT cbStride = m_frames.GetCBStride();

        std::function<void(std::shared_ptr<GameObject>)> drawGame =
            [&](std::shared_ptr<GameObject> go)
            {
                if (!go || slot >= MaxObjects) return;
                auto mr = go->GetComponent<MeshRendererComponent>();
                if (mr && mr->VertexBuffer && mr->IndexBuffer && mr->IndexCount > 0)
                {
                    XMMATRIX world = go->Transform->GetWorldMatrix();
                    XMMATRIX mvp = world * viewMatrix * projMatrix;

                    XMVECTOR det;
                    XMMATRIX inv = XMMatrixInverse(&det, world);
                    float detScalar = XMVectorGetX(det);
                    if (!std::isfinite(detScalar) || std::fabs(detScalar) < 1e-8f) inv = XMMatrixIdentity();
                    XMMATRIX worldIT = XMMatrixTranspose(inv);

                    SceneConstantBuffer cb{};
                    XMStoreFloat4x4(&cb.mvp, mvp);
                    XMStoreFloat4x4(&cb.world, world);
                    XMStoreFloat4x4(&cb.worldIT, worldIT);
                    XMStoreFloat3(&cb.lightDir, XMVector3Normalize(XMVectorSet(0.0f, -1.0f, -1.0f, 0.0f)));
                    cb.pad = 0.0f;

                    const UINT dst = baseGame + slot;
                    std::memcpy(cbCPU + (UINT64)dst * cbStride, &cb, sizeof(cb));
                    m_cmd->SetGraphicsRootConstantBufferView(0, cbGPU + (UINT64)dst * cbStride);

                    m_cmd->IASetVertexBuffers(0, 1, &mr->VertexBufferView);
                    m_cmd->IASetIndexBuffer(&mr->IndexBufferView);
                    m_cmd->DrawIndexedInstanced(mr->IndexCount, 1, 0, 0, 0);
                    ++slot;
                }
                for (auto& ch : go->GetChildren()) drawGame(ch);
            };

        if (m_CurrentScene) {
            for (auto& root : m_CurrentScene->GetRootGameObjects()) drawGame(root);
        }

        m_gameRT.TransitionToSRV(m_cmd.Get());
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

        // RenderTarget から毎フレーム SRV を保証
        ctx.sceneTexId = m_sceneRT.EnsureImGuiSRV(m_imgui.get(), kSceneSrvSlot);
        ctx.sceneRTWidth = m_sceneRT.Width();
        ctx.sceneRTHeight = m_sceneRT.Height();

        ctx.gameTexId = m_gameRT.EnsureImGuiSRV(m_imgui.get(), kGameSrvSlot);
        ctx.gameRTWidth = m_gameRT.Width();
        ctx.gameRTHeight = m_gameRT.Height();

        ctx.DrawInspector = [&]()
            {
                if (auto sel = m_Selected.lock())
                {
                    ImGui::Text("Selected: %s", GONameUTF8(sel.get()));
                    ImGui::Separator();
                    if (BeginComponent("Transform"))
                    {
                        auto& tr = sel->Transform;
                        DrawVec3Row("Position", tr->Position.x, tr->Position.y, tr->Position.z);
                        DrawVec3Row("Rotation", tr->Rotation.x, tr->Rotation.y, tr->Rotation.z);
                        DrawVec3Row("Scale", tr->Scale.x, tr->Scale.y, tr->Scale.z);
                        EndComponent();
                    }
                }
                else {
                    ImGui::TextDisabled("No selection");
                }
            };

        ctx.DrawHierarchy = [&]()
            {
                if (m_CurrentScene) {
                    for (auto& root : m_CurrentScene->GetRootGameObjects())
                        DrawHierarchyNode(root);
                }
                else {
                    ImGui::TextDisabled("No scene");
                }
            };

        m_imgui->NewFrame();
        m_imgui->BuildDockAndWindows(ctx);
        m_imgui->Render(m_cmd.Get());

        // 次フレーム用に Scene RT のリサイズ要求
        if (ctx.sceneTexId) {
            UINT wantW = (UINT)ImMax(1.0f, ctx.sceneViewportSize.x);
            UINT wantH = (UINT)ImMax(1.0f, ctx.sceneViewportSize.y);
            if (wantW != m_sceneRT.Width() || wantH != m_sceneRT.Height()) {
                RequestSceneRTResize(wantW, wantH);
            }
        }
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

    // ここでは即作り直さず、次フレームの保留リサイズで安全に差し替える
    RequestSceneRTResize(width, height);

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
