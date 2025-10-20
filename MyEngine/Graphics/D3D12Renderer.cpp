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

/*
    D3D12Renderer
    ----------------------------------------------------------------------------
    役割：
      - DX12 デバイス/スワップチェイン/コマンド/フェンスなど“土台”の生成破棄
      - フレームスケジューリング（Begin/End、Present とシグナル）
      - シーン描画（SceneLayer 経由で Scene/Game のオフスクリーン描画）
      - ImGui のセットアップと描画
      - RT の遅延破棄（GpuGarbageQueue に渡す）

    メモ：
      - SceneLayer は SceneRenderer と Viewports を内包しており、
        描画と RT リサイズ・SRV 供給をまとめて扱う。
*/
D3D12Renderer::D3D12Renderer() {}
D3D12Renderer::~D3D12Renderer() { Cleanup(); }

//==============================================================================
// Initialize
//==============================================================================

/*
    Initialize
    ----------------------------------------------------------------------------
    引数：
      - hwnd   : Win32 ウィンドウハンドル
      - width  : 初期ウィンドウ幅
      - height : 初期ウィンドウ高

    流れ：
      1) DeviceResources（デバイス/スワップチェイン/RTV/DSV/キュー）
      2) FrameResources（各フレームのコマンドアロケータ＋アップロード CB）
      3) フェンスとイベント
      4) パイプライン（Lambert）構築
      5) SceneLayer 初期化（初期サイズを渡して RT を生成）
      6) ImGui 初期化（DX12/Win32 バックエンド）
      7) フレームスケジューラ初期化
      8) カメラのアスペクト更新（既にカメラがある場合）

    失敗時は false を返す（上位でアプリを終了させる等）
*/
bool D3D12Renderer::Initialize(HWND hwnd, UINT width, UINT height)
{
    // Device/SwapChain：内部で RTV/DSV も生成
    m_dev = std::make_unique<DeviceResources>();
    if (!m_dev->Initialize(hwnd, width, height, FrameCount))
        return false;

    ID3D12Device* dev = m_dev->GetDevice();

    // FrameResources：各フレームにコマンドアロケータと Upload CB を用意
    // ※ MaxObjects×2：Scene と Game の 2 セット分を 1 フレームで使い分ける想定
    if (!m_frames.Initialize(dev, FrameCount, sizeof(SceneConstantBuffer), MaxObjects * 2))
        return false;

    // Fence：CPU-GPU 同期のためのフェンスと OS イベント
    HRESULT hr = dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr)) return false;
    m_nextFence = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) return false;

    // PSO（Lambert）：シンプルな頂点/ピクセルシェーダによるライティング
    if (!BuildLambertPipeline(dev, m_dev->GetRTVFormat(), m_dev->GetDSVFormat(), m_pipe))
        return false;

    // SceneLayer 初期化：
    //   - 内部で Viewports.Initialize(dev, width, height) が呼ばれ、Scene/Game の RT 作成
    //   - “初期サイズを渡す”ことで拡大直後のぼやけを避ける（最初から十分な解像度）
    m_sceneLayer.Initialize(dev, m_dev->GetRTVFormat(), m_dev->GetDSVFormat(),
        &m_frames, m_pipe, width, height);

    // ImGui：DX12/Win32 バックエンドの初期化
    m_imgui = std::make_unique<ImGuiLayer>();
    if (!m_imgui->Initialize(hwnd, dev, m_dev->GetQueue(),
        m_dev->GetRTVFormat(), m_dev->GetDSVFormat(), FrameCount))
        return false;

    // フレームスケジューラ（Present, Signal, 遅延破棄 Collect まで）
    m_scheduler.Initialize(m_dev.get(), m_fence.Get(), m_fenceEvent, &m_frames, &m_garbage);

    // ImGui の ini ファイル名（レイアウトの永続化）
    ImGui::GetIO().IniFilename = "EditorLayout.ini";

    // 既にカメラがある場合はアスペクトだけ同期（スワップチェインの比率）
    if (m_Camera)
        m_Camera->SetAspect(static_cast<float>(width) / static_cast<float>(height));

    return true;
}

//==============================================================================
// Render
//==============================================================================

/*
    Render
    ----------------------------------------------------------------------------
    1) BeginFrame：バックバッファインデックスの確定、前フレームの待ち、コマンドリストのリセット
    2) Viewports：前フレの“確定済みリサイズ”を適用（古い RT を detach）
       - detach された RT は今フレームの EndFrame で遅延破棄へ（最大 1 個）
       - 2 個目は carry-over（次のフレームの Begin で拾う）
    3) SceneLayer.Record：Scene/Game をオフスクリーンに描画
    4) Presenter.Begin：バックバッファを RT に遷移、クリア、VP/Scissor 設定
    5) ImGui：EditorContext を構築し、UI を描く
       - ウィンドウ（スワップチェイン）サイズはバックバッファ記述子から取得
       - Scene/Game の実 RT サイズは SceneLayer.SyncStatsTo() で同期
    6) Presenter.End：バックバッファを PRESENT へ遷移
    7) EndFrame：Present, Signal, Garbage 登録/回収
*/
void D3D12Renderer::Render()
{
    // --- 1) BeginFrame ---
    auto begin = m_scheduler.BeginFrame();
    const UINT fi = begin.frameIndex;
    ID3D12GraphicsCommandList* cmd = begin.cmd;

    // PSO 設定（固定であればここで一度セット）
    cmd->SetPipelineState(m_pipe.pso.Get());

    // --- 2) Viewports：確定リサイズの適用（古い RT を detach で受け取る） ---
    RenderTargetHandles deadScene = m_sceneLayer.BeginFrame(m_dev->GetDevice());

    // もし“今フレで捨てるもの”が無ければ、Viewports 内の持ち越し死骸を拾う（最大 1 個）
    auto isEmptyRT = [](const RenderTargetHandles& h) noexcept {
        return !h.color && !h.depth && !h.rtvHeap && !h.dsvHeap;
        };
    if (isEmptyRT(deadScene)) {
        // SceneLayer::TakeCarryOverDead() で 2 個目以降の“死骸”を回収
        RenderTargetHandles carry = m_sceneLayer.TakeCarryOverDead();
        if (!isEmptyRT(carry)) {
            deadScene = std::move(carry);
        }
    }

    // --- 3) Scene/Game のオフスクリーン描画 ---
    if (m_Camera) {
        SceneLayerBeginArgs args{};
        args.device = m_dev->GetDevice();
        args.cmd = cmd;
        args.frameIndex = fi;
        args.scene = m_CurrentScene.get();
        args.camera = m_Camera.get();
        m_sceneLayer.Record(args, /*maxObjects=*/MaxObjects);
    }

    // --- 4) Presenter.Begin：バックバッファを描ける状態へ ---
    ID3D12Resource* bb = m_dev->GetBackBuffer(fi);
    PresentTargets pt{};
    pt.rtv = m_dev->GetRTVHandle(fi);
    pt.backBuffer = bb;
    pt.width = m_dev->GetWidth();
    pt.height = m_dev->GetHeight();
    m_presenter.Begin(cmd, pt);

    // --- 5) ImGui：EditorContext 構築とウィンドウ構築 ---
    static bool s_resetLayout = false;
    static bool s_autoRelayout = false;

    EditorContext ctx{};
    ctx.pEnableEditor = &m_IsEditor;

    // Scene/Game の“実 RT サイズ”を SceneLayer から同期（ここで最新状態を反映）
    m_sceneLayer.SyncStatsTo(ctx);

    // ウィンドウ（スワップチェイン）のサイズは BackBuffer の記述子から取得
    {
        D3D12_RESOURCE_DESC bbDesc = bb->GetDesc();
        ctx.rtWidth = static_cast<std::uint32_t>(bbDesc.Width);
        ctx.rtHeight = static_cast<std::uint32_t>(bbDesc.Height);
    }

    // フレームの統計情報/フラグ
    ctx.fps = ImGui::GetIO().Framerate;
    ctx.pRequestResetLayout = &s_resetLayout;
    ctx.pAutoRelayout = &s_autoRelayout;

    // 念のためもう一度 Sync（rtWidth/rtHeight は上書きしない）
    m_sceneLayer.SyncStatsTo(ctx);

    // ImGui へ表示するための SRV (ImTextureID) を供給
    m_sceneLayer.FeedToUI(ctx, m_imgui.get(), fi, kSceneSrvBase, kGameSrvBase);

    // パネル描画関数を注入（UI レイヤは関数を呼ぶだけで中身は EditorPanels 側）
    ctx.DrawInspector = [&]() { EditorPanels::DrawInspector(m_Selected); };
    ctx.DrawHierarchy = [&]() { EditorPanels::DrawHierarchy(m_CurrentScene.get(), m_Selected); };

    // ImGui 1フレーム開始 → ウィンドウ/ドッキング構築
    m_imgui->NewFrame();
    m_imgui->BuildDockAndWindows(ctx);

    // UI 側が要求する Scene ビューポートサイズを取得（デバウンス付き確定へ渡す）
    const float dt = ImGui::GetIO().DeltaTime;
    const UINT wantW = (UINT)std::lroundf(ctx.sceneViewportSize.x);
    const UINT wantH = (UINT)std::lroundf(ctx.sceneViewportSize.y);
    if (wantW > 0 && wantH > 0) {
        m_sceneLayer.RequestResize(wantW, wantH, dt);
    }

    // ImGui のドローコマンドを現在のコマンドリストに記録
    m_imgui->Render(cmd);

    // --- 6) Presenter.End：バックバッファを PRESENT 状態へ ---
    m_presenter.End(cmd, bb);

    // --- 7) EndFrame：Present/Signal、ガベージ登録/回収 ---
    // 1 フレームにつき“最大 1 個”の RT を遅延破棄へ。2 個目以降は carry-over で次フレに回す。
    m_scheduler.EndFrame(&deadScene);
    ++m_frameCount;
}


//==============================================================================
// Cleanup
//==============================================================================

/*
    Cleanup
    ----------------------------------------------------------------------------
    - すべての GPU 仕事が完了するまで待機
    - ImGui/シーンのリソース解放
    - 遅延破棄キューの全回収
    - フェンスや DeviceResources の破棄
*/
void D3D12Renderer::Cleanup()
{
    if (!m_dev) return;

    // 全フレーム待機（FrameResources の fenceValue を見ながら）
    for (UINT i = 0; i < m_frames.GetCount(); ++i) {
        auto& fr = m_frames.Get(i);
        if (fr.fenceValue != 0 && m_fence->GetCompletedValue() < fr.fenceValue) {
            m_fence->SetEventOnCompletion(fr.fenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }

    // ImGui のシャットダウン
    if (m_imgui) { m_imgui->Shutdown(); m_imgui.reset(); }

    // シーン側の GPU リソース解放（メッシュ VB/IB 等）
    ReleaseSceneResources();

    // 念のため完全待機 → 遅延破棄をすべて Flush
    WaitForGPU();
    m_garbage.FlushAll();

    // フレームリソース破棄（Map 解除等）
    m_frames.Destroy();

    if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
    m_fence.Reset();

    m_dev.reset();
}

//==============================================================================
// Utilities
//==============================================================================

/*
    WaitForGPU
    ----------------------------------------------------------------------------
    現在キューに積まれている仕事が完了するまで待つ。
    “今この時点まで”の仕事の完了をフェンスで確認する用途。
*/
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

/*
    DrawMesh
    ----------------------------------------------------------------------------
    MeshRendererComponent に積んである VB/IB/IndexCount を使って即描画。
    事前に PSO、RootSig、CBV などは呼び出し側で設定済み前提。
*/
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

/*
    CreateMeshRendererResources
    ----------------------------------------------------------------------------
    MeshRendererComponent の MeshData（CPU 側）から、Upload ヒープに VB/IB を作成。
    小規模/開発ツール向け。パフォーマンスを求めるなら DEFAULT ヒープ＋コピーへ移行。
*/
bool D3D12Renderer::CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> mr)
{
    if (!mr) return false;
    const MeshData& md = mr->GetMeshData();
    if (md.Vertices.empty() || md.Indices.empty()) return false;

    ID3D12Device* dev = m_dev->GetDevice();
    HRESULT hr;
    D3D12_HEAP_PROPERTIES heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // VB 作成 → Map → memcpy → Unmap → VBV 設定
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

    // IB 作成 → Map → memcpy → Unmap → IBV 設定
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

/*
    ReleaseSceneResources
    ----------------------------------------------------------------------------
    現在のシーンが保持している MeshRendererComponent の GPU リソースを解放。
    （ガベージキューは使わず、素直に ComPtr を Reset）
*/
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

//==============================================================================
// Resize
//==============================================================================

/*
    Resize
    ----------------------------------------------------------------------------
    ウィンドウ側から通知されたスワップチェインのリサイズ処理。
      - すべてのフレームを待機してから ResizeBuffers（DeviceResources::Resize）
      - Scene の RT は UI ビューポート追従なのでここでは手を出さない
      - カメラのアスペクトのみ更新（従来仕様の互換）
*/
void D3D12Renderer::Resize(UINT width, UINT height) noexcept
{
    if (width == 0 || height == 0) return;

    // 全フレーム待機（SwapChain の再作成に備える）
    for (UINT i = 0; i < m_frames.GetCount(); ++i) {
        auto& fr = m_frames.Get(i);
        if (fr.fenceValue != 0 && m_fence->GetCompletedValue() < fr.fenceValue) {
            m_fence->SetEventOnCompletion(fr.fenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }

    // スワップチェインをリサイズ（RTV/DSV 再作成は DeviceResources 内で処理）
    m_dev->Resize(width, height);

    // SceneRT は UI ビューポートに合わせて別途 Viewports 側で再生成する方針
    // カメラのアスペクトだけは従来通りここで更新しておく
    if (m_Camera)
        m_Camera->SetAspect(static_cast<float>(width) / static_cast<float>(height));
}
