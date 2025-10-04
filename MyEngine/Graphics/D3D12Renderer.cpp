#include "D3D12Renderer.h"
#include "d3dx12.h"

#include <stdexcept>
#include <functional>
#include <cmath>
#include <cstring>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"


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
    // Device/SwapChain/RT/DS をラッパ経由で作成
    m_dev = std::make_unique<DeviceResources>();
    if (!m_dev->Initialize(hwnd, width, height, FrameCount))
        return false;

    ID3D12Device* dev = m_dev->GetDevice();

    // FrameResources：Upload CB（MaxObjects×FrameCount）
    if (!m_frames.Initialize(dev, FrameCount, sizeof(SceneConstantBuffer), MaxObjects))
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

    // フレーム完了待ち
    if (fr.fenceValue != 0 && m_fence->GetCompletedValue() < fr.fenceValue) {
        m_fence->SetEventOnCompletion(fr.fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // Reset
    fr.cmdAlloc->Reset();
    m_cmd->Reset(fr.cmdAlloc.Get(), m_pipe.pso.Get());

    // PRESENT -> RT
    ID3D12Resource* bb = m_dev->GetBackBuffer(fi);
    auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(
        bb, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_cmd->ResourceBarrier(1, &toRT);

    // クリア
    auto rtv = m_dev->GetRTVHandle(fi);
    auto dsv = m_dev->GetDSVHandle();
    const float clear[4] = { 0.2f,0.2f,0.4f,1.0f };
    m_cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    m_cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_cmd->ClearRenderTargetView(rtv, clear, 0, nullptr);

    // 固定状態
    D3D12_VIEWPORT vp{ 0.f, 0.f, (float)m_dev->GetWidth(), (float)m_dev->GetHeight(), 0.f, 1.f };
    D3D12_RECT sc{ 0, 0, (LONG)m_dev->GetWidth(), (LONG)m_dev->GetHeight() };
    m_cmd->RSSetViewports(1, &vp);
    m_cmd->RSSetScissorRects(1, &sc);
    m_cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_cmd->SetGraphicsRootSignature(m_pipe.root.Get());

    // シーン描画
    if (m_CurrentScene && m_Camera)
    {
        using namespace DirectX;
        const XMMATRIX viewMatrix = m_Camera->GetViewMatrix();
        const XMMATRIX projMatrix = m_Camera->GetProjectionMatrix();

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

                    std::memcpy(cbCPU + (UINT64)slot * cbStride, &cb, sizeof(cb));
                    m_cmd->SetGraphicsRootConstantBufferView(0, cbGPU + (UINT64)slot * cbStride);

                    m_cmd->IASetVertexBuffers(0, 1, &mr->VertexBufferView);
                    m_cmd->IASetIndexBuffer(&mr->IndexBufferView);
                    m_cmd->DrawIndexedInstanced(mr->IndexCount, 1, 0, 0, 0);

                    ++slot;
                }
                for (auto& ch : go->GetChildren()) draw(ch);
            };

        for (auto& root : m_CurrentScene->GetRootGameObjects()) draw(root);
    }

    // ImGui
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
    }

    // RT -> PRESENT
    auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        bb, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_cmd->ResourceBarrier(1, &toPresent);

    // Submit & Present & Signal
    m_cmd->Close();
    ID3D12CommandList* lists[] = { m_cmd.Get() };
    m_dev->GetQueue()->ExecuteCommandLists(1, lists);

    m_dev->Present(1);

    const UINT64 sig = m_nextFence++;
    m_dev->GetQueue()->Signal(m_fence.Get(), sig);
    fr.fenceValue = sig;

    ++m_frameCount;
}

//==============================================================================
// Resize
//==============================================================================
void D3D12Renderer::Resize(UINT width, UINT height) noexcept
{
    if (width == 0 || height == 0) return;

    // 全フレーム待機
    for (UINT i = 0; i < m_frames.GetCount(); ++i) {
        auto& fr = m_frames.Get(i);
        if (fr.fenceValue != 0 && m_fence->GetCompletedValue() < fr.fenceValue) {
            m_fence->SetEventOnCompletion(fr.fenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }

    m_dev->Resize(width, height);

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
