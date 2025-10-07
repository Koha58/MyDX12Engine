#define IMGUI_DEFINE_MATH_OPERATORS
#include "ImGuiLayer.h"
// ▼プロジェクト構成に合わせて相対パスを調整（例は Renderer/ と Editor/ が同階層）
#include "EditorContext.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "imgui_internal.h"

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
    desc.NumDescriptors = 1;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvHeap))))
        return false;

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
    ii.LegacySingleSrvCpuDescriptor = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    ii.LegacySingleSrvGpuDescriptor = m_srvHeap->GetGPUDescriptorHandleForHeapStart();

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
        ImGui::DockBuilderDockWindow("Scene",     dock_center);
        ImGui::DockBuilderDockWindow("Game",      dock_center_bottom);

        ImGui::DockBuilderFinish(dockspace_id);
    }
    else if (resized && ctx.pAutoRelayout && *ctx.pAutoRelayout) {
        if (ImGui::DockBuilderGetNode(dockspace_id))
            ImGui::DockBuilderSetNodeSize(dockspace_id, vp->WorkSize);
    }

    // ===== ここから “未ドック窓の救済” を追加 =====
    if (resized) {
        const float margin = 24.0f; // 端からの安全マージン（タイトルバー分など）
        ImRect work(vp->WorkPos, vp->WorkPos + vp->WorkSize);

        auto clamp_window_into_work = [&](const char* name)
            {
                ImGuiWindow* w = ImGui::FindWindowByName(name);
                if (!w) return;
                if (w->DockIsActive) return; // ドック中は不要

                const float margin = 12.0f;
                const ImVec2 min_size(160.0f, 120.0f);
                ImRect work(vp->WorkPos, vp->WorkPos + vp->WorkSize);

                // AutoResize の場合は「想定サイズ」を使う（サイズはいじらない）
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

                // AutoResize 窓はサイズを触らず、位置のみ反映
                if (!auto_resize) ImGui::SetWindowSize(w, size, ImGuiCond_Always);
                ImGui::SetWindowPos(w, pos, ImGuiCond_Always);
            };



        // 自前ウィンドウは確実に救済
        clamp_window_into_work("Inspector");
        clamp_window_into_work("Hierarchy");
        clamp_window_into_work("Scene");
        clamp_window_into_work("Game");
        clamp_window_into_work("Stats");

        // （オプション）全ウィンドウを救済したい場合：
        // if (ctx.pAutoRecoverAll && *ctx.pAutoRecoverAll) {
        //     ImGuiContext* g = ImGui::GetCurrentContext();
        //     for (ImGuiWindow* w : g->Windows) {
        //         if (!w->DockIsActive && (w->Flags & ImGuiWindowFlags_ChildWindow) == 0)
        //             clamp_window_into_work(w->Name);
        //     }
        // }
    }

    // ===== UI =====
    ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("FPS: %.1f", ctx.fps);
    ImGui::Text("Size: %u x %u", ctx.rtWidth, ctx.rtHeight);
    ImGui::End();

    if (ctx.pEnableEditor && *ctx.pEnableEditor) 
    {
        // ===== Scene =====
        if (ImGui::Begin("Scene"))
        {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ctx.sceneViewportSize = avail;
            ctx.sceneHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            ctx.sceneFocused = ImGui::IsWindowFocused();

            // 後で：ImGui::Image(ctx.sceneTexture, avail)
            ImGui::TextDisabled("Scene View(offscreen not wired yet)");
        }
        ImGui::End();

        // ===== Game =====
        if (ImGui::Begin("Game"))
        {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ctx.gameViewportSize = avail;
            ctx.gameHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            ctx.gameFocused = ImGui::IsWindowFocused();

            // 後で：ImGui::Image(ctx.sceneTexture, avail)
            ImGui::TextDisabled("Game View(offscreen not wired yet)");
        }
        ImGui::End();

        ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse);
        if (ctx.DrawInspector) ctx.DrawInspector();
        ImGui::End();

        ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoCollapse);
        if (ctx.DrawHierarchy) ctx.DrawHierarchy();
        ImGui::End();
    }
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
