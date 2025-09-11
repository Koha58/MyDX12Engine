//======================================================================================
// D3D12Renderer.cpp  ―  DX12 最小レンダラー（可読性・再学習しやすさ重視）
//
// 物理配置（読みやすさ目的の順番）
//   0) ヘッダ／ユーティリティ
//   1) 外から呼ぶ順：Initialize → Render → Resize → Cleanup
//   2) 作成系：CreateDevice / CreateCommandQueue / CreateSwapChain / CreateRenderTargetViews
//              CreateDepthStencilBuffer / CreateCommandAllocatorsAndList / CreatePipelineState
//              CreateMeshRendererResources
//   3) 同期・ユーティリティ：WaitForPreviousFrame / WaitForGPU / DrawMesh
//
// 設計メモ：
//  - CB(定数バッファ)は 256B アライン必須。起動時に MaxObjects 分の CBV を一括生成し、
//    描画時は GPU ディスクリプタを「オフセット指定で差し替え」るだけにして軽量化。
//  - Render() の CBV バインドは 1 回のみ（過去の重複呼び出しを削除）。
//  - 同期は「毎フレーム完全同期」（安全だが性能は控えめ）。スループットを上げるなら
//    フレーム・リソースの二重化/三重化と書き込み先のラウンドロビン化が次の一手。
//======================================================================================

#include "D3D12Renderer.h"            // このレンダラーの宣言（FrameCount などはヘッダ側を参照）
#include "SceneConstantBuffer.h"      // C++/HLSL 間でメモリレイアウトを一致させる定数バッファ構造体
#include "d3dx12.h"                   // CD3DX12_* ヘルパ（DX12公式サンプルの便利ラッパ群）

#include <stdexcept>                  // 致命的エラー時に例外を投げる
#include <d3dcompiler.h>              // ランタイム HLSL コンパイル（D3DCompile）
#include <d3d12sdklayers.h>           
#include <sstream>                    // デバッグ出力の整形
#include <comdef.h>                   // _com_error（HRESULT→人間可読メッセージ）
#include <functional>                 // 再帰ラムダ用
#include <cmath>                      // isfinite / fabsf
#include <cstring>                    // std::strlen / std::memcpy
#include <dxgi1_6.h>                  // もしくは <dxgi1_4.h>

#pragma comment(lib, "d3dcompiler.lib")   // D3DCompile 用
#pragma comment(lib, "d3d12.lib")         // D3D12 本体
#pragma comment(lib, "dxgi.lib")          // DXGI（アダプタ/スワップチェイン）

using Microsoft::WRL::ComPtr;        // COM ポインタ管理（AddRef/Release 自動化）

namespace // この翻訳単位のみで使うユーティリティ
{
    //----------------------------------------------------------------------------------
    // LogHRESULTError
    // 目的  : HRESULT 失敗時に VS の出力へ詳細ログを出す（成功時は何もしない）
    // 引数  : hr  … エラーコード
    //         msg … 併せて出したいコンテキスト文字列（英語/日本語可）
    // 備考  : _com_error 経由で OS のエラーメッセージを取得して可読性を上げる
    //----------------------------------------------------------------------------------
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
}

//--------------------------------------------------------------------------------------
// 一度に描ける最大オブジェクト数。
// ・CBV（定数バッファビュー）スロット数と Upload バッファサイズに一致させること。
// ・将来オブジェクト数を拡張する場合は、ディスクリプタヒープ/CB を可変長に。
//--------------------------------------------------------------------------------------
constexpr UINT MaxObjects = 100;

//======================================================================================
// コンストラクタ / デストラクタ
//======================================================================================
D3D12Renderer::D3D12Renderer()
    : m_Width(0), m_Height(0),
    frameIndex(0),            // 現フレームのバックバッファインデックス（Present 後に更新）
    fenceValue(0),            // 次に Signal するフェンス値
    fenceEvent(nullptr),      // GPU 完了通知用の OS イベント（自動リセット）
    m_pCbvDataBegin(nullptr), // 永続マップした CB の CPU 側ポインタ（Upload ヒープ）
    m_frameCount(0),
    rtvDescriptorSize(0),
    m_ViewMatrix(DirectX::XMMatrixIdentity()),      // 既定は単位行列（安全）
    m_ProjectionMatrix(DirectX::XMMatrixIdentity()) // 同上
{
    // ここでは重い初期化は行わず、ゼロ初期化のみ。リソース確保は Initialize() で。
}

D3D12Renderer::~D3D12Renderer()
{
    // デストラクタでは確実に GPU 側の処理完了を待ってから破棄する。
    Cleanup();
}

//======================================================================================
// 1) 外から呼ぶ順（API エントリ）
//    Initialize → Render → Resize → Cleanup
//======================================================================================

/**
 * Initialize
 * 概要   : DX12 の主要オブジェクト（デバイス→キュー→スワップチェイン→RTV→DSV→
 *          コマンド関連→PSO）を依存順に作成し、シーン共通の CBV テーブルを事前生成。
 * 引数   : hwnd   … 出力先ウィンドウハンドル
 *          width  … 初期バックバッファ幅
 *          height … 初期バックバッファ高さ
 * 戻り値 : 成功で true / 失敗で false（詳細は VS 出力ウィンドウのログを参照）
 * 前提   : 本クラスは新規状態（再初期化は未考慮）
 * 事後   : フェンス/イベント/ディスクリプタヒープ/CB Upload など一式が有効化される
 * 失敗時 : 途中で false を返す（呼び出し側で中断し、必要なら Cleanup を呼ぶ）
 * 同期   : 初期化中に他スレッドから Render/Resize/Cleanup を呼ばないこと
 * 性能   : CBV は MaxObjects 分を起動時に一括生成 → 毎フレームのディスクリプタ作成コストゼロ
 */
bool D3D12Renderer::Initialize(HWND hwnd, UINT width, UINT height)
{
    m_Width = width;   // スワップチェインやビューポートに使用
    m_Height = height;

    //--- DX12 基本オブジェクトの生成（依存順） --------------------------------
    if (!CreateDevice())                         return false; // デバイスは最上位
    if (!CreateCommandQueue())                   return false; // GPU へコマンドを送る通路
    if (!CreateSwapChain(hwnd, width, height))   return false; // 表示用のバックバッファ群
    if (!CreateRenderTargetViews())              return false; // RTV を FrameCount 分作成
    if (!CreateDepthStencilBuffer(width, height))return false; // 深度テクスチャ + DSV
    if (!CreateCommandAllocatorsAndList())       return false; // 録画器（アロケータ/リスト）
    if (!CreatePipelineState())                  return false; // ルートシグネチャ + PSO

    //--- シーン共通の CB バッファ/CBV 事前生成 ---------------------------------
    // D3D12 の CB は 256B アライン必須（SizeInBytes も 256 の倍数）。
    const UINT alignedConstantBufferSize =
        (sizeof(SceneConstantBuffer) + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)
        & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);

    // シェーダ可視な CBV/SRV/UAV ヒープ（描画時 SetDescriptorHeaps で束ねて渡す）
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = MaxObjects; // 1 オブジェクト = 1 CBV
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    HRESULT hr = device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateDescriptorHeap for CBV failed");
        return false;
    }

    // Upload ヒープに全オブジェクト分の CB を敷き詰める
    //  - CPU 書き込み / GPU 読み取り。頻繁更新向け。静的なら Default ヒープ移行を検討。
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC resourceDesc =
        CD3DX12_RESOURCE_DESC::Buffer(alignedConstantBufferSize * MaxObjects);

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,  // Upload は GenericRead 固定で OK
        nullptr,
        IID_PPV_ARGS(&m_constantBuffer)
    );
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateCommittedResource for constant buffer failed");
        return false;
    }

    // 永続マップ（persistent mapping）。readRange=(0,0) は CPU Read 無しを意味。
    CD3DX12_RANGE readRange(0, 0);
    hr = m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "constantBuffer->Map failed");
        return false;
    }

    // 起動時に MaxObjects 分の CBV を一括生成。描画時は「オフセット指定」だけで済む。
    {
        m_cbStride = (sizeof(SceneConstantBuffer) + 255) & ~255u; // 256B アライン
        m_cbvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        CD3DX12_CPU_DESCRIPTOR_HANDLE cpu(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT i = 0; i < MaxObjects; ++i)
        {
            D3D12_CONSTANT_BUFFER_VIEW_DESC d{};
            d.BufferLocation = m_constantBuffer->GetGPUVirtualAddress() + i * m_cbStride;
            d.SizeInBytes = m_cbStride; // 256 の倍数必須
            device->CreateConstantBufferView(&d, cpu);
            cpu.Offset(1, m_cbvDescriptorSize);
        }
    }

    //--- CPU/GPU 同期（Fence + イベント）---------------------------------------
    // ・フェンス値は単調増加。Signal で「この値に到達したら完了」という目印を打つ。
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateFence failed");
        return false;
    }
    fenceValue = 1; // 初回 Signal で 1 を書く

    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr); // auto-reset
    if (fenceEvent == nullptr)
    {
        OutputDebugStringA("[D3D12Renderer ERROR] CreateEvent failed\n");
        return false;
    }

    return true;
}

/**
 * Render
 * 概要   : 1 フレーム描画。Present→RTV 遷移 → クリア → シーン描画 → RTV→Present → Present。
 * 前提   : Initialize 済み。m_CurrentScene と m_Camera が妥当。
 * 事後   : フレームが 1 つ進む（m_frameCount++）。本実装では毎回完全同期して戻る。
 * 同期   : 終端で WaitForPreviousFrame() を呼ぶため CPU/GPU の並行性は低いが確実。
 * 注意   : MaxObjects を超える描画要求は安全にスキップ。
 */
void D3D12Renderer::Render()
{
    HRESULT hr;

    // (1) 当該フレームのアロケータ/コマンドリストを Reset
    hr = commandAllocators[frameIndex]->Reset();
    if (FAILED(hr)) { LogHRESULTError(hr, "Render: CommandAllocator Reset failed"); return; }

    hr = commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get());
    if (FAILED(hr)) { LogHRESULTError(hr, "Render: CommandList Reset failed"); return; }

    // (2) カメラが無ければ描画できない（安全にスキップ）
    using namespace DirectX;
    if (!m_Camera) return;

    const XMMATRIX viewMatrix = m_Camera->GetViewMatrix();
    const XMMATRIX projMatrix = m_Camera->GetProjectionMatrix();

    // (3) Present → RenderTarget へ遷移
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        rtvHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &barrier);

    // (4) クリア（深度 → カラーバッファ）
    const float clearColor[] = { 0.2f, 0.2f, 0.4f, 1.0f }; // 背景色（調整用）
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // (5) ルート/ヒープ/ビューポート/シザー/トポロジ
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() }; // SHADER_VISIBLE 必須
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    D3D12_VIEWPORT viewport{ 0.0f, 0.0f, (float)m_Width, (float)m_Height, 0.0f, 1.0f };
    commandList->RSSetViewports(1, &viewport);

    D3D12_RECT scissorRect{ 0, 0, (LONG)m_Width, (LONG)m_Height };
    commandList->RSSetScissorRects(1, &scissorRect);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // (6) シーン全体を走査して描画（GameObject ツリーを再帰）
    if (m_CurrentScene)
    {
        UINT objectIndex = 0; // CBV スロット割当（0..MaxObjects-1）

        // 内部ラムダ：1 つの GameObject を描画し、子を再帰処理
        std::function<void(std::shared_ptr<GameObject>)> renderGameObject =
            [&](std::shared_ptr<GameObject> gameObject)
            {
                if (!gameObject || objectIndex >= MaxObjects) return; // スロット満了で打ち切り

                auto meshRenderer = gameObject->GetComponent<MeshRendererComponent>();

                // 必要な GPU リソースが揃っている場合のみ描画
                if (meshRenderer && meshRenderer->VertexBuffer && meshRenderer->IndexBuffer && meshRenderer->IndexCount > 0)
                {
                    // (6-1) MVP（row-major 前提：右から適用 → world * view * proj）
                    XMMATRIX world = gameObject->Transform->GetWorldMatrix();
                    XMMATRIX mvp = world * viewMatrix * projMatrix;

                    // (6-2) 法線行列：transpose(inverse(world))
                    //       det が非有限/極小なら単位行列にフォールバック（特異行列対策）
                    XMVECTOR det;
                    XMMATRIX worldInv = XMMatrixInverse(&det, world);
                    float detScalar = XMVectorGetX(det);
                    if (!std::isfinite(detScalar) || std::fabs(detScalar) < 1e-8f)
                        worldInv = XMMatrixIdentity();
                    XMMATRIX worldIT = XMMatrixTranspose(worldInv);

                    // (6-3) CB 書き込み（HLSL 側 row_major のため転置不要）
                    SceneConstantBuffer cb{};
                    XMStoreFloat4x4(&cb.mvp, mvp);
                    XMStoreFloat4x4(&cb.world, world);
                    XMStoreFloat4x4(&cb.worldIT, worldIT);

                    // ライト方向（ワールド空間）：見やすい陰影用に斜め上から
                    XMVECTOR L = XMVector3Normalize(XMVectorSet(0.0f, -1.0f, -1.0f, 0.0f));
                    XMStoreFloat3(&cb.lightDir, L);
                    cb.pad = 0.0f; // 16B アライン穴埋め

                    // (6-4) Upload バッファ（永続マップ）へ memcpy
                    //       本実装は毎フレーム完全同期なので、同領域の上書きでも破綻しない。
                    std::memcpy(
                        m_pCbvDataBegin + objectIndex * m_cbStride,
                        &cb,
                        sizeof(cb));

                    // (6-5) CBV バインド：起動時に作成済みのディスクリプタをオフセットで指定
                    commandList->SetGraphicsRootDescriptorTable(
                        0,
                        CD3DX12_GPU_DESCRIPTOR_HANDLE(
                            m_cbvHeap->GetGPUDescriptorHandleForHeapStart(),
                            objectIndex,
                            m_cbvDescriptorSize));

                    // (6-6) VB/IB セット → ドロー
                    commandList->IASetVertexBuffers(0, 1, &meshRenderer->VertexBufferView);
                    commandList->IASetIndexBuffer(&meshRenderer->IndexBufferView);
                    commandList->DrawIndexedInstanced(meshRenderer->IndexCount, 1, 0, 0, 0);

                    objectIndex++; // 次の CBV スロットへ
                }

                // 子ノードを再帰
                for (const auto& child : gameObject->GetChildren())
                    renderGameObject(child);
            };

        // ルートノードから全体を走査
        for (const auto& rootGo : m_CurrentScene->GetRootGameObjects())
            renderGameObject(rootGo);
    }
    else
    {
        // シーン未設定の通知（Silent fail より問題に気づきやすい）
        OutputDebugStringA("[D3D12Renderer WARNING] Render: No scene set to render.\n");
    }

    // (7) RenderTarget → Present へ遷移（Present 前に戻し忘れ注意）
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    commandList->ResourceBarrier(1, &barrier);

    // (8) コマンドを閉じて実行 → Present
    hr = commandList->Close();
    if (FAILED(hr)) { LogHRESULTError(hr, "Render: CommandList Close failed"); return; }

    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present。引数 (SyncInterval=1) はほぼ vsync ON 相当。
    hr = swapChain->Present(1, 0);
    if (FAILED(hr)) { LogHRESULTError(hr, "Render: Present failed"); return; }

    // (9) 完全同期してフレームを進行（安全側）
    WaitForPreviousFrame();
    m_frameCount++;
}

/**
 * Resize
 * 概要   : スワップチェインのバッファサイズ変更に伴う RTV/DSV の再作成。
 * 引数   : width/height … 新しいバックバッファサイズ
 * 前提   : Initialize 済み。Resize 中に Render は走らないこと（アプリ側で排他）。
 * 事後   : frameIndex はスワップチェインから取り直し。RTV/DSV は新サイズに更新。
 * 注意   : 0 サイズ（最小化など）は何もしない。
 */
void D3D12Renderer::Resize(UINT width, UINT height) noexcept
{
    if (width == 0 || height == 0) return;

    // 表示側で毎フレーム m_Width/Height を使っているので更新
    m_Width = width;
    m_Height = height;

    // 古いバックバッファ/DSV を GPU が使い終えるまで待つ
    WaitForGPU(); // ないなら WaitForPreviousFrame() でも可

    // 古いリソースを解放（ComPtr は Reset で Release）
    for (UINT i = 0; i < FrameCount; ++i) {
        renderTargets[i].Reset();
    }
    depthStencilBuffer.Reset();
    dsvHeap.Reset(); // 古い DSV ヒープを確実に捨てる

    // スワップチェインのサイズだけ変更（フォーマット/フラグは据え置き）
    DXGI_SWAP_CHAIN_DESC1 desc1{};
    swapChain->GetDesc1(&desc1);
    HRESULT hr = swapChain->ResizeBuffers(
        FrameCount,
        width, height,
        desc1.Format,
        desc1.Flags);
    if (FAILED(hr)) { LogHRESULTError(hr, "ResizeBuffers failed"); return; }

    frameIndex = swapChain->GetCurrentBackBufferIndex();

    // RTV 再作成（ヒープは再利用）
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(rtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT i = 0; i < FrameCount; ++i) {
            hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
            if (FAILED(hr)) { LogHRESULTError(hr, "GetBuffer failed in Resize"); return; }
            device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtv);
            rtv.Offset(1, rtvDescriptorSize);
        }
    }

    // DSV 再作成
    if (!CreateDepthStencilBuffer(width, height)) {
        OutputDebugStringA("[Resize] CreateDepthStencilBuffer failed\n");
        return;
    }

    // Viewport/Scissor は Render() 内で m_Width/Height から毎回設定するのでここでは不要
}

/**
 * Cleanup
 * 概要   : GPU 完了待ち → 永続マップ解除 → OS ハンドル解放。
 * 前提   : どのタイミングで呼ばれても安全（ただし二重呼び出し時の nullptr 保護あり）。
 * 事後   : fenceEvent / m_pCbvDataBegin は無効化。ComPtr は自動的に Release。
 */
void D3D12Renderer::Cleanup()
{
    if (device) // device が nullptr の場合、既に破棄済みの可能性
    {
        WaitForGPU();
    }
    if (fenceEvent) {
        CloseHandle(fenceEvent);
        fenceEvent = nullptr;
    }
    if (m_constantBuffer && m_pCbvDataBegin) {
        // 永続マップしていた Upload バッファをアンマップ
        m_constantBuffer->Unmap(0, nullptr);
        m_pCbvDataBegin = nullptr;
    }
    // ComPtr はスコープ外で自動解放。順序依存が強い場合のみ明示リセットを検討。
}

//======================================================================================
// 2) 作成系（Initialize から呼ばれる）
//======================================================================================

/**
 * CreateDevice
 * 目的   : DXGI でハードウェアアダプタを列挙し、D3D12 デバイスを作成。
 * 前提   : なし（最初に呼ばれる）。
 * 失敗時 : false（ログ出力）。本実装では WARP へのフォールバックは行わない。
 * デバッグ: _DEBUG 時は DebugLayer を有効化（パフォーマンス低下あり）。
 */
bool D3D12Renderer::CreateDevice()
{
    HRESULT hr;
    ComPtr<IDXGIFactory4> factory;
    UINT createFactoryFlags = 0;

#ifdef _DEBUG
    // Debug Layer：D3D12 API 呼び出しの前後条件チェックを有効化
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateDXGIFactory2 failed"); return false; }

    // ハードウェアアダプタ列挙：WARP（ソフトウェア）はスキップ
    // Feature Level は最小 11.0 で作成（必要に応じて引き上げる）
    ComPtr<IDXGIAdapter1> hardwareAdapter;
    for (UINT adapterIndex = 0;; ++adapterIndex)
    {
        if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters1(adapterIndex, &hardwareAdapter))
            break;

        DXGI_ADAPTER_DESC1 desc;
        hardwareAdapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

        hr = D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
        if (SUCCEEDED(hr)) break;
    }

    if (!device)
    {
        // WARP fallback を入れる設計もあるが、本実装では失敗で終了。
        OutputDebugStringA("[D3D12Renderer ERROR] Failed to create D3D12 device\n");
        return false;
    }

#ifdef _DEBUG
    // ②デバイス作成「後」：InfoQueue でブレーク＆フィルタ設定
    {
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(device.As(&infoQueue)))
        {
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

            // ノイズ抑制(必要に応じて調整)
            D3D12_MESSAGE_ID denyIds[] = {
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
            };
            D3D12_INFO_QUEUE_FILTER filter{};
            filter.DenyList.NumIDs = _countof(denyIds);
            filter.DenyList.pIDList = denyIds;
            infoQueue->PushStorageFilter(&filter);
        }
    }
#endif

    return true;
}

/**
 * CreateCommandQueue
 * 目的   : Direct（汎用）タイプのコマンドキューを作成。
 * 備考   : Direct は Graphics/Compute/Copy を全て流せる。
 */
bool D3D12Renderer::CreateCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    HRESULT hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommandQueue failed"); return false; }
    return true;
}

/**
 * CreateSwapChain
 * 目的   : スワップチェイン（Flip Discard）を作成。
 * 引数   : hwnd/width/height … 初期化引数そのまま。
 * 注意   : tearing を使う場合は AllowTearing/PresentFlags など追加考慮が必要。
 */
bool D3D12Renderer::CreateSwapChain(HWND hwnd, UINT width, UINT height)
{
    ComPtr<IDXGIFactory4> factory;
    UINT createFactoryFlags = 0;
#ifdef _DEBUG
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    HRESULT hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateDXGIFactory2 failed in CreateSwapChain"); return false; }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;              // バックバッファ数（ヘッダ側の定義と一致）
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // sRGB が必要なら *_UNORM_SRGB
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // 近年の推奨
    swapChainDesc.SampleDesc.Count = 1;                              // MSAA 無し

    ComPtr<IDXGISwapChain1> swapChain1;
    hr = factory->CreateSwapChainForHwnd(
        commandQueue.Get(), hwnd, &swapChainDesc,
        nullptr, nullptr, &swapChain1);
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateSwapChainForHwnd failed"); return false; }

    hr = swapChain1.As(&swapChain); // IDXGISwapChain4 へ QI
    if (FAILED(hr)) { LogHRESULTError(hr, "swapChain1.As failed"); return false; }

    hr = factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(hr)) { LogHRESULTError(hr, "MakeWindowAssociation(DXGI_MWA_NO_ALT_ENTER) failed"); /* 継続可 */ }

    frameIndex = swapChain->GetCurrentBackBufferIndex(); // 0..FrameCount-1
    return true;
}

/**
 * CreateRenderTargetViews
 * 目的   : バックバッファごとの RTV を作成（ディスクリプタヒープに並べる）。
 * 事後   : rtvDescriptorSize を取得。renderTargets[i] が有効化。
 */
bool D3D12Renderer::CreateRenderTargetViews()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // CPU からのみ参照

    HRESULT hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateDescriptorHeap for RTVs failed"); return false; }

    // ヒープ内インクリメント幅を取得（この値でハンドルをオフセットする）
    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // 先頭 CPU ハンドルから順に RTV を作成
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < FrameCount; i++)
    {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
        if (FAILED(hr)) { LogHRESULTError(hr, "GetBuffer failed for render target"); return false; }

        device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, rtvDescriptorSize);
    }
    return true;
}

/**
 * CreateDepthStencilBuffer
 * 目的   : 深度バッファ用テクスチャ（Default ヒープ）と DSV を作成。
 * 引数   : width/height … 現在のバックバッファサイズに合わせる。
 * 注意   : Stencil を使う場合はフォーマット/フラグを適切に変更。
 */
bool D3D12Renderer::CreateDepthStencilBuffer(UINT width, UINT height)
{
    HRESULT hr;

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1; // 今は 1 枚のみ
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateDescriptorHeap for DSV failed"); return false; }

    // 深度テクスチャ（Default ヒープ）
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D32_FLOAT, width, height, 1, 1, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthClearValue.DepthStencil.Depth = 1.0f;
    depthClearValue.DepthStencil.Stencil = 0;

    hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, // 初期状態は深度書き込み可能
        &depthClearValue,
        IID_PPV_ARGS(&depthStencilBuffer)
    );
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommittedResource for DepthStencilBuffer failed"); return false; }

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    device->CreateDepthStencilView(depthStencilBuffer.Get(), &dsvDesc, dsvHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
}

/**
 * CreateCommandAllocatorsAndList
 * 目的   : FrameCount 分のコマンドアロケータと、使い回す 1 本のコマンドリストを作成。
 * 注意   : アロケータは GPU が使い終えるまで Reset 不可 → フレーム毎に分けるのが定石。
 */
bool D3D12Renderer::CreateCommandAllocatorsAndList()
{
    HRESULT hr;
    for (UINT i = 0; i < FrameCount; i++)
    {
        hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[i]));
        if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommandAllocator failed"); return false; }
    }

    // コマンドリストは一度作って Close。描画時に Reset して使い回す。
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&commandList));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommandList failed"); return false; }

    commandList->Close(); // Close→Reset→Close のサイクルで運用
    return true;
}

/**
 * CreatePipelineState
 * 目的   : ルートシグネチャ（CBV テーブル b0 のみ）と PSO（Lambert 拡散）を作成。
 * シェーダ: HLSL 内の行列は row_major とし、C++ 側からは転置せずに詰める前提。
 * 注意   : 製品コードでは DXC(DXIL) やオフラインコンパイル＋キャッシュが望ましい。
 */
bool D3D12Renderer::CreatePipelineState()
{
    //--- ルートシグネチャ（CBV テーブル b0 のみ：最小構成） ---
    CD3DX12_DESCRIPTOR_RANGE ranges[1] = {};
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0

    CD3DX12_ROOT_PARAMETER rootParameters[1] = {};
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;
    // 入力レイアウトを IA で使う（頂点バッファの意味付けを許可）
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signatureBlob, errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        LogHRESULTError(hr, "D3D12SerializeRootSignature failed");
        return false;
    }

    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateRootSignature failed"); return false; }

    //--- シェーダ（row_major を明示。法線は inverse-transpose(world) で変換） ---
    const char* vsSource = R"(
cbuffer cb0 : register(b0)
{
    row_major float4x4 g_mvp;       // C++ 側から「転置せず」詰める前提
    row_major float4x4 g_world;
    row_major float4x4 g_worldIT;   // inverse-transpose(world)
    float3 g_lightDir;              // World 空間のライト入射方向（向き）
    float  pad;
};

struct VSInput
{
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float4 color  : COLOR;
};

struct PSInput
{
    float4 pos    : SV_POSITION;
    float3 normal : NORMAL;    // world 空間
    float4 color  : COLOR;
};

PSInput main(VSInput input)
{
    PSInput o;
    // mul(行ベクトル, 行列)（row_major 前提）
    o.pos    = mul(float4(input.pos, 1.0f), g_mvp);
    o.normal = normalize(mul(input.normal, (float3x3)g_worldIT));
    o.color  = input.color;
    return o;
}
)";

    const char* psSource = R"(
cbuffer cb0 : register(b0)
{
    row_major float4x4 g_mvp;
    row_major float4x4 g_world;
    row_major float4x4 g_worldIT;
    float3 g_lightDir; float pad;
};

struct PSInput
{
    float4 pos    : SV_POSITION;
    float3 normal : NORMAL; // world
    float4 color  : COLOR;
};

float4 main(PSInput input) : SV_TARGET
{
    // シンプル Lambert：入射方向は -g_lightDir として使用
    float NdotL = max(dot(normalize(input.normal), -normalize(g_lightDir)), 0.0f);
    float3 diffuse = input.color.rgb * NdotL;
    return float4(diffuse, input.color.a);
}
)";

    ComPtr<ID3DBlob> vertexShader, pixelShader, compileErrors;

    // ※製品では DXC（DXIL）やオフラインコンパイル + シェーダキャッシュ推奨
    hr = D3DCompile(vsSource, std::strlen(vsSource), nullptr, nullptr, nullptr, "main", "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vertexShader, &compileErrors);
    if (FAILED(hr)) {
        if (compileErrors) OutputDebugStringA((char*)compileErrors->GetBufferPointer());
        LogHRESULTError(hr, "Vertex shader compilation failed");
        return false;
    }
    hr = D3DCompile(psSource, std::strlen(psSource), nullptr, nullptr, nullptr, "main", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &pixelShader, &compileErrors);
    if (FAILED(hr)) {
        if (compileErrors) OutputDebugStringA((char*)compileErrors->GetBufferPointer());
        LogHRESULTError(hr, "Pixel shader compilation failed");
        return false;
    }

    // 頂点レイアウト（P/N/C）：P(12) N(12) C(16) = 40B/頂点
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // PSO：基本は D3D12_DEFAULT。Depth/Blend/Rasterizer を素のまま使用。
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;  // 近景が小さいZで可視
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;            // sRGB なら *_UNORM_SRGB
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;

    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateGraphicsPipelineState failed"); return false; }
    return true;
}

/**
 * CreateMeshRendererResources
 * 目的   : 1 メッシュの頂点/インデックスを Upload ヒープ上に確保し、ビューを設定。
 * 引数   : meshRenderer … メッシュデータ保有コンポーネント
 * 前提   : m_MeshData.Vertices / Indices が妥当（空は不可）。
 * 注意   : 大量/静的データでは Default ヒープへ移し、Copy キューで転送する方が高速。
 */
bool D3D12Renderer::CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> meshRenderer)
{
    // 妥当性チェック：頂点数ゼロ等の空メッシュは描画不可
    if (!meshRenderer || meshRenderer->m_MeshData.Indices.empty())
    {
        OutputDebugStringA("[D3D12Renderer WARNING] CreateMeshRendererResources: Invalid MeshRendererComponent or empty mesh data.\n");
        return false;
    }

    HRESULT hr;
    // Upload ヒープ（CPU 書き込み / GPU 読み取り）
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    //--- 頂点バッファ ---
    const UINT vertexBufferSize = (UINT)meshRenderer->m_MeshData.Vertices.size() * sizeof(Vertex);
    D3D12_RESOURCE_DESC vertexResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

    hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &vertexResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&meshRenderer->VertexBuffer));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommittedResource for vertex buffer failed in CreateMeshRendererResources"); return false; }

    // Map→memcpy→Unmap（CPU→Upload）。readRange=(0,0) は CPU Read 無し。
    UINT8* pVertexDataBegin = nullptr;
    CD3DX12_RANGE readRange(0, 0);
    hr = meshRenderer->VertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    if (FAILED(hr)) { LogHRESULTError(hr, "Vertex buffer Map failed in CreateMeshRendererResources"); return false; }
    std::memcpy(pVertexDataBegin, meshRenderer->m_MeshData.Vertices.data(), vertexBufferSize);
    meshRenderer->VertexBuffer->Unmap(0, nullptr);

    meshRenderer->VertexBufferView.BufferLocation = meshRenderer->VertexBuffer->GetGPUVirtualAddress();
    meshRenderer->VertexBufferView.StrideInBytes = sizeof(Vertex);
    meshRenderer->VertexBufferView.SizeInBytes = vertexBufferSize;

    //--- インデックスバッファ ---
    const UINT indexBufferSize = (UINT)meshRenderer->m_MeshData.Indices.size() * sizeof(unsigned int);
    D3D12_RESOURCE_DESC indexResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

    hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &indexResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&meshRenderer->IndexBuffer));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommittedResource for index buffer failed in CreateMeshRendererResources"); return false; }

    UINT8* pIndexDataBegin = nullptr;
    hr = meshRenderer->IndexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin));
    if (FAILED(hr)) { LogHRESULTError(hr, "Index buffer Map failed in CreateMeshRendererResources"); return false; }
    std::memcpy(pIndexDataBegin, meshRenderer->m_MeshData.Indices.data(), indexBufferSize);
    meshRenderer->IndexBuffer->Unmap(0, nullptr);

    meshRenderer->IndexBufferView.BufferLocation = meshRenderer->IndexBuffer->GetGPUVirtualAddress();
    meshRenderer->IndexBufferView.Format = DXGI_FORMAT_R32_UINT; // 32bit index。16bit なら R16_UINT
    meshRenderer->IndexBufferView.SizeInBytes = indexBufferSize;

    meshRenderer->IndexCount = (UINT)meshRenderer->m_MeshData.Indices.size();
    return true;
}

//======================================================================================
// 3) 同期・ユーティリティ
//======================================================================================

/**
 * WaitForPreviousFrame
 * 目的   : 現在のキューにフェンス値を Signal → まだ未達なら OS イベントで待機。
 * 前提   : commandQueue / fence / fenceEvent が有効。
 * 事後   : fenceValue++。frameIndex を最新に更新（Present 後に変わり得る）。
 * 例外   : Signal/SetEventOnCompletion 失敗時は runtime_error を投げる。
 */
void D3D12Renderer::WaitForPreviousFrame()
{
    HRESULT hr = commandQueue->Signal(fence.Get(), fenceValue);
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "WaitForPreviousFrame: commandQueue->Signal failed");
        throw std::runtime_error("commandQueue->Signal failed");
    }

    if (fence->GetCompletedValue() < fenceValue)
    {
        hr = fence->SetEventOnCompletion(fenceValue, fenceEvent);
        if (FAILED(hr))
        {
            LogHRESULTError(hr, "WaitForPreviousFrame: fence->SetEventOnCompletion failed");
            throw std::runtime_error("fence->SetEventOnCompletion failed");
        }
        // GPU がフェンス値に到達するまで待機（ブロッキング）
        WaitForSingleObject(fenceEvent, INFINITE);
    }
    fenceValue++; // 次のフレームで使う値へ進める

    // Present 後の最新バックバッファ番号を取得（トリプルバッファ等で変わる）
    frameIndex = swapChain->GetCurrentBackBufferIndex();
}

/**
 * WaitForGPU
 * 目的   : 例外を投げない簡易待機。Resize などで「とりあえず待つ」用途に。
 * 注意   : 失敗しても戻り値無し（ベストエフォート）。堅牢性が必要なら WaitForPreviousFrame を使用。
 */
void D3D12Renderer::WaitForGPU() noexcept
{
    if (!commandQueue || !fence) return;
    const UINT64 value = ++fenceValue;
    if (FAILED(commandQueue->Signal(fence.Get(), value))) return;

    if (fence->GetCompletedValue() < value) {
        if (SUCCEEDED(fence->SetEventOnCompletion(value, fenceEvent))) {
            WaitForSingleObject(fenceEvent, INFINITE);
        }
    }
}

/**
 * DrawMesh
 * 目的   : 既に PSO/ルート/CBV/RT 等が設定済みであることを前提に、単一メッシュを即時ドロー。
 * 前提   : commandList は Recording 中、IA/CBV/RS 等は適切に設定済み。
 * 注意   : 外部から呼ぶ際は「呼び出し前の前提条件」を満たすこと。
 */
void D3D12Renderer::DrawMesh(MeshRendererComponent* meshRenderer)
{
    if (!meshRenderer) return;

    commandList->IASetVertexBuffers(0, 1, &meshRenderer->VertexBufferView);
    commandList->IASetIndexBuffer(&meshRenderer->IndexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->DrawIndexedInstanced(meshRenderer->IndexCount, 1, 0, 0, 0);
}
