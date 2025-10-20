// ============================================================================
// ImGuiLayer.cpp  ―  DX12 + Win32 版 ImGui の薄いラッパ
// 目的：初期化／Dockレイアウト／表示キャンバス矩形計算／テクスチャ貼り付けのみを担当。
//       ・“描画リソースの生成/破棄/コマンド記録”はレンダラー側（エンジン）に完全委譲
//       ・“データ/状態”は EditorContext 経由（ImGui 層はステートレス志向）
// ポイント：後から読んでも迷わないよう、設計判断の理由と注意点をコメントで明示。
// 依存：imgui_impl_win32 / imgui_impl_dx12（v1.92+New Init API）
// ----------------------------------------------------------------------------
// 変更しやすい箇所：
//  - INI保存先:      ImGui::GetIO().IniFilename
//  - Dock初期配置:   BuildDockAndWindows() の DockBuilder* セクション
//  - SRV割り当て:    CreateOrUpdateTextureSRV() の slot 管理（簡易アロケータ化余地）
//  - レタボ/ピラボ:  AddImage_NoEdge()＋Scene/Game の矩形計算
//  - 偶数化規約:     “偶数化は ImGuiLayer のみで行う”（二重丸め禁止）
// ----------------------------------------------------------------------------
// スレッド前提：メインスレッド専用（ImGui はスレッドセーフではない）
// リソース寿命：Shutdown() 以前に GPU 側は Wait 済の想定（呼び出し側で同期管理）
// 座標系メモ：ImGui は基本 float ピクセル座標（DPI スケールは ImGui 側が吸収）
// ============================================================================

#define IMGUI_DEFINE_MATH_OPERATORS
#include "ImGuiLayer.h"

// Editor 側の状態と、エンジンへのブリッジ（フォーカス/ホバー通知）
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
// [ユーティリティ] AddImage_NoEdge
// - 用途：テクスチャ端の“にじみ”や 1px 縁の出現を軽減する標準手法。
// - 方法：UV を半テクセル内側に寄せる（境界を跨ぐサンプリングを避ける）＋矩形を整数座標へ丸め。
// - 想定対象：レンダーターゲット（Scene/Game RT）や UI テクスチャ。
// - 副作用：ミップやフィルタが強い場合は完全に消えないこともある（本関数は first aid）。
//   → より厳密にやるなら“外周に1pxのクランプ用パディング”を持つRT設計も選択肢。
// ----------------------------------------------------------------------------
static inline void AddImage_NoEdge(ImDrawList* dl, ImTextureID tex,
    ImVec2 p0, ImVec2 p1,
    int texW, int texH)
{
    if (!tex || texW <= 0 || texH <= 0) return;

    // 半テクセル・オフセット（Direct3D 由来の定石）
    const float ueps = 0.5f / (float)texW;
    const float veps = 0.5f / (float)texH;
    ImVec2 uv0(ueps, veps);
    ImVec2 uv1(1.0f - ueps, 1.0f - veps);

    // ピクセルスナップ：ImGui は float 座標だが、表示側は整数境界に揃えた方がにじみにくい
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
    // 多重初期化の防止（呼び出し側が複数回呼んでも安全なように）
    if (m_initialized) return true;

    // =========================================================================
    // [1] SRV ディスクリプタヒープの作成
    // - ImGui のフォントSRV＋ユーザーのテクスチャSRV（Scene/Game 等）を同居させる。
    // - SHADER_VISIBLE 必須：描画中にピクセル/頂点シェーダから SRV 参照するため。
    // - NumDescriptors：将来拡張を見越し余裕を持たせる（64）。必要に応じて増やすこと。
    // - 所有権：本クラスが生成/保持/破棄。呼び出し側は SRV スロット番号だけ意識する。
    // =========================================================================
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 64;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvHeap))))
        return false;

    // DX12 ヒープのアドレス情報（CPU/GPU 両方）と増分サイズを保持
    m_device = device; // ※ 後段の CreateShaderResourceView で使用
    m_srvIncSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_srvCpuStart = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    m_srvGpuStart = m_srvHeap->GetGPUDescriptorHandleForHeapStart();

    // =========================================================================
    // [2] ImGui Core 初期化
    // - Docking を有効化：エディタ的 UI を構築する前提になる。
    // - INI ファイル：プロジェクト相対で固定すると、レイアウト共有/追跡がしやすい。
    //   （ユーザーローカルにしたければ OS 既定のAppDataにする等、運用ルールで決める）
    // =========================================================================
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Docking/マルチビューポートを使う場合はここで Flags を追加する（今回は Docking のみ）
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // レイアウトファイルの保存先（相対パス）：実行ディレクトリ基準。存在しない場合は自動作成。
    ImGui::GetIO().IniFilename = "Editor/EditorLayout.ini";

    // デフォルトの配色（ダーク）。プロジェクトのガイドラインに合わせて調整可。
    ImGui::StyleColorsDark();

    // Win32 バックエンド（入力・ウィンドウ連携）
    ImGui_ImplWin32_Init(hwnd);

    // =========================================================================
    // [3] DX12 バックエンド初期化（New Init API）
    // - SrvDescriptorHeap：先ほど作成したヒープ。ImGui 内部のフォントSRVもここに載る。
    // - LegacySingleSrv* ：旧 API 互換のために必要になる一部パスの指定（現行でも必須）。
    // - RTV/DSV フォーマット：最終描画先のスワップチェインに合わせる（DSV は適宜）。
    // - NumFramesInFlight：フレームリング数。レンダラーの FrameResources と一致させる。
    // =========================================================================
    ImGui_ImplDX12_InitInfo ii{};
    ii.Device = device;
    ii.CommandQueue = queue;
    ii.NumFramesInFlight = numFramesInFlight;
    ii.RTVFormat = rtvFormat;                   // 例：DXGI_FORMAT_R8G8B8A8_UNORM
    ii.DSVFormat = DXGI_FORMAT_D32_FLOAT;       // DSV を使わなくても設定は必要
    ii.SrvDescriptorHeap = m_srvHeap.Get();     // フォントSRV等をここに載せる
    ii.LegacySingleSrvCpuDescriptor = m_srvCpuStart;
    ii.LegacySingleSrvGpuDescriptor = m_srvGpuStart;
    ImGui_ImplDX12_Init(&ii);

    // フォント（現状はデフォルト）。日本語/アイコン混在にしたい場合はここで追加する。
    // 例：
    // ImFontConfig cfg; cfg.MergeMode = true; cfg.PixelSnapH = true; ...
    // io.Fonts->AddFontFromFileTTF("path/to/jp.ttf", 18.0f, &cfg, glyphRangesJP);
    ImGui::GetIO().Fonts->AddFontDefault();

    m_initialized = true;
    return true;
}

void ImGuiLayer::NewFrame()
{
    if (!m_initialized) return;

    // NewFrame の順序は backend に依存：DX12 → Win32 → ImGui::NewFrame が定石。
    // 順番を崩すと入力や描画状態がずれる可能性があるので固定すること。
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

// ----------------------------------------------------------------------------
// BuildDockAndWindows
// - DockSpace を全面に敷き、初回またはリセット要求時に DockBuilder で初期配置。
// - ウィンドウサイズ変更時は、可能な限り“非破壊”に追随（DockBuilderSetNodeSize）。
// - 画面外に出た未ドック窓は“救済”してワーク領域内に押し戻す。
// - 入力フォーカス/ホバー情報を EditorInterop 経由でエンジンへ橋渡し。
// ----------------------------------------------------------------------------
void ImGuiLayer::BuildDockAndWindows(EditorContext& ctx)
{
    if (!m_initialized) return;

    // MainViewport：複数ビューポート機能を使う場合は増えるが、本実装はシングル想定。
    const ImGuiViewport* vp = ImGui::GetMainViewport();

    // DockSpace の ID は固定文字列から生成（変えると別のレイアウトとして扱われる）
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

    // DockSpace を画面全体にオーバーレイ（Passthru で中央の背景を透過）
    ImGui::DockSpaceOverViewport(
        dockspace_id, vp, ImGuiDockNodeFlags_PassthruCentralNode, nullptr);

    // --- メインメニュー（必要最低限のみ） ---
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Editor")) {
            ImGui::MenuItem("Enable Editor", nullptr, ctx.pEnableEditor);         // Editor UI 全体の有効/無効
            if (ctx.pRequestResetLayout && ImGui::MenuItem("Reset Editor Layout"))
                *ctx.pRequestResetLayout = true;                                  // 次フレームでレイアウト再構築
            if (ctx.pAutoRelayout)
                ImGui::MenuItem("Keep ratio on resize", nullptr, ctx.pAutoRelayout); // Dock ノードサイズを自動追随
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // 初回判定 & リサイズ検知（WorkSize：メニューバーやタスクバーを除いた作業領域）
    static bool  s_first = true;
    static ImVec2 s_lastWorkSize = ImVec2(0, 0);

    const bool resized = (s_lastWorkSize.x != vp->WorkSize.x) || (s_lastWorkSize.y != vp->WorkSize.y);
    s_lastWorkSize = vp->WorkSize;

    const bool request_reset = (ctx.pRequestResetLayout && *ctx.pRequestResetLayout);
    const bool need_build = s_first || request_reset;

    // === Dock 初期配置（初回 or ユーザーリセット） ===
    if (need_build) {
        s_first = false;
        if (ctx.pRequestResetLayout) *ctx.pRequestResetLayout = false;

        // 既存ノードを破棄 → 新規 DockSpace を作ってサイズを現在の Work に合わせる
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, vp->WorkSize);

        ImGuiID dock_main = dockspace_id;

        // 初期レイアウト：
        //   左：Inspector（上）/ Hierarchy（下）
        //   右：Scene（上）/ Game（下）
        // 分割比率は“わかりやすい比率”を採用（0.25, 0.50）。プロジェクトで好みが分かれやすい。
        ImGuiID dock_left, dock_left_bottom, dock_center, dock_center_bottom;
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.25f, &dock_left, &dock_main);
        ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.50f, &dock_left_bottom, &dock_left);
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.50f, &dock_center_bottom, &dock_center);

        // それぞれのタブを割り当て
        ImGui::DockBuilderDockWindow("Inspector", dock_left);
        ImGui::DockBuilderDockWindow("Hierarchy", dock_left_bottom);
        ImGui::DockBuilderDockWindow("Scene", dock_center);
        ImGui::DockBuilderDockWindow("Game", dock_center_bottom);

        ImGui::DockBuilderFinish(dockspace_id);
    }
    // === 単なるリサイズ時：ノードサイズのみ追随（破壊的再配置は避ける） ===
    else if (resized && ctx.pAutoRelayout && *ctx.pAutoRelayout) {
        if (ImGui::DockBuilderGetNode(dockspace_id))
            ImGui::DockBuilderSetNodeSize(dockspace_id, vp->WorkSize);
    }

    // === 画面外に出た可能性のある“未ドック窓”の位置/サイズを救済 ===
    // 展示機の解像度変更やウィンドウ移動で稀に発生。Dock 中の窓は対象外。
    if (resized) {
        ImRect work(vp->WorkPos, vp->WorkPos + vp->WorkSize);

        auto clamp_window_into_work = [&](const char* name)
            {
                ImGuiWindow* w = ImGui::FindWindowByName(name);
                if (!w) return;
                if (w->DockIsActive) return; // ドック中なら不要

                const float  margin = 12.0f;            // 端にべったり貼り付かないための余白
                const ImVec2 min_size = ImVec2(160, 120); // 最低限見えるサイズ
                ImRect work(vp->WorkPos, vp->WorkPos + vp->WorkSize);

                // AutoResize 指定の場合は「次フレームの想定サイズ」で安全側に調整
                const bool auto_resize = (w->Flags & ImGuiWindowFlags_AlwaysAutoResize) != 0;
                ImVec2 size = auto_resize ? ImGui::CalcWindowNextAutoFitSize(w) : w->SizeFull;

                // 作業領域に収まる最大サイズにクランプ
                ImVec2 max_size(
                    ImMax(min_size.x, work.GetWidth() - 2.0f * margin),
                    ImMax(min_size.y, work.GetHeight() - 2.0f * margin)
                );
                size.x = ImClamp(size.x, min_size.x, max_size.x);
                size.y = ImClamp(size.y, min_size.y, max_size.y);

                // そもそも作業領域が極端に小さいとき：中央に強制配置
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

                // 画面内に入るように平行移動
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

        // 主要ウィンドウのみ救済（不要な副作用を避ける）
        clamp_window_into_work("Inspector");
        clamp_window_into_work("Hierarchy");
        clamp_window_into_work("Scene");
        clamp_window_into_work("Game");
        clamp_window_into_work("Stats");
    }

    // --- 軽量 Stats（実行時確認用：ビルドには影響しない） ---
    ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("FPS: %.1f", ctx.fps);
    ImGui::Text("Size: %u x %u", ctx.rtWidth, ctx.rtHeight); // ※どの RT のことかは呼び出し側の運用次第
    ImGui::End();

    // Scene 入力可否の初期化（毎フレーム false → 該当ウィジェットで true に上書きする）
    EditorInterop::SetSceneHovered(false);
    EditorInterop::SetSceneFocused(false);

    // ========================================================================
    // Editor UI 本体：EditorContext のフラグが有効なときのみ描画
    // ========================================================================
    if (ctx.pEnableEditor && *ctx.pEnableEditor)
    {
        // --------------------------------------------------------------------
        // Scene ウィンドウ
        // - 本ウィンドウは“表示器”に徹する：RT の生成/破棄/リサイズ要求の判断は呼び出し側。
        // - ただし「希望ビューポートサイズ」はここで算出して ctx に返す（偶数化して整合性を保つ）。
        // --------------------------------------------------------------------
        {
            // 親ウィンドウの余白/枠/角丸を殺して“キャンバス”にする
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

            const ImGuiWindowFlags sceneFlags =
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse |
                ImGuiWindowFlags_NoCollapse;

            if (ImGui::Begin("Scene", nullptr, sceneFlags))
            {
                // 子（SceneCanvas）で全面ヒットテスト＆背景塗りを行う（NoBackground で二重描画回避）
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
                ImGuiWindowFlags childFlags =
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoBackground;
                ImGui::BeginChild("SceneCanvas", ImVec2(0, 0), false, childFlags);

                // 可用領域とスクリーン座標（※GetCursorScreenPos はスクリーン座標に直）
                ImVec2 avail = ImFloor(ImGui::GetContentRegionAvail());
                ImVec2 p0 = ImFloor(ImGui::GetCursorScreenPos());
                ImVec2 p1 = p0 + avail;

                // InvisibleButton で入力を確実に捕捉（ウィンドウ操作に吸われない）
                ImGui::InvisibleButton("##SceneInputCatcher", avail,
                    ImGuiButtonFlags_MouseButtonLeft |
                    ImGuiButtonFlags_MouseButtonRight |
                    ImGuiButtonFlags_MouseButtonMiddle);

                // ホバー/フォーカス → エンジン側のカメラ操作/ギズモ等の可否判断に利用
                ctx.sceneHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                ctx.sceneFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
                EditorInterop::SetSceneHovered(ctx.sceneHovered);
                EditorInterop::SetSceneFocused(ctx.sceneFocused);

                ImDrawList* dl = ImGui::GetWindowDrawList();
                // 子背景は自前で塗る：NoBackground のため
                dl->AddRectFilled(p0, p1, IM_COL32(30, 30, 30, 255));

                if (ctx.sceneTexId && ctx.sceneRTWidth > 0 && ctx.sceneRTHeight > 0)
                {
                    // レターボックス/ピラーボックス：アスペクト維持で最大化
                    const float texAspect = (float)ctx.sceneRTWidth / (float)ctx.sceneRTHeight;
                    const float winAspect = (avail.y > 0.0f) ? (avail.x / avail.y) : texAspect;

                    float w, h;
                    if (winAspect > texAspect) { w = avail.x; h = w / texAspect; }
                    else { h = avail.y; w = h * texAspect; }

                    // 偶数化（※重要）：
                    //  - DX のスナップ/サンプリングの都合で 1px の縁やジャギが出やすいのを抑える。
                    //  - “偶数化はここだけ”の規約にして、レンダラー側との二重丸めを禁止。
                    auto evenf = [](float v) { int i = (int)v; i &= ~1; if (i < 2) i = 2; return (float)i; };
                    w = evenf(w); h = evenf(h);

                    // 中央に配置（Fit）
                    ImVec2 size(w, h);
                    ImVec2 center((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
                    ImVec2 q0(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
                    ImVec2 q1(center.x + size.x * 0.5f, center.y + size.y * 0.5f);

                    // 半テクセル＆ピクセルスナップ付きで貼る
                    AddImage_NoEdge(dl, ctx.sceneTexId, ImFloor(q0), ImFloor(q1),
                        (int)ctx.sceneRTWidth, (int)ctx.sceneRTHeight);
                }
                else {
                    // SRV 未設定のときのガイド（デバッグ支援）
                    ImGui::SetCursorScreenPos(p0 + ImVec2(6, 6));
                    ImGui::TextDisabled("Scene View (no SRV set)");
                }

                // レンダラー側に返す“希望ビューポートサイズ”（偶数化）：
                //  - この値を元に RT リサイズする設計にすると、UI とレンダラーでの整合が取れる。
                auto evenf_sz = [](float v) { int i = (int)v; i &= ~1; if (i < 2) i = 2; return (float)i; };
                ctx.sceneViewportSize = ImVec2(evenf_sz(avail.x), evenf_sz(avail.y));

                ImGui::EndChild();
                ImGui::PopStyleVar(); // ChildBorderSize
            }
            ImGui::End();

            ImGui::PopStyleVar(3); // WindowPadding, WindowBorderSize, WindowRounding
        }

        // --------------------------------------------------------------------
        // Game ウィンドウ
        // - 機能：Fit + 中央配置 + ズーム（ホイール / Ctrl+0 で 100%）
        // - 方針：Scene と違い“偶数化は行わない” → ユーザー視点のズーム操作の素直さを優先。
        //         （必要なら同様の偶数化ルールを導入してもよいが、UI文字がぼやける可能性に注意）
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

                // 入力を確実にキャッチ（ドラッグやスクロールがウィンドウに奪われない）
                ImGui::InvisibleButton("##GameInputCatcher", avail,
                    ImGuiButtonFlags_MouseButtonLeft |
                    ImGuiButtonFlags_MouseButtonRight |
                    ImGuiButtonFlags_MouseButtonMiddle);

                // 呼び出し側（ゲームロジック）の判定用フラグ
                ctx.gameViewportSize = avail;
                ctx.gameHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                ctx.gameFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(p0, p1, IM_COL32(30, 30, 30, 255)); // レタボ背景

                // ==== ズーム（状態は静的に保持：レイアウトリセットでは初期化しない想定）====
                static float s_gameScale = 1.0f;        // 現在のズーム倍率
                static const float s_minScale = 1.0f;   // 下限：Fit を 1.0 とする
                static const float s_maxScale = 4.0f;   // 上限：必要に応じ調整

                if (ctx.gameHovered) {
                    ImGuiIO& io = ImGui::GetIO();
                    float wheel = io.MouseWheel;
                    if (wheel != 0.0f) {
                        // 乗算ズーム：細かくスムーズに動く
                        s_gameScale *= (wheel > 0 ? 1.1f : 1.0f / 1.1f);
                    }
                    // Ctrl+0：100%（Fit）に戻すショートカット
                    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_0)) {
                        s_gameScale = 1.0f;
                    }
                }
                s_gameScale = ImClamp(s_gameScale, s_minScale, s_maxScale);

                if (ctx.gameTexId && ctx.gameRTWidth > 0 && ctx.gameRTHeight > 0)
                {
                    const float texAspect = (float)ctx.gameRTWidth / (float)ctx.gameRTHeight;

                    // Fit（アスペクト維持）
                    float w = avail.x, h = avail.y;
                    if (w / h > texAspect) { w = h * texAspect; }
                    else { h = w / texAspect; }

                    // ズーム適用
                    w *= s_gameScale;
                    h *= s_gameScale;

                    // 中央配置
                    ImVec2 size(w, h);
                    ImVec2 center = ImVec2((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
                    ImVec2 q0 = ImVec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
                    ImVec2 q1 = ImVec2(center.x + size.x * 0.5f, center.y + size.y * 0.5f);

                    AddImage_NoEdge(dl, ctx.gameTexId, q0, q1,
                        (int)ctx.gameRTWidth, (int)ctx.gameRTHeight);

                    // 右下に簡易 HUD（現在のズーム倍率）
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
        // - 責務：ウィンドウの器だけ提供。中身の描画は EditorContext の関数に委譲。
        // - 所有：選択状態やプロパティは Editor 側で保持（ImGui 層は状態を持たない方針）。
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
// - 目的：指定スロットに 2D SRV を作成/上書きし、ImGui で使える ImTextureID（GPU句読点）を返す。
// - 返値：DX12 の ImTextureID は D3D12_GPU_DESCRIPTOR_HANDLE.ptr をそのまま格納する実装。
// - 注意：slot=0 はフォント使用が前提（予約）。ユーザーは 1 以降を使う規約にすること。
//   将来：名前ベースの登録（簡易アロケータ）を設けると、重複割り当て事故を防げる。
// ----------------------------------------------------------------------------
ImTextureID ImGuiLayer::CreateOrUpdateTextureSRV(ID3D12Resource* tex, DXGI_FORMAT fmt, UINT slot)
{
    // SRV ヒープ先頭から slot 分だけ飛ばす（CPU/GPU の両方で）
    D3D12_CPU_DESCRIPTOR_HANDLE cpu = m_srvCpuStart;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu = m_srvGpuStart;
    cpu.ptr += SIZE_T(m_srvIncSize) * slot;
    gpu.ptr += SIZE_T(m_srvIncSize) * slot;

    D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = fmt;                                     // 例：DXGI_FORMAT_R8G8B8A8_UNORM
    sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sd.Texture2D.MipLevels = 1;                          // RT を直貼りする想定なので Mip=1（適宜変更可）

    // 所有権：SRV は“ヒープのスロット”に紐付く。tex の寿命管理は呼び出し側（レンダラー）。
    m_device->CreateShaderResourceView(tex, &sd, cpu);

    // DX12 backend は ImTextureID に GPU ハンドルを期待する
    return (ImTextureID)gpu.ptr;
}

void ImGuiLayer::Render(ID3D12GraphicsCommandList* cmd)
{
    if (!m_initialized) return;

    // ImGui に使わせたい SRV ヒープをバインド（DX12 は明示バインド必須）
    ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
    cmd->SetDescriptorHeaps(1, heaps);

    // ImGui の描画データをコマンドリストへ記録
    // 注意：ここは “ImGui の描画のみ”。シーン描画などは呼び出し側で先に記録済みの想定。
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);
}

// ----------------------------------------------------------------------------
// Shutdown
// - 目的：バックエンドと ImGui コンテキストを破棄。SRV ヒープも解放。
// - 前提：呼び出し側で GPU の完了待ちを済ませてから来る（後始末の順序が重要）。
// - 注意：複数回呼ばれても安全（m_initialized フラグでガード）。
// ----------------------------------------------------------------------------
void ImGuiLayer::Shutdown()
{
    if (!m_initialized) return;

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // SRV ヒープは本クラス所有なのでここで破棄（他で使い回さない設計）
    m_srvHeap.Reset();

    m_initialized = false;
}
