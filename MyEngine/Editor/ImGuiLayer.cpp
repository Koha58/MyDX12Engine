#define IMGUI_DEFINE_MATH_OPERATORS
#include "ImGuiLayer.h"
// ▼プロジェクト構成に合わせて相対パスを調整（例は Renderer/ と Editor/ が同階層）
#include "EditorContext.h"
#include "Core/EditorInterop.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "imgui_internal.h"

#include <cmath>   // floorf
#include <cstdio>  // snprintf

// 端のにじみを防ぐ：UVを半テクセル内側に詰め、矩形はピクセルスナップ
static inline void AddImage_NoEdge(ImDrawList* dl, ImTextureID tex,
    ImVec2 p0, ImVec2 p1,
    int texW, int texH)
{
    if (!tex || texW <= 0 || texH <= 0) return;

    const float ueps = 0.5f / (float)texW;
    const float veps = 0.5f / (float)texH;
    ImVec2 uv0(ueps, veps);
    ImVec2 uv1(1.0f - ueps, 1.0f - veps);

    // ピクセルスナップ（小数→整数）で微妙な隙間/にじみを抑える
    ImVec2 q0 = ImFloor(p0);
    ImVec2 q1 = ImFloor(p1);

    dl->AddImage(tex, q0, q1, uv0, uv1);
}


bool ImGuiLayer::Initialize(HWND hwnd,
    ID3D12Device* device,
    ID3D12CommandQueue* queue,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT /*dsvFormat*/,
    UINT numFramesInFlight)
{
    if (m_initialized) return true;

    // SRV heap
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 64;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvHeap))))
        return false;

    m_device = device; // ★ 後で SRV を作るために保持
    m_srvIncSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_srvCpuStart = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    m_srvGpuStart = m_srvHeap->GetGPUDescriptorHandleForHeapStart();

    // ImGui core
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::GetIO().IniFilename = "Editor/EditorLayout.ini";
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);

    // DX12 backend (new API)
    ImGui_ImplDX12_InitInfo ii{};
    ii.Device = device;
    ii.CommandQueue = queue;
    ii.NumFramesInFlight = numFramesInFlight;
    ii.RTVFormat = rtvFormat;
    ii.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    ii.SrvDescriptorHeap = m_srvHeap.Get();
    ii.LegacySingleSrvCpuDescriptor = m_srvCpuStart;
    ii.LegacySingleSrvGpuDescriptor = m_srvGpuStart;

    ImGui_ImplDX12_Init(&ii);

    // Fonts
    ImGui::GetIO().Fonts->AddFontDefault();

    m_initialized = true;
    return true;
}

void ImGuiLayer::NewFrame()
{
    if (!m_initialized) return;
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::BuildDockAndWindows(EditorContext& ctx)
{
    if (!m_initialized) return;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

    ImGui::DockSpaceOverViewport(
        dockspace_id, vp, ImGuiDockNodeFlags_PassthruCentralNode, nullptr);

    // ===== メニュー =====
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Editor")) {
            ImGui::MenuItem("Enable Editor", nullptr, ctx.pEnableEditor);
            if (ctx.pRequestResetLayout && ImGui::MenuItem("Reset Editor Layout"))
                *ctx.pRequestResetLayout = true;
            if (ctx.pAutoRelayout)
                ImGui::MenuItem("Keep ratio on resize", nullptr, ctx.pAutoRelayout);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // ===== レイアウト初期化/リセット or 非破壊リサイズ対応 =====
    static bool  s_first = true;
    static ImVec2 s_lastWorkSize = ImVec2(0, 0);

    const bool resized = (s_lastWorkSize.x != vp->WorkSize.x) || (s_lastWorkSize.y != vp->WorkSize.y);
    s_lastWorkSize = vp->WorkSize;

    const bool request_reset = (ctx.pRequestResetLayout && *ctx.pRequestResetLayout);
    const bool need_build = s_first || request_reset;

    if (need_build) {
        s_first = false;
        if (ctx.pRequestResetLayout) *ctx.pRequestResetLayout = false;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, vp->WorkSize);

        ImGuiID dock_main = dockspace_id;

        ImGuiID dock_left, dock_left_bottom, dock_center, dock_center_bottom;
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.25f, &dock_left, &dock_main);
        ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.50f, &dock_left_bottom, &dock_left);
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.50f, &dock_center_bottom, &dock_center);

        ImGui::DockBuilderDockWindow("Inspector", dock_left);
        ImGui::DockBuilderDockWindow("Hierarchy", dock_left_bottom);
        ImGui::DockBuilderDockWindow("Scene", dock_center);
        ImGui::DockBuilderDockWindow("Game", dock_center_bottom);

        ImGui::DockBuilderFinish(dockspace_id);
    }
    else if (resized && ctx.pAutoRelayout && *ctx.pAutoRelayout) {
        if (ImGui::DockBuilderGetNode(dockspace_id))
            ImGui::DockBuilderSetNodeSize(dockspace_id, vp->WorkSize);
    }

    // ===== ここから “未ドック窓の救済” =====
    if (resized) {
        ImRect work(vp->WorkPos, vp->WorkPos + vp->WorkSize);

        auto clamp_window_into_work = [&](const char* name)
            {
                ImGuiWindow* w = ImGui::FindWindowByName(name);
                if (!w) return;
                if (w->DockIsActive) return; // ドック中は不要

                const float margin = 12.0f;
                const ImVec2 min_size(160.0f, 120.0f);
                ImRect work(vp->WorkPos, vp->WorkPos + vp->WorkSize);

                // AutoResize の場合は「想定サイズ」
                const bool auto_resize = (w->Flags & ImGuiWindowFlags_AlwaysAutoResize) != 0;
                ImVec2 size = auto_resize ? ImGui::CalcWindowNextAutoFitSize(w) : w->SizeFull;

                ImVec2 max_size(
                    ImMax(min_size.x, work.GetWidth() - 2.0f * margin),
                    ImMax(min_size.y, work.GetHeight() - 2.0f * margin)
                );
                size.x = ImClamp(size.x, min_size.x, max_size.x);
                size.y = ImClamp(size.y, min_size.y, max_size.y);

                if (work.GetWidth() < min_size.x || work.GetHeight() < min_size.y)
                {
                    size.x = ImMin(size.x, ImMax(1.0f, work.GetWidth() - 2.0f * margin));
                    size.y = ImMin(size.y, ImMax(1.0f, work.GetHeight() - 2.0f * margin));

                    ImVec2 center(
                        work.Min.x + ImMax(0.0f, (work.GetWidth() - size.x)) * 0.5f,
                        work.Min.y + ImMax(0.0f, (work.GetHeight() - size.y)) * 0.5f
                    );
                    if (!auto_resize) ImGui::SetWindowSize(w, size, ImGuiCond_Always);
                    ImGui::SetWindowPos(w, center, ImGuiCond_Always);
                    return;
                }

                ImVec2 min_pos = work.Min + ImVec2(margin, margin);
                ImVec2 max_pos = work.Max - size - ImVec2(margin, margin);
                if (max_pos.x < min_pos.x) { float c = work.Min.x + (work.GetWidth() - size.x) * 0.5f; min_pos.x = max_pos.x = c; }
                if (max_pos.y < min_pos.y) { float c = work.Min.y + (work.GetHeight() - size.y) * 0.5f; min_pos.y = max_pos.y = c; }

                ImVec2 pos = w->Pos;
                pos.x = ImClamp(pos.x, min_pos.x, max_pos.x);
                pos.y = ImClamp(pos.y, min_pos.y, max_pos.y);

                if (!auto_resize) ImGui::SetWindowSize(w, size, ImGuiCond_Always);
                ImGui::SetWindowPos(w, pos, ImGuiCond_Always);
            };

        clamp_window_into_work("Inspector");
        clamp_window_into_work("Hierarchy");
        clamp_window_into_work("Scene");
        clamp_window_into_work("Game");
        clamp_window_into_work("Stats");
    }

    // ===== UI =====
    ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("FPS: %.1f", ctx.fps);
    ImGui::Text("Size: %u x %u", ctx.rtWidth, ctx.rtHeight);
    ImGui::End();

    // Scene フラグ初期化
    EditorInterop::SetSceneHovered(false);
    EditorInterop::SetSceneFocused(false);

    if (ctx.pEnableEditor && *ctx.pEnableEditor)
    {
        // ===== Scene =====
        {
            // 親の枠と余白を殺す（←ここが残っていると1pxの縁が出ます）
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

            const ImGuiWindowFlags sceneFlags =
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse |
                ImGuiWindowFlags_NoCollapse;

            if (ImGui::Begin("Scene", nullptr, sceneFlags))
            {
                // 子は背景・枠なしで全面キャッチ＆描画
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
                ImGuiWindowFlags childFlags =
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoBackground; // 子の背景も描かない
                ImGui::BeginChild("SceneCanvas", ImVec2(0, 0), false, childFlags);

                ImVec2 avail = ImFloor(ImGui::GetContentRegionAvail());
                ImVec2 p0 = ImFloor(ImGui::GetCursorScreenPos());
                ImVec2 p1 = p0 + avail;

                ImGui::InvisibleButton("##SceneInputCatcher", avail,
                    ImGuiButtonFlags_MouseButtonLeft |
                    ImGuiButtonFlags_MouseButtonRight |
                    ImGuiButtonFlags_MouseButtonMiddle);

                ctx.sceneHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                ctx.sceneFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
                EditorInterop::SetSceneHovered(ctx.sceneHovered);
                EditorInterop::SetSceneFocused(ctx.sceneFocused);

                ImDrawList* dl = ImGui::GetWindowDrawList();
                // 背景は自前でフラット塗り（子は NoBackground なので二重描画にならない）
                dl->AddRectFilled(p0, p1, IM_COL32(30, 30, 30, 255));

                if (ctx.sceneTexId && ctx.sceneRTWidth > 0 && ctx.sceneRTHeight > 0)
                {
                    const float texAspect = (float)ctx.sceneRTWidth / (float)ctx.sceneRTHeight;
                    const float winAspect = (avail.y > 0.0f) ? (avail.x / avail.y) : texAspect;
                    float w, h;
                    if (winAspect > texAspect) { w = avail.x; h = w / texAspect; }
                    else { h = avail.y; w = h * texAspect; }

                    auto evenf = [](float v) { int i = (int)v; i &= ~1; if (i < 2)i = 2; return (float)i; };
                    w = evenf(w); h = evenf(h);

                    ImVec2 size(w, h);
                    ImVec2 center((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
                    ImVec2 q0(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
                    ImVec2 q1(center.x + size.x * 0.5f, center.y + size.y * 0.5f);

                    AddImage_NoEdge(dl, ctx.sceneTexId, ImFloor(q0), ImFloor(q1),
                        (int)ctx.sceneRTWidth, (int)ctx.sceneRTHeight);
                }
                else {
                    ImGui::SetCursorScreenPos(p0 + ImVec2(6, 6));
                    ImGui::TextDisabled("Scene View (no SRV set)");
                }

                auto evenf_sz = [](float v) { int i = (int)v; i &= ~1; if (i < 2)i = 2; return (float)i; };
                ctx.sceneViewportSize = ImVec2(evenf_sz(avail.x), evenf_sz(avail.y));

                ImGui::EndChild();
                ImGui::PopStyleVar(); // ChildBorderSize
            }
            ImGui::End();

            ImGui::PopStyleVar(3); // WindowPadding, WindowBorderSize, WindowRounding
        }



        // ===== Game =====
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            const ImGuiWindowFlags gameFlags =
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

            if (ImGui::Begin("Game", nullptr, gameFlags))
            {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                avail.x = floorf(avail.x);
                avail.y = floorf(avail.y);

                // 入力キャッチャ（ウィンドウ操作に奪われないように）
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 p1 = ImVec2(p0.x + avail.x, p0.y + avail.y);
                ImGui::InvisibleButton("##GameInputCatcher", avail,
                    ImGuiButtonFlags_MouseButtonLeft |
                    ImGuiButtonFlags_MouseButtonRight |
                    ImGuiButtonFlags_MouseButtonMiddle);

                ctx.gameViewportSize = avail;
                ctx.gameHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                ctx.gameFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

                ImDrawList* dl = ImGui::GetWindowDrawList();
                // 背景（レタボ・ピラボを見せる）
                dl->AddRectFilled(p0, p1, IM_COL32(30, 30, 30, 255));

                // ====== ズーム ======
                static float s_gameScale = 1.0f;        // 現在のズーム
                static const float s_minScale = 1.0f;   // 初期“Fit”が下限（これ未満に縮めない）
                static const float s_maxScale = 4.0f;   // 上限

                if (ctx.gameHovered) {
                    ImGuiIO& io = ImGui::GetIO();
                    float wheel = io.MouseWheel;
                    if (wheel != 0.0f) {
                        s_gameScale *= (wheel > 0 ? 1.1f : 1.0f / 1.1f);
                    }
                    // Ctrl+0 で 100%（Fit）に戻す
                    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_0)) {
                        s_gameScale = 1.0f;
                    }
                }
                s_gameScale = ImClamp(s_gameScale, s_minScale, s_maxScale);

                if (ctx.gameTexId && ctx.gameRTWidth > 0 && ctx.gameRTHeight > 0)
                {
                    // === Fit（アスペクト維持・全体表示）＋中央配置 ===
                    const float texAspect = (float)ctx.gameRTWidth / (float)ctx.gameRTHeight;
                    float w = avail.x, h = avail.y;
                    if (w / h > texAspect) { w = h * texAspect; }
                    else { h = w / texAspect; }

                    w *= s_gameScale;
                    h *= s_gameScale;

                    ImVec2 size(w, h);
                    ImVec2 center = ImVec2((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
                    ImVec2 q0 = ImVec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
                    ImVec2 q1 = ImVec2(center.x + size.x * 0.5f, center.y + size.y * 0.5f);

                    AddImage_NoEdge(dl, ctx.gameTexId, q0, q1, (int)ctx.gameRTWidth, (int)ctx.gameRTHeight);

                    // 右下にScale表示
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "Scale: %.0f%%", s_gameScale * 100.f);
                    ImVec2 ts = ImGui::CalcTextSize(buf);
                    ImVec2 pad(6, 4);
                    ImVec2 r0 = ImVec2(p1.x - ts.x - pad.x * 2, p1.y - ts.y - pad.y * 2);
                    dl->AddRectFilled(r0, p1, IM_COL32(0, 0, 0, 130), 6.0f);
                    dl->AddText(ImVec2(r0.x + pad.x, r0.y + pad.y), IM_COL32_WHITE, buf);
                }
                else {
                    ImGui::SetCursorScreenPos(ImVec2(p0.x + 6, p0.y + 6));
                    ImGui::TextDisabled("Game View (no SRV set)");
                }
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }

        ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse);
        if (ctx.DrawInspector) ctx.DrawInspector();
        ImGui::End();

        ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoCollapse);
        if (ctx.DrawHierarchy) ctx.DrawHierarchy();
        ImGui::End();
    }
}

ImTextureID ImGuiLayer::CreateOrUpdateTextureSRV(ID3D12Resource* tex, DXGI_FORMAT fmt, UINT slot)
{
    // slot=0 はフォント用に予約済み。1 以降を使う。
    D3D12_CPU_DESCRIPTOR_HANDLE cpu = m_srvCpuStart;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu = m_srvGpuStart;
    cpu.ptr += SIZE_T(m_srvIncSize) * slot;
    gpu.ptr += SIZE_T(m_srvIncSize) * slot;

    D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = fmt;
    sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sd.Texture2D.MipLevels = 1;

    m_device->CreateShaderResourceView(tex, &sd, cpu);
    return (ImTextureID)gpu.ptr; // DX12: ImTextureID は GPU SRV ハンドル
}

void ImGuiLayer::Render(ID3D12GraphicsCommandList* cmd)
{
    if (!m_initialized) return;

    ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
    cmd->SetDescriptorHeaps(1, heaps);

    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void ImGuiLayer::Shutdown()
{
    if (!m_initialized) return;
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    m_srvHeap.Reset();
    m_initialized = false;
}
