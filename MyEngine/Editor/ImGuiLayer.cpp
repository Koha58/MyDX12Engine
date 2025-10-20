// ============================================================================
// ImGuiLayer.cpp  �\  DX12 + Win32 �� ImGui �̔������b�p
// �ړI�F�������^Dock���C�A�E�g�^�\���L�����o�X��`�v�Z�^�e�N�X�`���\��t���݂̂�S���B
//       �E�g�`�惊�\�[�X�̐���/�j��/�R�}���h�L�^�h�̓����_���[���i�G���W���j�Ɋ��S�Ϗ�
//       �E�g�f�[�^/��ԁh�� EditorContext �o�R�iImGui �w�̓X�e�[�g���X�u���j
// �|�C���g�F�ォ��ǂ�ł�����Ȃ��悤�A�݌v���f�̗��R�ƒ��ӓ_���R�����g�Ŗ����B
// �ˑ��Fimgui_impl_win32 / imgui_impl_dx12�iv1.92+New Init API�j
// ----------------------------------------------------------------------------
// �ύX���₷���ӏ��F
//  - INI�ۑ���:      ImGui::GetIO().IniFilename
//  - Dock�����z�u:   BuildDockAndWindows() �� DockBuilder* �Z�N�V����
//  - SRV���蓖��:    CreateOrUpdateTextureSRV() �� slot �Ǘ��i�ȈՃA���P�[�^���]�n�j
//  - ���^�{/�s���{:  AddImage_NoEdge()�{Scene/Game �̋�`�v�Z
//  - �������K��:     �g�������� ImGuiLayer �݂̂ōs���h�i��d�ۂߋ֎~�j
// ----------------------------------------------------------------------------
// �X���b�h�O��F���C���X���b�h��p�iImGui �̓X���b�h�Z�[�t�ł͂Ȃ��j
// ���\�[�X�����FShutdown() �ȑO�� GPU ���� Wait �ς̑z��i�Ăяo�����œ����Ǘ��j
// ���W�n�����FImGui �͊�{ float �s�N�Z�����W�iDPI �X�P�[���� ImGui �����z���j
// ============================================================================

#define IMGUI_DEFINE_MATH_OPERATORS
#include "ImGuiLayer.h"

// Editor ���̏�ԂƁA�G���W���ւ̃u���b�W�i�t�H�[�J�X/�z�o�[�ʒm�j
#include "EditorContext.h"
#include "Core/EditorInterop.h"

// ImGui / backend
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "imgui_internal.h"

#include <cmath>   // floorf
#include <cstdio>  // snprintf

// ----------------------------------------------------------------------------
// [���[�e�B���e�B] AddImage_NoEdge
// - �p�r�F�e�N�X�`���[�́g�ɂ��݁h�� 1px ���̏o�����y������W����@�B
// - ���@�FUV �𔼃e�N�Z�������Ɋ񂹂�i���E���ׂ��T���v�����O�������j�{��`�𐮐����W�֊ۂ߁B
// - �z��ΏہF�����_�[�^�[�Q�b�g�iScene/Game RT�j�� UI �e�N�X�`���B
// - ����p�F�~�b�v��t�B���^�������ꍇ�͊��S�ɏ����Ȃ����Ƃ�����i�{�֐��� first aid�j�B
//   �� ��茵���ɂ��Ȃ�g�O����1px�̃N�����v�p�p�f�B���O�h������RT�݌v���I�����B
// ----------------------------------------------------------------------------
static inline void AddImage_NoEdge(ImDrawList* dl, ImTextureID tex,
    ImVec2 p0, ImVec2 p1,
    int texW, int texH)
{
    if (!tex || texW <= 0 || texH <= 0) return;

    // ���e�N�Z���E�I�t�Z�b�g�iDirect3D �R���̒�΁j
    const float ueps = 0.5f / (float)texW;
    const float veps = 0.5f / (float)texH;
    ImVec2 uv0(ueps, veps);
    ImVec2 uv1(1.0f - ueps, 1.0f - veps);

    // �s�N�Z���X�i�b�v�FImGui �� float ���W�����A�\�����͐������E�ɑ����������ɂ��݂ɂ���
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
    // ���d�������̖h�~�i�Ăяo������������Ă�ł����S�Ȃ悤�Ɂj
    if (m_initialized) return true;

    // =========================================================================
    // [1] SRV �f�B�X�N���v�^�q�[�v�̍쐬
    // - ImGui �̃t�H���gSRV�{���[�U�[�̃e�N�X�`��SRV�iScene/Game ���j�𓯋�������B
    // - SHADER_VISIBLE �K�{�F�`�撆�Ƀs�N�Z��/���_�V�F�[�_���� SRV �Q�Ƃ��邽�߁B
    // - NumDescriptors�F�����g�������z���]�T����������i64�j�B�K�v�ɉ����đ��₷���ƁB
    // - ���L���F�{�N���X������/�ێ�/�j���B�Ăяo������ SRV �X���b�g�ԍ������ӎ�����B
    // =========================================================================
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 64;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvHeap))))
        return false;

    // DX12 �q�[�v�̃A�h���X���iCPU/GPU �����j�Ƒ����T�C�Y��ێ�
    m_device = device; // �� ��i�� CreateShaderResourceView �Ŏg�p
    m_srvIncSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_srvCpuStart = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    m_srvGpuStart = m_srvHeap->GetGPUDescriptorHandleForHeapStart();

    // =========================================================================
    // [2] ImGui Core ������
    // - Docking ��L�����F�G�f�B�^�I UI ���\�z����O��ɂȂ�B
    // - INI �t�@�C���F�v���W�F�N�g���΂ŌŒ肷��ƁA���C�A�E�g���L/�ǐՂ����₷���B
    //   �i���[�U�[���[�J���ɂ�������� OS �����AppData�ɂ��铙�A�^�p���[���Ō��߂�j
    // =========================================================================
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Docking/�}���`�r���[�|�[�g���g���ꍇ�͂����� Flags ��ǉ�����i����� Docking �̂݁j
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // ���C�A�E�g�t�@�C���̕ۑ���i���΃p�X�j�F���s�f�B���N�g����B���݂��Ȃ��ꍇ�͎����쐬�B
    ImGui::GetIO().IniFilename = "Editor/EditorLayout.ini";

    // �f�t�H���g�̔z�F�i�_�[�N�j�B�v���W�F�N�g�̃K�C�h���C���ɍ��킹�Ē����B
    ImGui::StyleColorsDark();

    // Win32 �o�b�N�G���h�i���́E�E�B���h�E�A�g�j
    ImGui_ImplWin32_Init(hwnd);

    // =========================================================================
    // [3] DX12 �o�b�N�G���h�������iNew Init API�j
    // - SrvDescriptorHeap�F��قǍ쐬�����q�[�v�BImGui �����̃t�H���gSRV�������ɍڂ�B
    // - LegacySingleSrv* �F�� API �݊��̂��߂ɕK�v�ɂȂ�ꕔ�p�X�̎w��i���s�ł��K�{�j�B
    // - RTV/DSV �t�H�[�}�b�g�F�ŏI�`���̃X���b�v�`�F�C���ɍ��킹��iDSV �͓K�X�j�B
    // - NumFramesInFlight�F�t���[�������O���B�����_���[�� FrameResources �ƈ�v������B
    // =========================================================================
    ImGui_ImplDX12_InitInfo ii{};
    ii.Device = device;
    ii.CommandQueue = queue;
    ii.NumFramesInFlight = numFramesInFlight;
    ii.RTVFormat = rtvFormat;                   // ��FDXGI_FORMAT_R8G8B8A8_UNORM
    ii.DSVFormat = DXGI_FORMAT_D32_FLOAT;       // DSV ���g��Ȃ��Ă��ݒ�͕K�v
    ii.SrvDescriptorHeap = m_srvHeap.Get();     // �t�H���gSRV���������ɍڂ���
    ii.LegacySingleSrvCpuDescriptor = m_srvCpuStart;
    ii.LegacySingleSrvGpuDescriptor = m_srvGpuStart;
    ImGui_ImplDX12_Init(&ii);

    // �t�H���g�i����̓f�t�H���g�j�B���{��/�A�C�R�����݂ɂ������ꍇ�͂����Œǉ�����B
    // ��F
    // ImFontConfig cfg; cfg.MergeMode = true; cfg.PixelSnapH = true; ...
    // io.Fonts->AddFontFromFileTTF("path/to/jp.ttf", 18.0f, &cfg, glyphRangesJP);
    ImGui::GetIO().Fonts->AddFontDefault();

    m_initialized = true;
    return true;
}

void ImGuiLayer::NewFrame()
{
    if (!m_initialized) return;

    // NewFrame �̏����� backend �Ɉˑ��FDX12 �� Win32 �� ImGui::NewFrame ����΁B
    // ���Ԃ�����Ɠ��͂�`���Ԃ������\��������̂ŌŒ肷�邱�ƁB
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

// ----------------------------------------------------------------------------
// BuildDockAndWindows
// - DockSpace ��S�ʂɕ~���A����܂��̓��Z�b�g�v������ DockBuilder �ŏ����z�u�B
// - �E�B���h�E�T�C�Y�ύX���́A�\�Ȍ���g��j��h�ɒǐ��iDockBuilderSetNodeSize�j�B
// - ��ʊO�ɏo�����h�b�N���́g�~�ρh���ă��[�N�̈���ɉ����߂��B
// - ���̓t�H�[�J�X/�z�o�[���� EditorInterop �o�R�ŃG���W���֋��n���B
// ----------------------------------------------------------------------------
void ImGuiLayer::BuildDockAndWindows(EditorContext& ctx)
{
    if (!m_initialized) return;

    // MainViewport�F�����r���[�|�[�g�@�\���g���ꍇ�͑����邪�A�{�����̓V���O���z��B
    const ImGuiViewport* vp = ImGui::GetMainViewport();

    // DockSpace �� ID �͌Œ蕶���񂩂琶���i�ς���ƕʂ̃��C�A�E�g�Ƃ��Ĉ�����j
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

    // DockSpace ����ʑS�̂ɃI�[�o�[���C�iPassthru �Œ����̔w�i�𓧉߁j
    ImGui::DockSpaceOverViewport(
        dockspace_id, vp, ImGuiDockNodeFlags_PassthruCentralNode, nullptr);

    // --- ���C�����j���[�i�K�v�Œ���̂݁j ---
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Editor")) {
            ImGui::MenuItem("Enable Editor", nullptr, ctx.pEnableEditor);         // Editor UI �S�̗̂L��/����
            if (ctx.pRequestResetLayout && ImGui::MenuItem("Reset Editor Layout"))
                *ctx.pRequestResetLayout = true;                                  // ���t���[���Ń��C�A�E�g�č\�z
            if (ctx.pAutoRelayout)
                ImGui::MenuItem("Keep ratio on resize", nullptr, ctx.pAutoRelayout); // Dock �m�[�h�T�C�Y�������ǐ�
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // ���񔻒� & ���T�C�Y���m�iWorkSize�F���j���[�o�[��^�X�N�o�[����������Ɨ̈�j
    static bool  s_first = true;
    static ImVec2 s_lastWorkSize = ImVec2(0, 0);

    const bool resized = (s_lastWorkSize.x != vp->WorkSize.x) || (s_lastWorkSize.y != vp->WorkSize.y);
    s_lastWorkSize = vp->WorkSize;

    const bool request_reset = (ctx.pRequestResetLayout && *ctx.pRequestResetLayout);
    const bool need_build = s_first || request_reset;

    // === Dock �����z�u�i���� or ���[�U�[���Z�b�g�j ===
    if (need_build) {
        s_first = false;
        if (ctx.pRequestResetLayout) *ctx.pRequestResetLayout = false;

        // �����m�[�h��j�� �� �V�K DockSpace ������ăT�C�Y�����݂� Work �ɍ��킹��
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, vp->WorkSize);

        ImGuiID dock_main = dockspace_id;

        // �������C�A�E�g�F
        //   ���FInspector�i��j/ Hierarchy�i���j
        //   �E�FScene�i��j/ Game�i���j
        // �����䗦�́g�킩��₷���䗦�h���̗p�i0.25, 0.50�j�B�v���W�F�N�g�ōD�݂�������₷���B
        ImGuiID dock_left, dock_left_bottom, dock_center, dock_center_bottom;
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.25f, &dock_left, &dock_main);
        ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.50f, &dock_left_bottom, &dock_left);
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.50f, &dock_center_bottom, &dock_center);

        // ���ꂼ��̃^�u�����蓖��
        ImGui::DockBuilderDockWindow("Inspector", dock_left);
        ImGui::DockBuilderDockWindow("Hierarchy", dock_left_bottom);
        ImGui::DockBuilderDockWindow("Scene", dock_center);
        ImGui::DockBuilderDockWindow("Game", dock_center_bottom);

        ImGui::DockBuilderFinish(dockspace_id);
    }
    // === �P�Ȃ郊�T�C�Y���F�m�[�h�T�C�Y�̂ݒǐ��i�j��I�Ĕz�u�͔�����j ===
    else if (resized && ctx.pAutoRelayout && *ctx.pAutoRelayout) {
        if (ImGui::DockBuilderGetNode(dockspace_id))
            ImGui::DockBuilderSetNodeSize(dockspace_id, vp->WorkSize);
    }

    // === ��ʊO�ɏo���\���̂���g���h�b�N���h�̈ʒu/�T�C�Y���~�� ===
    // �W���@�̉𑜓x�ύX��E�B���h�E�ړ��ŋH�ɔ����BDock ���̑��͑ΏۊO�B
    if (resized) {
        ImRect work(vp->WorkPos, vp->WorkPos + vp->WorkSize);

        auto clamp_window_into_work = [&](const char* name)
            {
                ImGuiWindow* w = ImGui::FindWindowByName(name);
                if (!w) return;
                if (w->DockIsActive) return; // �h�b�N���Ȃ�s�v

                const float  margin = 12.0f;            // �[�ɂׂ�����\��t���Ȃ����߂̗]��
                const ImVec2 min_size = ImVec2(160, 120); // �Œ��������T�C�Y
                ImRect work(vp->WorkPos, vp->WorkPos + vp->WorkSize);

                // AutoResize �w��̏ꍇ�́u���t���[���̑z��T�C�Y�v�ň��S���ɒ���
                const bool auto_resize = (w->Flags & ImGuiWindowFlags_AlwaysAutoResize) != 0;
                ImVec2 size = auto_resize ? ImGui::CalcWindowNextAutoFitSize(w) : w->SizeFull;

                // ��Ɨ̈�Ɏ��܂�ő�T�C�Y�ɃN�����v
                ImVec2 max_size(
                    ImMax(min_size.x, work.GetWidth() - 2.0f * margin),
                    ImMax(min_size.y, work.GetHeight() - 2.0f * margin)
                );
                size.x = ImClamp(size.x, min_size.x, max_size.x);
                size.y = ImClamp(size.y, min_size.y, max_size.y);

                // ����������Ɨ̈悪�ɒ[�ɏ������Ƃ��F�����ɋ����z�u
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

                // ��ʓ��ɓ���悤�ɕ��s�ړ�
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

        // ��v�E�B���h�E�̂݋~�ρi�s�v�ȕ���p�������j
        clamp_window_into_work("Inspector");
        clamp_window_into_work("Hierarchy");
        clamp_window_into_work("Scene");
        clamp_window_into_work("Game");
        clamp_window_into_work("Stats");
    }

    // --- �y�� Stats�i���s���m�F�p�F�r���h�ɂ͉e�����Ȃ��j ---
    ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("FPS: %.1f", ctx.fps);
    ImGui::Text("Size: %u x %u", ctx.rtWidth, ctx.rtHeight); // ���ǂ� RT �̂��Ƃ��͌Ăяo�����̉^�p����
    ImGui::End();

    // Scene ���͉ۂ̏������i���t���[�� false �� �Y���E�B�W�F�b�g�� true �ɏ㏑������j
    EditorInterop::SetSceneHovered(false);
    EditorInterop::SetSceneFocused(false);

    // ========================================================================
    // Editor UI �{�́FEditorContext �̃t���O���L���ȂƂ��̂ݕ`��
    // ========================================================================
    if (ctx.pEnableEditor && *ctx.pEnableEditor)
    {
        // --------------------------------------------------------------------
        // Scene �E�B���h�E
        // - �{�E�B���h�E�́g�\����h�ɓO����FRT �̐���/�j��/���T�C�Y�v���̔��f�͌Ăяo�����B
        // - �������u��]�r���[�|�[�g�T�C�Y�v�͂����ŎZ�o���� ctx �ɕԂ��i���������Đ�������ۂj�B
        // --------------------------------------------------------------------
        {
            // �e�E�B���h�E�̗]��/�g/�p�ۂ��E���āg�L�����o�X�h�ɂ���
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

            const ImGuiWindowFlags sceneFlags =
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse |
                ImGuiWindowFlags_NoCollapse;

            if (ImGui::Begin("Scene", nullptr, sceneFlags))
            {
                // �q�iSceneCanvas�j�őS�ʃq�b�g�e�X�g���w�i�h����s���iNoBackground �œ�d�`�����j
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
                ImGuiWindowFlags childFlags =
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoBackground;
                ImGui::BeginChild("SceneCanvas", ImVec2(0, 0), false, childFlags);

                // �p�̈�ƃX�N���[�����W�i��GetCursorScreenPos �̓X�N���[�����W�ɒ��j
                ImVec2 avail = ImFloor(ImGui::GetContentRegionAvail());
                ImVec2 p0 = ImFloor(ImGui::GetCursorScreenPos());
                ImVec2 p1 = p0 + avail;

                // InvisibleButton �œ��͂��m���ɕߑ��i�E�B���h�E����ɋz���Ȃ��j
                ImGui::InvisibleButton("##SceneInputCatcher", avail,
                    ImGuiButtonFlags_MouseButtonLeft |
                    ImGuiButtonFlags_MouseButtonRight |
                    ImGuiButtonFlags_MouseButtonMiddle);

                // �z�o�[/�t�H�[�J�X �� �G���W�����̃J��������/�M�Y�����̉۔��f�ɗ��p
                ctx.sceneHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                ctx.sceneFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
                EditorInterop::SetSceneHovered(ctx.sceneHovered);
                EditorInterop::SetSceneFocused(ctx.sceneFocused);

                ImDrawList* dl = ImGui::GetWindowDrawList();
                // �q�w�i�͎��O�œh��FNoBackground �̂���
                dl->AddRectFilled(p0, p1, IM_COL32(30, 30, 30, 255));

                if (ctx.sceneTexId && ctx.sceneRTWidth > 0 && ctx.sceneRTHeight > 0)
                {
                    // ���^�[�{�b�N�X/�s���[�{�b�N�X�F�A�X�y�N�g�ێ��ōő剻
                    const float texAspect = (float)ctx.sceneRTWidth / (float)ctx.sceneRTHeight;
                    const float winAspect = (avail.y > 0.0f) ? (avail.x / avail.y) : texAspect;

                    float w, h;
                    if (winAspect > texAspect) { w = avail.x; h = w / texAspect; }
                    else { h = avail.y; w = h * texAspect; }

                    // �������i���d�v�j�F
                    //  - DX �̃X�i�b�v/�T���v�����O�̓s���� 1px �̉���W���M���o�₷���̂�}����B
                    //  - �g�������͂��������h�̋K��ɂ��āA�����_���[���Ƃ̓�d�ۂ߂��֎~�B
                    auto evenf = [](float v) { int i = (int)v; i &= ~1; if (i < 2) i = 2; return (float)i; };
                    w = evenf(w); h = evenf(h);

                    // �����ɔz�u�iFit�j
                    ImVec2 size(w, h);
                    ImVec2 center((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
                    ImVec2 q0(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
                    ImVec2 q1(center.x + size.x * 0.5f, center.y + size.y * 0.5f);

                    // ���e�N�Z�����s�N�Z���X�i�b�v�t���œ\��
                    AddImage_NoEdge(dl, ctx.sceneTexId, ImFloor(q0), ImFloor(q1),
                        (int)ctx.sceneRTWidth, (int)ctx.sceneRTHeight);
                }
                else {
                    // SRV ���ݒ�̂Ƃ��̃K�C�h�i�f�o�b�O�x���j
                    ImGui::SetCursorScreenPos(p0 + ImVec2(6, 6));
                    ImGui::TextDisabled("Scene View (no SRV set)");
                }

                // �����_���[���ɕԂ��g��]�r���[�|�[�g�T�C�Y�h�i�������j�F
                //  - ���̒l������ RT ���T�C�Y����݌v�ɂ���ƁAUI �ƃ����_���[�ł̐���������B
                auto evenf_sz = [](float v) { int i = (int)v; i &= ~1; if (i < 2) i = 2; return (float)i; };
                ctx.sceneViewportSize = ImVec2(evenf_sz(avail.x), evenf_sz(avail.y));

                ImGui::EndChild();
                ImGui::PopStyleVar(); // ChildBorderSize
            }
            ImGui::End();

            ImGui::PopStyleVar(3); // WindowPadding, WindowBorderSize, WindowRounding
        }

        // --------------------------------------------------------------------
        // Game �E�B���h�E
        // - �@�\�FFit + �����z�u + �Y�[���i�z�C�[�� / Ctrl+0 �� 100%�j
        // - ���j�FScene �ƈႢ�g�������͍s��Ȃ��h �� ���[�U�[���_�̃Y�[������̑f������D��B
        //         �i�K�v�Ȃ瓯�l�̋��������[���𓱓����Ă��悢���AUI�������ڂ₯��\���ɒ��Ӂj
        // --------------------------------------------------------------------
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            const ImGuiWindowFlags gameFlags =
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

            if (ImGui::Begin("Game", nullptr, gameFlags))
            {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                avail.x = floorf(avail.x);
                avail.y = floorf(avail.y);

                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 p1 = ImVec2(p0.x + avail.x, p0.y + avail.y);

                // ���͂��m���ɃL���b�`�i�h���b�O��X�N���[�����E�B���h�E�ɒD���Ȃ��j
                ImGui::InvisibleButton("##GameInputCatcher", avail,
                    ImGuiButtonFlags_MouseButtonLeft |
                    ImGuiButtonFlags_MouseButtonRight |
                    ImGuiButtonFlags_MouseButtonMiddle);

                // �Ăяo�����i�Q�[�����W�b�N�j�̔���p�t���O
                ctx.gameViewportSize = avail;
                ctx.gameHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                ctx.gameFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(p0, p1, IM_COL32(30, 30, 30, 255)); // ���^�{�w�i

                // ==== �Y�[���i��Ԃ͐ÓI�ɕێ��F���C�A�E�g���Z�b�g�ł͏��������Ȃ��z��j====
                static float s_gameScale = 1.0f;        // ���݂̃Y�[���{��
                static const float s_minScale = 1.0f;   // �����FFit �� 1.0 �Ƃ���
                static const float s_maxScale = 4.0f;   // ����F�K�v�ɉ�������

                if (ctx.gameHovered) {
                    ImGuiIO& io = ImGui::GetIO();
                    float wheel = io.MouseWheel;
                    if (wheel != 0.0f) {
                        // ��Z�Y�[���F�ׂ����X���[�Y�ɓ���
                        s_gameScale *= (wheel > 0 ? 1.1f : 1.0f / 1.1f);
                    }
                    // Ctrl+0�F100%�iFit�j�ɖ߂��V���[�g�J�b�g
                    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_0)) {
                        s_gameScale = 1.0f;
                    }
                }
                s_gameScale = ImClamp(s_gameScale, s_minScale, s_maxScale);

                if (ctx.gameTexId && ctx.gameRTWidth > 0 && ctx.gameRTHeight > 0)
                {
                    const float texAspect = (float)ctx.gameRTWidth / (float)ctx.gameRTHeight;

                    // Fit�i�A�X�y�N�g�ێ��j
                    float w = avail.x, h = avail.y;
                    if (w / h > texAspect) { w = h * texAspect; }
                    else { h = w / texAspect; }

                    // �Y�[���K�p
                    w *= s_gameScale;
                    h *= s_gameScale;

                    // �����z�u
                    ImVec2 size(w, h);
                    ImVec2 center = ImVec2((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
                    ImVec2 q0 = ImVec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
                    ImVec2 q1 = ImVec2(center.x + size.x * 0.5f, center.y + size.y * 0.5f);

                    AddImage_NoEdge(dl, ctx.gameTexId, q0, q1,
                        (int)ctx.gameRTWidth, (int)ctx.gameRTHeight);

                    // �E���ɊȈ� HUD�i���݂̃Y�[���{���j
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

        // --------------------------------------------------------------------
        // Inspector / Hierarchy
        // - �Ӗ��F�E�B���h�E�̊킾���񋟁B���g�̕`��� EditorContext �̊֐��ɈϏ��B
        // - ���L�F�I����Ԃ�v���p�e�B�� Editor ���ŕێ��iImGui �w�͏�Ԃ������Ȃ����j�j�B
        // --------------------------------------------------------------------
        ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse);
        if (ctx.DrawInspector) ctx.DrawInspector();
        ImGui::End();

        ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoCollapse);
        if (ctx.DrawHierarchy) ctx.DrawHierarchy();
        ImGui::End();
    }
}

// ----------------------------------------------------------------------------
// CreateOrUpdateTextureSRV
// - �ړI�F�w��X���b�g�� 2D SRV ���쐬/�㏑�����AImGui �Ŏg���� ImTextureID�iGPU��Ǔ_�j��Ԃ��B
// - �Ԓl�FDX12 �� ImTextureID �� D3D12_GPU_DESCRIPTOR_HANDLE.ptr �����̂܂܊i�[��������B
// - ���ӁFslot=0 �̓t�H���g�g�p���O��i�\��j�B���[�U�[�� 1 �ȍ~���g���K��ɂ��邱�ƁB
//   �����F���O�x�[�X�̓o�^�i�ȈՃA���P�[�^�j��݂���ƁA�d�����蓖�Ď��̂�h����B
// ----------------------------------------------------------------------------
ImTextureID ImGuiLayer::CreateOrUpdateTextureSRV(ID3D12Resource* tex, DXGI_FORMAT fmt, UINT slot)
{
    // SRV �q�[�v�擪���� slot ��������΂��iCPU/GPU �̗����Łj
    D3D12_CPU_DESCRIPTOR_HANDLE cpu = m_srvCpuStart;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu = m_srvGpuStart;
    cpu.ptr += SIZE_T(m_srvIncSize) * slot;
    gpu.ptr += SIZE_T(m_srvIncSize) * slot;

    D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = fmt;                                     // ��FDXGI_FORMAT_R8G8B8A8_UNORM
    sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sd.Texture2D.MipLevels = 1;                          // RT �𒼓\�肷��z��Ȃ̂� Mip=1�i�K�X�ύX�j

    // ���L���FSRV �́g�q�[�v�̃X���b�g�h�ɕR�t���Btex �̎����Ǘ��͌Ăяo�����i�����_���[�j�B
    m_device->CreateShaderResourceView(tex, &sd, cpu);

    // DX12 backend �� ImTextureID �� GPU �n���h�������҂���
    return (ImTextureID)gpu.ptr;
}

void ImGuiLayer::Render(ID3D12GraphicsCommandList* cmd)
{
    if (!m_initialized) return;

    // ImGui �Ɏg�킹���� SRV �q�[�v���o�C���h�iDX12 �͖����o�C���h�K�{�j
    ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
    cmd->SetDescriptorHeaps(1, heaps);

    // ImGui �̕`��f�[�^���R�}���h���X�g�֋L�^
    // ���ӁF������ �gImGui �̕`��̂݁h�B�V�[���`��Ȃǂ͌Ăяo�����Ő�ɋL�^�ς݂̑z��B
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);
}

// ----------------------------------------------------------------------------
// Shutdown
// - �ړI�F�o�b�N�G���h�� ImGui �R���e�L�X�g��j���BSRV �q�[�v������B
// - �O��F�Ăяo������ GPU �̊����҂����ς܂��Ă��痈��i��n���̏������d�v�j�B
// - ���ӁF������Ă΂�Ă����S�im_initialized �t���O�ŃK�[�h�j�B
// ----------------------------------------------------------------------------
void ImGuiLayer::Shutdown()
{
    if (!m_initialized) return;

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // SRV �q�[�v�͖{�N���X���L�Ȃ̂ł����Ŕj���i���Ŏg���񂳂Ȃ��݌v�j
    m_srvHeap.Reset();

    m_initialized = false;
}
