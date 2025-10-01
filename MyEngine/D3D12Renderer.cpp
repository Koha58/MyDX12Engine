//======================================================================================
// D3D12Renderer.cpp  ―  DX12 最小レンダラー（可読性・再学習しやすさ重視）
//
// 本ファイルは「いつ読み返しても迷わない」ことを目的に、処理順に沿って定義し、
// 各ステップの意図・前提・失敗時の挙動・性能上の注意を細かくコメントしています。
//
// 配置（読む順／処理順）
//   0) ヘッダ＆ユーティリティ（Includes & Helpers）
//   1) 公開API：Initialize → Render → Resize → Cleanup（Public API）
//   2) 作成系：CreateDevice / CreateCommandQueue / CreateSwapChain / CreateRenderTargetViews
//               CreateDepthStencilBuffer / CreateCommandAllocatorsAndList / CreatePipelineState
//               CreateMeshRendererResources（Creation）
//   3) 同期・ユーティリティ：WaitForPreviousFrame / WaitForGPU / DrawMesh / ReleaseSceneResources
//
// 設計メモ（再学習の取っ掛かり）：
//  - 定数バッファ(CB)は 256B アライン必須。起動時に MaxObjects 分の CBV を一括生成し、
//    描画時は GPU ディスクリプタを「オフセット指定で差し替え」るだけで高速に回す。
//  - Render() の CBV バインドは 1 回のみ（無駄な再設定を避ける）。
//  - 同期は「毎フレーム完全同期」（安全だがスループットは控えめ）。より高性能化するなら
//    フレームリソースを二重化/三重化して、書き込み先のラウンドロビン化を行う。
//  - HLSL は row_major を明示。C++ 側は「転置せず」float4x4 を詰める約束とする。
//  - SwapChain は Flip Discard / vsync=ON（Present(1,0)）。ティアリング対応は未実装。
//  - 例外方針：致命的同期エラー（Fence到達待ち失敗）などは例外を投げる。他は false 返却。
//
// 依存：D3D12Renderer.h / SceneConstantBuffer.h / MeshRendererComponent, GameObject など。
//       それらの宣言・ライフサイクルは本ファイル外で管理する前提。
//======================================================================================

#pragma region Includes & Helpers
#include "D3D12Renderer.h"
#include "SceneConstantBuffer.h"
#include "d3dx12.h"                 // CD3DX12_* ヘルパ（DX12公式サンプル系）

#include <Windows.h>                 // HWND / WaitForSingleObject / OutputDebugString
#include <wrl/client.h>              // ComPtr（RAIIでAddRef/Releaseを自動化）
#include <stdexcept>                 // std::runtime_error（致命エラー用）
#include <d3dcompiler.h>             // D3DCompile（ランタイムコンパイル。製品ではDXC推奨）
#include <d3d12sdklayers.h>          // DebugLayer / InfoQueue / DebugDevice（診断用）
#include <sstream>                   // ログ文整形
#include <comdef.h>                  // _com_error（HRESULT→文字列）
#include <functional>                // 再帰ラムダ
#include <cmath>                     // std::isfinite / std::fabs
#include <cstring>                   // std::strlen / std::memcpy
#include <dxgi1_6.h>                 // DXGI（SwapChain/Adapter列挙）

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;


// --- 失敗時ログ補助（成功時は何もしない）-----------------------------------------
// ・DirectXの関数は HRESULT で返ってくる。失敗時に OS 側の説明文字列を付けて出力する。
// ・例外は投げない（投げる箇所は別途明示）。復帰可能性がある部分は戻り値で伝える。
namespace
{
    void LogHRESULTError(HRESULT hr, const char* msg)
    {
        if (FAILED(hr))
        {
            _com_error err(hr);
            std::wstring errMsg = err.ErrorMessage();
            std::wstringstream wss;
            wss << L"[D3D12Renderer ERROR] " << msg
                << L" HRESULT=0x" << std::hex << hr
                << L" : " << errMsg << L"\n";
            OutputDebugStringW(wss.str().c_str());
        }
    }

    // 一時バッファ(毎フレーム再利用)
    static std::string g_tmpNameBuf;
}
#pragma endregion // Includes & Helpers

const char* D3D12Renderer::GONameUTF8(const GameObject* go)
{
    return go ? go->Name.c_str() : "(null)";
}

// 一度に描ける最大数（CBVスロットと一致させる。将来は可変長へ拡張可）
constexpr UINT MaxObjects = 100;

//======================================================================================
// コンストラクタ / デストラクタ
//   ・ここでは重い初期化はしない（Create* は Initialize() に集約）。
//   ・デストラクタでは Cleanup() を呼ぶことで、GPU完了待ち→解放の順序を保証。
//======================================================================================
D3D12Renderer::D3D12Renderer()
    : m_Width(0), m_Height(0),
    frameIndex(0),
    fenceEvent(nullptr),
    m_frameCount(0),
    rtvDescriptorSize(0),
    m_nextFenceValue(1),
    m_ViewMatrix(DirectX::XMMatrixIdentity()),
    m_ProjectionMatrix(DirectX::XMMatrixIdentity())
{
    m_IsEditor = true;  // 起動時はエディタONに

    // フレーム別の初期化
    for (UINT f = 0; f < FrameCount; ++f)
    {
        m_frames[f].cbCPU = nullptr;
        m_frames[f].fenceValue = 0;
    }
}

D3D12Renderer::~D3D12Renderer()
{
    // Render中に解放してクラッシュしないよう、必ずGPU完了待ちを含む Cleanup を通す
    Cleanup();
}

#pragma region Public API (Initialize / Render / Resize / Cleanup)

/**
 * Initialize
 * 主要オブジェクトを依存順に生成し、共通CB/CBVを事前生成する。
 * 戻り値: 成功 true / 失敗 false（詳細はデバッグ出力を参照）
 * 注意点:
 *  - 再初期化は未考慮（既存リソースがある状態では呼ばない）
 *  - 他スレッドから同時に Render/Resize/Cleanup を呼ばないこと
 */
bool D3D12Renderer::Initialize(HWND hwnd, UINT width, UINT height)
{
    m_Width = width;   // ビューポートやスワップチェインに使用
    m_Height = height;

    HRESULT hr = S_OK;

    // --- 依存順に作成（失敗で即座に false）。例外は使わず呼び出し側で分岐しやすくする。
    if (!CreateDevice())                          return false; // 1) 物理/論理デバイス
    if (!CreateCommandQueue())                    return false; // 2) GPUへコマンド送出
    if (!CreateSwapChain(hwnd, width, height))    return false; // 3) 表示バックバッファ
    if (!CreateRenderTargetViews())               return false; // 4) 各バックバッファのRTV
    if (!CreateDepthStencilBuffer(width, height)) return false; // 5) 深度テクスチャ + DSV
    if (!CreateCommandAllocatorsAndList())        return false; // 6) フレーム別アロケータ + CL
    if (!CreatePipelineState())                   return false; // 7) ルートシグネチャ + PSO

    // ---- 8) 定数バッファ（Upload）と CBV を MaxObjects 分まとめて用意 -----------------
    m_cbStride = (sizeof(SceneConstantBuffer) + 255) & ~255u;   // 256Bアライン

    for (UINT f = 0; f < FrameCount; ++f)
    {
        if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommandAllocator"); return false; }

        // CBは64bitサイズで作成
        const UINT64 cbBytes = (UINT64)m_cbStride * (UINT64)MaxObjects;
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC   resDesc = CD3DX12_RESOURCE_DESC::Buffer(cbBytes);

        hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_frames[f].constantBuffer)
        );

        if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommittedResource(CB Upload)"); return false; }

        // 永続マップ (CPU Readしないので readRange=(0,0))
        CD3DX12_RANGE rr(0, 0);
        hr = m_frames[f].constantBuffer->Map(0, &rr, reinterpret_cast<void**>(&m_frames[f].cbCPU));
        if (FAILED(hr)) { LogHRESULTError(hr, "CB Map"); return false; }

        m_frames[f].fenceValue = 0;
    }

    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateFence"); return false; }
    m_nextFenceValue = 1;

    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr); // auto-reset
    if (!fenceEvent) { OutputDebugStringA("[D3D12Renderer ERROR] CreateEvent failed\n"); return false; }

    // ===== ImGui 初期化（新API簡易パス）=====
    {
        // 1) SRVヒープ（Shader Visible）
        D3D12_DESCRIPTOR_HEAP_DESC h{};
        h.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        h.NumDescriptors = 1;                         // フォント用。必要なら増やす
        h.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(device->CreateDescriptorHeap(&h, IID_PPV_ARGS(&m_imguiSrvHeap))))
        {
            LogHRESULTError(E_FAIL, "CreateDescriptorHeap(ImGui SRV)"); return false;
        }

        // 2) Core & Platform
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();
        ImGui_ImplWin32_Init(hwnd);


        // 3) Renderer(DX12) : 必須情報だけ渡す
        ImGui_ImplDX12_InitInfo ii{};
        ii.Device = device.Get();
        ii.CommandQueue = commandQueue.Get();         // 自前のキューを使用
        ii.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        ii.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        ii.NumFramesInFlight = FrameCount;
        ii.SrvDescriptorHeap = m_imguiSrvHeap.Get();

        // 単一ディスクリプタ（レガシー互換モード）
        ii.LegacySingleSrvCpuDescriptor = m_imguiSrvHeap->GetCPUDescriptorHandleForHeapStart();
        ii.LegacySingleSrvGpuDescriptor = m_imguiSrvHeap->GetGPUDescriptorHandleForHeapStart();
        // ※ SrvDescriptorAllocFn/FreeFn は nullptr のままでOK（バックエンドが補う）

        ImGui_ImplDX12_Init(&ii);

        // 4) フォント登録（GPUアップロードは描画時に自動）
        ImGui::GetIO().Fonts->AddFontDefault();

        m_imguiInited = true;
    }


    return true;
}

/**
 * Render
 * 1フレーム描画：PRESENT→RTV遷移→クリア→シーン描画→PRESENT遷移→Present→完全同期。
 * 重要ポイント：
 *  - SetDescriptorHeaps はルートテーブル使用前に必ず呼ぶ。
 *  - バリアの対は「Present→RenderTarget」「RenderTarget→Present」できっちり戻す。
 *  - 本実装では最後に WaitForPreviousFrame() を呼び、CPU/GPU 完全同期で戻る。
 */
void D3D12Renderer::Render()
{
    const UINT fi = frameIndex;

    // 未完了ならこのフレームの完了を待つ
    const UINT64 fv = m_frames[fi].fenceValue;

    if (fv != 0 && fence->GetCompletedValue() < fv)
    {
        fence->SetEventOnCompletion(fv, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    // Reset
    HRESULT hr = m_frames[fi].cmdAlloc->Reset();
    if (FAILED(hr)) { LogHRESULTError(hr, "cmdAlloc->Reset"); return; }
    hr = commandList->Reset(m_frames[fi].cmdAlloc.Get(), pipelineState.Get());
    if (FAILED(hr)) { LogHRESULTError(hr, "commandList->Reset"); return; }

    // 現在のバックバッファに対応する RTV/DSV ハンドルを取得
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        rtvHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());

    // --- 2) PRESENT → RENDER_TARGET へリソース状態遷移 ------------------------------
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            renderTargets[frameIndex].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList->ResourceBarrier(1, &barrier);
    }

    // --- 3) クリア（深度→カラー） --------------------------------------------------
    // 深度を先にクリア→カラーをクリア。順序自体は厳密ではないが慣習として読みやすい。
    const float clearColor[] = { 0.2f, 0.2f, 0.4f, 1.0f }; // 背景色
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // --- 4) 固定状態（ルート/ヒープ/ビューポート/シザー/トポロジ）設定 --------------
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    const UINT f = frameIndex;

    D3D12_VIEWPORT vp{ 0.f, 0.f, (float)m_Width, (float)m_Height, 0.f, 1.f };
    D3D12_RECT     sc{ 0, 0, (LONG)m_Width, (LONG)m_Height };
    commandList->RSSetViewports(1, &vp);
    commandList->RSSetScissorRects(1, &sc);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // --- 5) シーン描画（GameObject ツリーを深さ優先で走査） -------------------------
    if (m_CurrentScene && m_Camera)
    {
        UINT slot = 0; // CBV スロット割り当て（0..MaxObjects-1）。超過は描画スキップ。

        using namespace DirectX;
        const XMMATRIX viewMatrix = m_Camera->GetViewMatrix();
        const XMMATRIX projMatrix = m_Camera->GetProjectionMatrix();

        std::function<void(std::shared_ptr<GameObject>)> draw =
            [&](std::shared_ptr<GameObject> go)
            {
                if (!go || slot >= MaxObjects) return;
                auto mr = go->GetComponent<MeshRendererComponent>();

                // 必要リソースが揃っているメッシュのみ描画（頂点/インデックス/カウント）
                if (mr && mr->VertexBuffer && mr->IndexBuffer && mr->IndexCount > 0)
                {
                    // 5-1) 行列の構築（row_major：右から掛ける）
                    XMMATRIX world = go->Transform->GetWorldMatrix();
                    XMMATRIX mvp = world * viewMatrix * projMatrix;

                    // 5-2) 法線行列 = transpose(inverse(world))。特異な場合は単位行列へフォールバック。
                    XMVECTOR det;
                    XMMATRIX inv = XMMatrixInverse(&det, world);
                    float detScalar = XMVectorGetX(det);
                    if (!std::isfinite(detScalar) || std::fabs(detScalar) < 1e-8f) inv = XMMatrixIdentity();
                    XMMATRIX worldIT = XMMatrixTranspose(inv);

                    // 5-3) 定数バッファ更新（永続マップ領域へ memcpy。行列の転置は不要）
                    SceneConstantBuffer cb{};
                    XMStoreFloat4x4(&cb.mvp, mvp);
                    XMStoreFloat4x4(&cb.world, world);
                    XMStoreFloat4x4(&cb.worldIT, worldIT);
                    XMStoreFloat3(&cb.lightDir, XMVector3Normalize(XMVectorSet(0.0f, -1.0f, -1.0f, 0.0f)));
                    cb.pad = 0.0f; // 16B アライン穴埋め

                    const UINT fi = frameIndex;
                    UINT8* cbBaseCPU = m_frames[fi].cbCPU;
                    D3D12_GPU_VIRTUAL_ADDRESS cbBaseGPU = m_frames[fi].constantBuffer->GetGPUVirtualAddress();

                    if (slot >= MaxObjects) return;

                    std::memcpy(cbBaseCPU + (UINT64)slot * m_cbStride, &cb, sizeof(cb));

                    // 5-4) ルートに“GPU仮想アドレス”を直指定
                    commandList->SetGraphicsRootConstantBufferView(
                    0, cbBaseGPU + (UINT64)slot * m_cbStride);

                    // 5-5) VB/IB 設定 → ドロー呼び出し
                    commandList->IASetVertexBuffers(0, 1, &mr->VertexBufferView);
                    commandList->IASetIndexBuffer(&mr->IndexBufferView);
                    commandList->DrawIndexedInstanced(mr->IndexCount, 1, 0, 0, 0);

                    ++slot; // 次オブジェクトへ
                }

                // 子ノードを再帰（深さ優先。順序要件があればここで制御）
                for (auto& ch : go->GetChildren()) draw(ch);
            };

        for (auto& root : m_CurrentScene->GetRootGameObjects()) draw(root);
    }
    else {
        // シーン未設定は Silent fail より警告ログで気づけるようにする
        OutputDebugStringA("[D3D12Renderer WARNING] Render: No scene set to render.\n");
    }

    // --- ImGui: UI 描画 --------------------------------------------------------
    if (m_imguiInited)
    {
        ID3D12DescriptorHeap* heaps[] = { m_imguiSrvHeap.Get() };
        commandList->SetDescriptorHeaps(1, heaps);

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // メインビューポート全面にDockSpaceを敷く
        {
            const ImGuiViewport* vp = ImGui::GetMainViewport();
            ImGui::DockSpaceOverViewport(
                (ImGuiID)0,
                (const ImGuiViewport*)vp, 
                (ImGuiDockNodeFlags)ImGuiDockNodeFlags_PassthruCentralNode, 
                (const ImGuiWindowClass*)nullptr
            );
        }

        ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Size: %u x %u", m_Width, m_Height);
        ImGui::Text("ImGui %s", ImGui::GetVersion());
        ImGui::End();

        // --- メインメニューバー(Editor ON/OFF) -------------
        static bool g_ResetEditorLayout = false;
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Editor"))
            {
                ImGui::MenuItem("Enable Editor", nullptr, &m_IsEditor);

                if (ImGui::MenuItem("Reset Editor Layout"))
                {
                    g_ResetEditorLayout = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (g_ResetEditorLayout)
        {
            ImGui::LoadIniSettingsFromMemory("");
            g_ResetEditorLayout = false;
        }

        // --- Editorウィンドウ -------------
        if (m_IsEditor)
        {
            // Inspector
            ImGui::SetNextWindowPos(ImVec2(10, 40), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(320, 320), ImGuiCond_Always);
            ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse);

            if (auto sel = m_Selected.lock())
            {
                ImGui::Text("Selected: %s", GONameUTF8(sel.get()));
                ImGui::Separator();
                ImGui::TextDisabled("Transform will appear here...");
            }
            else
            {
                ImGui::TextDisabled("No selection");
            }

            ImGui::End();

            // Hierarchy
            ImGui::SetNextWindowPos(ImVec2(10, 380), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(320, 300), ImGuiCond_Always);
            ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoCollapse);

            if (m_CurrentScene)
            {
                for (auto& root : m_CurrentScene->GetRootGameObjects())
                {
                    DrawHierarchyNode(root);
                }
            }
            else
            {
                ImGui::TextDisabled("No scene");
            }
            ImGui::End();
        }

        ImGui::Render();
        commandList->SetDescriptorHeaps(1, heaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
    }

    // --- 6) RENDER_TARGET → PRESENT へ遷移（戻し忘れると Present 失敗） -------------
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            renderTargets[frameIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        commandList->ResourceBarrier(1, &barrier);
    }

    // --- 7) コマンドを閉じて実行 → Present -----------------------------------------
    hr = commandList->Close();
    if (FAILED(hr)) { LogHRESULTError(hr, "Close"); return; }

    ID3D12CommandList* lists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(lists), lists);

    // vsync ON（SyncInterval=1）。ティアリング対応する場合は AllowTearing が必要。
    hr = swapChain->Present(1, 0);
    if (FAILED(hr)) { LogHRESULTError(hr, "Present"); return; }

    const UINT64 signal = m_nextFenceValue++;
    hr = commandQueue->Signal(fence.Get(), signal);
    if (FAILED(hr)) { LogHRESULTError(hr, "Signal"); return; }
    m_frames[fi].fenceValue = signal;

    // --- 8) 完全同期（安全最優先。高スループット化は将来対応） -----------------------
    frameIndex = swapChain->GetCurrentBackBufferIndex();
    ++m_frameCount;
}

/**
 * Resize
 * スワップチェインのバッファを作り直し、RTV/DSV を再生成する。
 * 注意点：
 *  - 最小化（width/height=0）は何もしない。
 *  - 使用中のリソース解放はクラッシュの温床 → 先に WaitForGPU() で待ってから解放。
 */
void D3D12Renderer::Resize(UINT width, UINT height) noexcept
{
    if (width == 0 || height == 0) return; // 最小化など

    m_Width = width;
    m_Height = height;

    // 1) すべてのフレームが完了するまで待つ
    for (UINT f = 0; f < FrameCount; ++f)
    {
        const UINT64 v = m_frames[f].fenceValue;
        if (v != 0 && fence->GetCompletedValue() < v)
        {
            fence->SetEventOnCompletion(v, fenceEvent);
            WaitForSingleObject(fenceEvent, INFINITE);
        }
    }

    // 2) 旧サイズ依存リソースを開放（ComPtr::Reset で Release）
    for (UINT i = 0; i < FrameCount; ++i) renderTargets[i].Reset();
    depthStencilBuffer.Reset();
    dsvHeap.Reset();

    // 3) スワップチェインのサイズのみ変更（フォーマット/フラグは据え置き）
    DXGI_SWAP_CHAIN_DESC1 desc1{};
    swapChain->GetDesc1(&desc1);
    HRESULT hr = swapChain->ResizeBuffers(FrameCount, width, height, desc1.Format, desc1.Flags);
    if (FAILED(hr)) { LogHRESULTError(hr, "swapChain->ResizeBuffers"); return; }

    frameIndex = swapChain->GetCurrentBackBufferIndex();

    // 4) RTV 再作成（RTVヒープは再利用）
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(rtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT i = 0; i < FrameCount; ++i) 
        {
            hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
            if (FAILED(hr)) { LogHRESULTError(hr, "swapChain->GetBuffer (Resize)"); return; }
            device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtv);
            rtv.Offset(1, rtvDescriptorSize);
        }
    }

    // 5) 新しいサイズで DSV を作り直す
    if (!CreateDepthStencilBuffer(width, height)) {
        OutputDebugStringA("[Resize] CreateDepthStencilBuffer failed\n");
        return;
    }

    // 6)各フレームのアロケーターをリセット
    for (UINT f = 0; f < FrameCount; ++f)
    {
        if (m_frames[f].cmdAlloc)
        {
            m_frames[f].cmdAlloc->Reset();
        }
    }
}

/**
 * Cleanup
 * GPU 完了待ち → シーンGPUリソース破棄 → コマンド/ヒープ/CB/同期/スワップチェイン/デバイスの順に解放。
 * いつ呼ばれても安全（nullptr/未初期化を許容）。二重呼び出しも無害。
 */
void D3D12Renderer::Cleanup()
{
    if (!device) return; // 既に解放済み

    // 0) すべてのフレームが完了するまで待つ
    for (UINT f = 0; f < FrameCount; ++f)
    {
        const UINT64 v = m_frames[f].fenceValue;
        if (v != 0 && fence->GetCompletedValue() < v)
        {
            fence->SetEventOnCompletion(v, fenceEvent);
            WaitForSingleObject(fenceEvent, INFINITE);
        }
    }

    if (m_imguiInited)
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        m_imguiSrvHeap.Reset();
        m_imguiInited = false;
    }

    // 1) シーン配下の GPU リソース（VB/IB等）を全解放
    ReleaseSceneResources();

    // 2) コマンド/PSO/ルート
    commandList.Reset();
    for (UINT i = 0; i < FrameCount; ++i)
    {
        m_frames[i].cmdAlloc.Reset();
    }
    pipelineState.Reset();
    rootSignature.Reset();

    // 3) フレームリソース/ヒープ
    for (UINT i = 0; i < FrameCount; ++i) renderTargets[i].Reset();
    rtvHeap.Reset();
    depthStencilBuffer.Reset();
    dsvHeap.Reset();

    // 4) CB のマップ解除→解放（永続マップは Unmap を忘れない）
    for (UINT f = 0; f < FrameCount; ++f)
    {
        if (m_frames[f].constantBuffer && m_frames[f].cbCPU)
        {
            m_frames[f].constantBuffer->Unmap(0, nullptr);
            m_frames[f].cbCPU = nullptr;
        }
        m_frames[f].constantBuffer.Reset();
        m_frames[f].cmdAlloc.Reset();
        m_frames[f].fenceValue = 0;
    }

    // 5) 同期/スワップチェイン/キュー
    if (fenceEvent) { CloseHandle(fenceEvent); fenceEvent = nullptr; }
    fence.Reset();
    swapChain.Reset();
    commandQueue.Reset();

#ifdef _DEBUG
    {
        // Live Object 診断：
        // ・WARNING ブレークは抑制（LiveDevice 警告で止まらないように）
        // ・終了直前に残オブジェクトをダンプ（原因追跡の手掛かり）
        ComPtr<ID3D12InfoQueue> iq;
        if (device && SUCCEEDED(device.As(&iq))) {
            iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);
        }
        ComPtr<ID3D12DebugDevice> dbg;
        if (device && SUCCEEDED(device.As(&dbg))) {
            dbg->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
        }
    }
#endif

    // 6) 最後にデバイスを解放
    device.Reset();
}
#pragma endregion // Public API

#pragma region Creation (Device / Queue / SwapChain / RTV / DSV / Allocators / PSO / Mesh)

/**
 * CreateDevice
 * DXGI でハードウェアアダプタを列挙し、D3D12 デバイスを作成する。
 * 方針：
 *  - ソフトウェアアダプタ（WARP）はスキップ。WARP フォールバックは本実装では未対応。
 *  - FeatureLevel 11.0 で作成（必要に応じて引き上げ可能）。
 *  - _DEBUG 時は DebugLayer を有効化し、InfoQueue でノイズを軽減。
 */
bool D3D12Renderer::CreateDevice()
{
    HRESULT hr;
    ComPtr<IDXGIFactory4> factory;
    UINT flags = 0;

#ifdef _DEBUG
    // DebugLayer を有効化（API 前後条件チェック）。性能は若干下がるが開発時は有用。
    if (ComPtr<ID3D12Debug> dbg; SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg))))
    {
        dbg->EnableDebugLayer();
        flags = DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateDXGIFactory2"); return false; }

    // ハードウェアアダプタを列挙し、最初にデバイス作成に成功したものを採用
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; ; ++i)
    {
        if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) break;
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue; // WARP は除外
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) break;
    }
    if (!device) {
        OutputDebugStringA("[D3D12Renderer ERROR] Failed to create D3D12 device\n");
        return false;
    }

#ifdef _DEBUG
    // InfoQueue：重大度ごとにブレーク、不要なメッセージを抑制
    if (ComPtr<ID3D12InfoQueue> iq; SUCCEEDED(device.As(&iq))) {
        iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        D3D12_MESSAGE_ID deny[] = {
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,   // Map/Unmap の NULL 範囲ノイズ
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
        };
        D3D12_INFO_QUEUE_FILTER f{};
        f.DenyList.NumIDs = _countof(deny);
        f.DenyList.pIDList = deny;
        iq->PushStorageFilter(&f);
    }
#endif
    return true;
}

/** CommandQueue（Directタイプ：Graphics/Compute/Copy 全対応） */
bool D3D12Renderer::CreateCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC q{};
    q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    q.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // タイムスタンプ等が不要ならNONEで十分
    HRESULT hr = device->CreateCommandQueue(&q, IID_PPV_ARGS(&commandQueue));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommandQueue"); return false; }
    return true;
}

/**
 * SwapChain（Flip Discard）
 *  - SyncInterval=1（vsync ON）。ティアリング対応は未実装（AllowTearingが必要）。
 *  - Alt+Enter の自動フルスクリーン切替は無効化（アプリが制御）。
 */
bool D3D12Renderer::CreateSwapChain(HWND hwnd, UINT width, UINT height)
{
    ComPtr<IDXGIFactory4> factory;
    UINT flags = 0;
#ifdef _DEBUG
    flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    HRESULT hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateDXGIFactory2 (SwapChain)"); return false; }

    DXGI_SWAP_CHAIN_DESC1 sc{};
    sc.BufferCount = FrameCount;                        // バックバッファ枚数はヘッダ側定義に一致
    sc.Width = width;
    sc.Height = height;
    sc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;        // sRGB が必要なら *_UNORM_SRGB
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;     // 近年の推奨
    sc.SampleDesc = { 1, 0 };                            // MSAA 無し

    ComPtr<IDXGISwapChain1> sc1;
    hr = factory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &sc, nullptr, nullptr, &sc1);
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateSwapChainForHwnd"); return false; }

    hr = sc1.As(&swapChain);                             // IDXGISwapChain4 へ昇格
    if (FAILED(hr)) { LogHRESULTError(hr, "IDXGISwapChain1::As"); return false; }

    // Alt+Enter を禁止（フルスクリーン切替はアプリ側の明示的操作に統一）
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    frameIndex = swapChain->GetCurrentBackBufferIndex(); // 0..FrameCount-1
    return true;
}

/** RTV（各バックバッファへ RenderTargetView を並べる） */
bool D3D12Renderer::CreateRenderTargetViews()
{
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.NumDescriptors = FrameCount;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvHeap));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateDescriptorHeap(RTV)"); return false; }

    // ヒープ内インクリメント幅（この値だけ Offset して次のハンドルへ進む）
    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE h(rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < FrameCount; ++i)
    {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
        if (FAILED(hr)) { LogHRESULTError(hr, "swapChain->GetBuffer"); return false; }
        device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, h);
        h.Offset(1, rtvDescriptorSize);
    }
    return true;
}

/**
 * DSV + Depth Buffer
 *  - Defaultヒープに深度テクスチャを確保し、DSV を 1 枚作成。
 *  - ClearValue はローカルな"ファストクリア"のためのヒント（必須ではない）。
 */
bool D3D12Renderer::CreateDepthStencilBuffer(UINT width, UINT height)
{
    HRESULT hr;

    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc{};
    dsvDesc.NumDescriptors = 1;
    dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    if (FAILED(device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&dsvHeap))))
    {
        LogHRESULTError(E_FAIL, "CreateDescriptorHeap(DSV)"); return false;
    }

    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC    texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D32_FLOAT, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    D3D12_CLEAR_VALUE clear{};
    clear.Format = DXGI_FORMAT_D32_FLOAT;
    clear.DepthStencil.Depth = 1.0f;
    clear.DepthStencil.Stencil = 0;

    hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear, IID_PPV_ARGS(&depthStencilBuffer));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommittedResource(Depth)"); return false; }

    D3D12_DEPTH_STENCIL_VIEW_DESC v{};
    v.Format = DXGI_FORMAT_D32_FLOAT;
    v.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(depthStencilBuffer.Get(), &v, dsvHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
}

/**
 * Command Allocators & Command List
 *  - アロケータは GPU 完了まで Reset 不可 → フレーム数分用意するのが定石。
 *  - コマンドリストは 1 本を Close→Reset で使い回す。
 */
bool D3D12Renderer::CreateCommandAllocatorsAndList()
{
    HRESULT hr = S_OK;

    // 1) 各フレームのCommandAllocatorを作成
    for (UINT f = 0; f < FrameCount; ++f)
    {
        hr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_frames[f].cmdAlloc)
        );

        if (FAILED(hr))
        {
            LogHRESULTError(hr, "CreateCommandAllocator");
            return false;
        }
    }

    // 2) コマンドリストをフレーム０のアロケーターで作成(初期状態はOpen)
    hr = device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT, 
        m_frames[0].cmdAlloc.Get(),
        nullptr,
        IID_PPV_ARGS(&commandList));

    if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommandList"); return false; }

    // 3) 作成直後は Open 状態なので一度 Close。以後は Reset→記録→Close のサイクルで運用。
    commandList->Close();
    return true;
}

/**
 * RootSig + PSO（Lambert）
 *  - ルートは CBV(b0) の 1 テーブルのみという最小構成。
 *  - HLSL は row_major。法線は inverse-transpose(world) で変換。
 *  - 製品では DXC(DXIL) やオフラインコンパイル＋キャッシュを推奨。
 */
bool D3D12Renderer::CreatePipelineState()
{
    // ルートシグネチャ（CBV テーブル b0）
    CD3DX12_ROOT_PARAMETER root{};
    root.InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

    D3D12_ROOT_SIGNATURE_DESC rs{};
    rs.NumParameters = 1;
    rs.pParameters = &root;
    rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sig, err;
    HRESULT hr = D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
    if (FAILED(hr)) { if (err) OutputDebugStringA((char*)err->GetBufferPointer()); LogHRESULTError(hr, "D3D12SerializeRootSignature"); return false; }

    hr = device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateRootSignature"); return false; }

    // HLSL（可読性のためソース直埋め。実際は .hlsl を別途ビルド/読み込みが望ましい）
    const char* vsSource = R"(
cbuffer cb0 : register(b0)
{
    row_major float4x4 g_mvp;
    row_major float4x4 g_world;
    row_major float4x4 g_worldIT;
    float3 g_lightDir; float pad;
};
struct VSInput { float3 pos:POSITION; float3 normal:NORMAL; float4 color:COLOR; };
struct PSInput { float4 pos:SV_POSITION; float3 normal:NORMAL; float4 color:COLOR; };
PSInput main(VSInput i){
    PSInput o;
    o.pos    = mul(float4(i.pos,1), g_mvp);
    o.normal = normalize(mul(i.normal, (float3x3)g_worldIT));
    o.color  = i.color;
    return o;
})";

    const char* psSource = R"(
cbuffer cb0 : register(b0)
{
    row_major float4x4 g_mvp;
    row_major float4x4 g_world;
    row_major float4x4 g_worldIT;
    float3 g_lightDir; float pad;
};
struct PSInput { float4 pos:SV_POSITION; float3 normal:NORMAL; float4 color:COLOR; };
float4 main(PSInput i) : SV_TARGET {
    float NdotL = max(dot(normalize(i.normal), -normalize(g_lightDir)), 0.0f);
    return float4(i.color.rgb * NdotL, i.color.a);
})";

    ComPtr<ID3DBlob> VS, PS, ce;
    hr = D3DCompile(vsSource, std::strlen(vsSource), nullptr, nullptr, nullptr, "main", "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &VS, &ce);
    if (FAILED(hr)) { if (ce) OutputDebugStringA((char*)ce->GetBufferPointer()); LogHRESULTError(hr, "VS compile"); return false; }
    hr = D3DCompile(psSource, std::strlen(psSource), nullptr, nullptr, nullptr, "main", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &PS, &ce);
    if (FAILED(hr)) { if (ce) OutputDebugStringA((char*)ce->GetBufferPointer()); LogHRESULTError(hr, "PS compile"); return false; }

    // 頂点レイアウト（P/N/C）：P(12) N(12) C(16) = 40B/頂点
    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // PSO：基本は D3D12_DEFAULT をそのまま採用。必要最低限の深度設定のみ明示。
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.InputLayout = { layout, _countof(layout) };
    pso.pRootSignature = rootSignature.Get();
    pso.VS = CD3DX12_SHADER_BYTECODE(VS.Get());
    pso.PS = CD3DX12_SHADER_BYTECODE(PS.Get());
    pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pso.DepthStencilState.DepthEnable = TRUE;                    // 深度テスト有効
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // 書き込み有効
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS; // 小さいZが手前
    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;          // sRGBなら *_UNORM_SRGB
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc.Count = 1;                                   // MSAA 無し

    hr = device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateGraphicsPipelineState"); return false; }
    return true;
}

/**
 * Mesh（Upload ヒープ版）
 *  - 頂点/インデックスを Upload ヒープに確保し、Map→memcpy→Unmap で転送。
 *  - 静的/大規模データは Default ヒープ + Copy キューに乗せる方が高速（将来拡張）。
 */
bool D3D12Renderer::CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> mr)
{
    if (!mr || mr->m_MeshData.Indices.empty()) {
        OutputDebugStringA("[D3D12Renderer WARNING] CreateMeshRendererResources: invalid/empty mesh\n");
        return false;
    }

    HRESULT hr;
    D3D12_HEAP_PROPERTIES heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // --- 頂点バッファ --------------------------------------------------------------
    const UINT vbSize = (UINT)mr->m_MeshData.Vertices.size() * sizeof(Vertex);
    D3D12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);
    hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &vbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mr->VertexBuffer));
    if (FAILED(hr)) { LogHRESULTError(hr, "Create VB"); return false; }

    {
        UINT8* dst = nullptr; CD3DX12_RANGE rr(0, 0);
        hr = mr->VertexBuffer->Map(0, &rr, reinterpret_cast<void**>(&dst));
        if (FAILED(hr)) { LogHRESULTError(hr, "VB Map"); return false; }
        std::memcpy(dst, mr->m_MeshData.Vertices.data(), vbSize);
        mr->VertexBuffer->Unmap(0, nullptr);
    }
    mr->VertexBufferView.BufferLocation = mr->VertexBuffer->GetGPUVirtualAddress();
    mr->VertexBufferView.StrideInBytes = sizeof(Vertex);
    mr->VertexBufferView.SizeInBytes = vbSize;

    // --- インデックスバッファ ------------------------------------------------------
    const UINT ibSize = (UINT)mr->m_MeshData.Indices.size() * sizeof(unsigned int);
    D3D12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);
    hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &ibDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mr->IndexBuffer));
    if (FAILED(hr)) { LogHRESULTError(hr, "Create IB"); return false; }

    {
        UINT8* dst = nullptr; CD3DX12_RANGE rr(0, 0);
        hr = mr->IndexBuffer->Map(0, &rr, reinterpret_cast<void**>(&dst));
        if (FAILED(hr)) { LogHRESULTError(hr, "IB Map"); return false; }
        std::memcpy(dst, mr->m_MeshData.Indices.data(), ibSize);
        mr->IndexBuffer->Unmap(0, nullptr);
    }
    mr->IndexBufferView.BufferLocation = mr->IndexBuffer->GetGPUVirtualAddress();
    mr->IndexBufferView.Format = DXGI_FORMAT_R32_UINT; // 32bit index（16bitなら R16_UINT）
    mr->IndexBufferView.SizeInBytes = ibSize;

    mr->IndexCount = (UINT)mr->m_MeshData.Indices.size();
    return true;
}
#pragma endregion // Creation

#pragma region Sync & Utilities (Wait / Draw / Release)

/**
 * WaitForGPU
 * 例外を投げない簡易待機。Resize 等の「とりあえず待つ」用途に。
 * 失敗しても戻り値無し（ベストエフォート）。
 */
void D3D12Renderer::WaitForGPU() noexcept
{
    if (!commandQueue || !fence) return;
    const UINT64 v = ++m_nextFenceValue;
    if (FAILED(commandQueue->Signal(fence.Get(), v))) return;

    if (fence->GetCompletedValue() < v) 
    {
        if (SUCCEEDED(fence->SetEventOnCompletion(v, fenceEvent))) 
        {
            WaitForSingleObject(fenceEvent, INFINITE);
        }
    }
}

/**
 * DrawMesh
 * 前提：
 *  - 既に PSO/ルート/CBV/RT/ビューポート/シザーなどが設定済みであること。
 *  - commandList は記録中（Reset〜Closeの間）。
 */
void D3D12Renderer::DrawMesh(MeshRendererComponent* mr)
{
    if (!mr) return;
    commandList->IASetVertexBuffers(0, 1, &mr->VertexBufferView);
    commandList->IASetIndexBuffer(&mr->IndexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawIndexedInstanced(mr->IndexCount, 1, 0, 0, 0);
}

void D3D12Renderer::DrawHierarchyNode(const std::shared_ptr<GameObject>& go)
{
    if (!go) return;

    // 同名でも衝突しないようIDを積む
    ImGui::PushID(go.get());

    // 選択状態
    bool isSelected = (!m_Selected.expired() && m_Selected.lock().get() == go.get());

    // 子を持つならツリーノード、持たないならリーフ
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                             | ImGuiTreeNodeFlags_SpanFullWidth
                             | (isSelected ? ImGuiTreeNodeFlags_Selected : 0);

    if (go->GetChildren().empty())
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    bool open = ImGui::TreeNodeEx(GONameUTF8(go.get()), flags);

    // クリックで選択
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        m_Selected = go;
    }

    // 再帰で子を描画
    if (open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
    {
        for (auto& ch : go->GetChildren())
        {
            DrawHierarchyNode(ch);
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
}

/**
 * ReleaseSceneResources
 * シーン配下の GPU リソース(VB/IB/SRV/テクスチャ等)を安全に解放。
 * 注意：呼び出し前に GPU 完了待ち済みであること（Resize/Cleanup 内から呼ぶ）。
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
                    // SRV/テクスチャ等を持たせた場合はここで Reset を追加
                }
                for (auto& ch : go->GetChildren()) walk(ch);
            };
        walk(root);
    }

    // 参照を切って上位のライフサイクルへ返す
    m_Camera.reset();
    m_CurrentScene.reset();
}
#pragma endregion // Sync & Utilities
