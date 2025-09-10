#include "D3D12Renderer.h"            // このレンダラーの宣言（FrameCount などはヘッダ側を参照）
#include "SceneConstantBuffer.h"      // C++/HLSL 間でメモリレイアウトを一致させる定数バッファ構造体

#include <stdexcept>                  // 致命的エラー時に例外を投げる
#include <d3dcompiler.h>              // ランタイム HLSL コンパイル（D3DCompile）
#include "d3dx12.h"                   // CD3DX12_* ヘルパ（DX12公式サンプルの便利ラッパ群）
#include <sstream>                    // デバッグ出力の整形
#include <comdef.h>                   // _com_error（HRESULT→人間可読メッセージ）
#include <functional>                 // 再帰ラムダ用
#include <cmath>                      // isfinite / fabsf

#pragma comment(lib, "d3dcompiler.lib")   // D3DCompile 用
#pragma comment(lib, "d3d12.lib")         // D3D12 本体
#pragma comment(lib, "dxgi.lib")          // DXGI（アダプタ/スワップチェイン）

using Microsoft::WRL::ComPtr;        // COM ポインタ管理（AddRef/Release 自動化）

namespace // この翻訳単位のみで使うユーティリティ
{
    //----------------------------------------------------------------------------------
    // HRESULT 失敗時に VS の出力へ詳細ログを出すヘルパ。
    // ・FAILED(hr) のときだけ出力（成功時は無視）
    // ・_com_error 経由でエラー文字列を取得（利便性重視の簡易実装）
    // ・本番ではロガーに流す/エラーコードで分岐なども検討
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
// ・CBV（定数バッファビュー）スロット / Upload バッファサイズ と一致させること
// ・動的に増やす予定があるなら、ディスクリプタヒープを拡張可能な設計に
//--------------------------------------------------------------------------------------
const UINT MaxObjects = 100;

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
    m_ViewMatrix(DirectX::XMMatrixIdentity()),      // デバッグ上、既定は単位
    m_ProjectionMatrix(DirectX::XMMatrixIdentity()) // 同上
{
    // 重い初期化は Initialize() 内で行う。ここではメンバのゼロ初期化のみ。
}

D3D12Renderer::~D3D12Renderer()
{
    // GPU がまだ作業中の可能性があるため、必ず同期してから解放。
    Cleanup();
}

//======================================================================================
// 初期化（順序は依存関係に注意）
// ・戻り値 false なら、ログを見ればどこで失敗したか分かるように設計
//======================================================================================
bool D3D12Renderer::Initialize(HWND hwnd, UINT width, UINT height)
{
    m_Width = width;   // スワップチェインやビューポートに使用
    m_Height = height;

    //--- D3D12 基本オブジェクトの生成（依存順） -------------------------------
    if (!CreateDevice())                     return false; // デバイス（最上位、ほぼ全機能の親）
    if (!CreateCommandQueue())               return false; // コマンド送出経路
    if (!CreateSwapChain(hwnd, width, height)) return false; // 画面表示用バッファ群
    if (!CreateRenderTargetViews())          return false; // バックバッファ用 RTV
    if (!CreateDepthStencilBuffer(width, height)) return false; // DSV（深度）
    if (!CreateCommandAllocatorsAndList())   return false; // レコード用リスト/アロケータ
    if (!CreatePipelineState())              return false; // ルートシグネチャ＋PSO

    //--- シーン共通の定数バッファ（CBV テーブル + Upload リソース）------------
    // D3D12 の CB は 256B アライン必須。SizeInBytes も 256 の倍数で指定する必要あり。
    // ※ D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT = 256
    const UINT alignedConstantBufferSize =
        (sizeof(SceneConstantBuffer) + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)
        & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);

    // シェーダから参照できるヒープ。CBV/SRV/UAV 用。SHADER_VISIBLE を忘れると
    // SetDescriptorHeaps / SetGraphicsRootDescriptorTable で参照できずにハマる。
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = MaxObjects; // 1 オブジェクト = 1 CBV の設計
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    HRESULT hr = device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateDescriptorHeap for CBV failed");
        return false;
    }

    // 1本の大きい Upload バッファに全オブジェクト分の CB を敷き詰める。
    // ・Upload は CPU 書き込み / GPU 読み取り。コヒーレントだが帯域は遅い。
    // ・静的データは Default ヒープ＆Copy 経由へ移行を検討。
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC resourceDesc =
        CD3DX12_RESOURCE_DESC::Buffer(alignedConstantBufferSize * MaxObjects);

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,  // Upload は常に GenericRead で OK
        nullptr,
        IID_PPV_ARGS(&m_constantBuffer)
    );
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateCommittedResource for constant buffer failed");
        return false;
    }

    // 永続マップ（persistent mapping）。readRange=(0,0) は CPU 読み込み無しの意味。
    CD3DX12_RANGE readRange(0, 0);
    hr = m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "constantBuffer->Map failed");
        return false;
    }
    // ※CBV自体（descriptor）は現在は描画ループで毎回生成しているがコストが高い。
    //   実装を育てる際は、起動時に MaxObjects 分を事前生成→描画はハンドル Offset のみ を推奨。

    //--- CPU/GPU 同期（Fence + イベント）---------------------------------------
    // ・フェンス値は単調増加。Signal 時に「この値を超えたら完了」とみなす。
    // ・SetEventOnCompletion で OS イベントに通知を飛ばし、Wait する。
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

//======================================================================================
// デバイス作成：適切なハードウェアアダプタを列挙して選択
// ・_DEBUG 時は API 使用ミス検知のため Debug Layer を有効化（パフォーマンス低下あり）
//======================================================================================
bool D3D12Renderer::CreateDevice()
{
    HRESULT hr;
    ComPtr<IDXGIFactory4> factory;
    UINT createFactoryFlags = 0;

#ifdef _DEBUG
    // DebugLayer：D3D12 API 呼び出しの前後条件チェックが有効になる。
    // ※GPU ベースの検証（GPU Validation）は別設定。必要に応じて追加。
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
    // Feature Level は最小 11.0 で作成（必要なら引き上げる）
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
        // ここで WARP fallback を入れる設計もあるが、本実装では失敗で終了。
        OutputDebugStringA("[D3D12Renderer ERROR] Failed to create D3D12 device\n");
        return false;
    }
    return true;
}

//======================================================================================
// コマンドキュー（Direct）
// ・Direct はグラフィックス/コピー/コンピュートを全て流せる（標準）
//======================================================================================
bool D3D12Renderer::CreateCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    HRESULT hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommandQueue failed"); return false; }
    return true;
}

//======================================================================================
// スワップチェイン（Flip Discard）
// ・プレゼントの近代的推奨モード。将来的に「テアリング対応」を入れる場合は
//   IDXGIFactory5::CheckFeatureSupport / DXGI_PRESENT_ALLOW_TEARING 等の考慮が必要。
//======================================================================================
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
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // sRGB が欲しければ *_UNORM_SRGB を選択
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // 近年の推奨
    swapChainDesc.SampleDesc.Count = 1;                        // MSAA 無し（使うなら PSO/DSV と揃える）

    ComPtr<IDXGISwapChain1> swapChain1;
    hr = factory->CreateSwapChainForHwnd(
        commandQueue.Get(), hwnd, &swapChainDesc,
        nullptr, nullptr, &swapChain1);
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateSwapChainForHwnd failed"); return false; }

    hr = swapChain1.As(&swapChain); // IDXGISwapChain4 へ QI
    if (FAILED(hr)) { LogHRESULTError(hr, "swapChain1.As failed"); return false; }

    frameIndex = swapChain->GetCurrentBackBufferIndex(); // 0..FrameCount-1
    return true;
}

//======================================================================================
// バックバッファ用 RTV
// ・1 つの RTV ディスクリプタヒープに、FrameCount 分を連続で割り当てる
//======================================================================================
bool D3D12Renderer::CreateRenderTargetViews()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameCount; // バックバッファの数だけ
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
        rtvHandle.Offset(1, rtvDescriptorSize); // 次のスロットへ
    }
    return true;
}

//======================================================================================
// 深度ステンシル（D32F, MSAA 無し）
// ・クリア値の設定は、Resource 作成時に渡す（最適化のヒント）
//======================================================================================
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
        DXGI_FORMAT_D32_FLOAT,           // 深度のみ。Stencil を使うなら D24_UNORM_S8_UINT 等。
        width, height, 1, 0, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    );

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

//======================================================================================
// コマンドアロケータ（フレーム数分）＋コマンドリスト
// ・コマンドアロケータは「GPU が使い終えるまで Reset 不可」→フレーム毎に分けるのが基本
//======================================================================================
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

    commandList->Close(); // Close→Reset→Close のサイクルで使うのが基本パターン
    return true;
}

//======================================================================================
// ルートシグネチャ + PSO（Lambert 拡散の最小構成）
// ・HLSL 側の行列は row_major。C++ 側で「転置せず」XMStoreFloat4x4 で詰める前提。
//======================================================================================
bool D3D12Renderer::CreatePipelineState()
{
    //--- ルートシグネチャ（CBV テーブル b0 のみ：最小構成） ---------------------
    // ※テクスチャを足すなら SRV/UAV の range を追加。サンプラは StaticSampler で入れることも多い。
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
    // mul(行ベクトル, 行列)（row_major 前提）。column_major の場合は書き方を変える。
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
    hr = D3DCompile(vsSource, strlen(vsSource), nullptr, nullptr, nullptr, "main", "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vertexShader, &compileErrors);
    if (FAILED(hr)) {
        if (compileErrors) OutputDebugStringA((char*)compileErrors->GetBufferPointer());
        LogHRESULTError(hr, "Vertex shader compilation failed");
        return false;
    }
    hr = D3DCompile(psSource, strlen(psSource), nullptr, nullptr, nullptr, "main", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &pixelShader, &compileErrors);
    if (FAILED(hr)) {
        if (compileErrors) OutputDebugStringA((char*)compileErrors->GetBufferPointer());
        LogHRESULTError(hr, "Pixel shader compilation failed");
        return false;
    }

    // 頂点レイアウト（P/N/C）：P(12) N(12) C(16) = 40B/頂点
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,     0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,     0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // PSO：基本は D3D12_DEFAULT。Depth/Blend/Rasterizer を素のまま使用。
    // MSAA を使うなら SampleDesc/DSV/RTV/SwapChain 側も一致させること。
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

//======================================================================================
// メッシュ GPU リソース（VB/IB：Upload ヒープ）
// ・デバッグ/プロトタイピング向き。大量/静的なら Default ヒープ＋Copy へ移行推奨。
//======================================================================================
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

    //--- 頂点バッファ ------------------------------------------------------------
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
    memcpy(pVertexDataBegin, meshRenderer->m_MeshData.Vertices.data(), vertexBufferSize);
    meshRenderer->VertexBuffer->Unmap(0, nullptr);

    meshRenderer->VertexBufferView.BufferLocation = meshRenderer->VertexBuffer->GetGPUVirtualAddress();
    meshRenderer->VertexBufferView.StrideInBytes = sizeof(Vertex);
    meshRenderer->VertexBufferView.SizeInBytes = vertexBufferSize;

    //--- インデックスバッファ ----------------------------------------------------
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
    memcpy(pIndexDataBegin, meshRenderer->m_MeshData.Indices.data(), indexBufferSize);
    meshRenderer->IndexBuffer->Unmap(0, nullptr);

    meshRenderer->IndexBufferView.BufferLocation = meshRenderer->IndexBuffer->GetGPUVirtualAddress();
    meshRenderer->IndexBufferView.Format = DXGI_FORMAT_R32_UINT; // 32bit index。16bit なら R16_UINT
    meshRenderer->IndexBufferView.SizeInBytes = indexBufferSize;

    meshRenderer->IndexCount = (UINT)meshRenderer->m_MeshData.Indices.size();
    return true;
}

//======================================================================================
// 1 フレーム描画：Present→RT へ遷移→クリア→各オブジェクト描画→Present へ戻す→表示
// ・本実装は毎フレーム「完全同期」する安全設計（性能は抑えめ）
//======================================================================================
void D3D12Renderer::Render()
{
    HRESULT hr;

    // (1) フレームのアロケータ/リスト準備（Reset）
    //     ※GPU がまだ使っているアロケータに Reset をかけると失敗するので注意。
    hr = commandAllocators[frameIndex]->Reset();
    if (FAILED(hr)) { LogHRESULTError(hr, "Render: CommandAllocator Reset failed"); return; }

    hr = commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get());
    if (FAILED(hr)) { LogHRESULTError(hr, "Render: CommandList Reset failed"); return; }

    // (2) カメラ行列（無ければ描画しない）
    using namespace DirectX;
    if (!m_Camera) return;

    const DirectX::XMMATRIX viewMatrix = m_Camera->GetViewMatrix();
    const DirectX::XMMATRIX projMatrix = m_Camera->GetProjectionMatrix();


    // (3) Present → RenderTarget へリソース遷移（必須）
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        rtvHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,       // いま画面表示に使われている状態
        D3D12_RESOURCE_STATE_RENDER_TARGET  // 書き込み可能な RT へ
    );
    commandList->ResourceBarrier(1, &barrier);

    // (4) クリア（順番は DSV→RTV どちらでもよいが、慣習的に両方クリア）
    const float clearColor[] = { 0.2f, 0.2f, 0.4f, 1.0f }; // 背景色（調整用）
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // (5) ルートシグネチャ / ヒープ / ビューポート / シザー / トポロジ
    commandList->SetGraphicsRootSignature(rootSignature.Get());

    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() }; // SHADER_VISIBLE 必須
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    D3D12_VIEWPORT viewport{ 0.0f, 0.0f, (float)m_Width, (float)m_Height, 0.0f, 1.0f };
    commandList->RSSetViewports(1, &viewport);

    D3D12_RECT scissorRect{ 0, 0, (LONG)m_Width, (LONG)m_Height };
    commandList->RSSetScissorRects(1, &scissorRect);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // (6) シーン全体を走査して描画
    if (m_CurrentScene)
    {
        int objectIndex = 0; // CBV スロット割り当て（0..MaxObjects-1）
        const UINT alignedConstantBufferSize =
            (sizeof(SceneConstantBuffer) + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)
            & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);

        const UINT cbvDescriptorSize =
            device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // 子を含めて再帰で描画。描画順序を制御したい場合はソートやレイヤを導入。
        std::function<void(std::shared_ptr<GameObject>)> renderGameObject =
            [&](std::shared_ptr<GameObject> gameObject)
            {
                if (!gameObject || objectIndex >= MaxObjects) return; // 溢れたら描画しない

                std::shared_ptr<MeshRendererComponent> meshRenderer =
                    gameObject->GetComponent<MeshRendererComponent>();

                // VB/IB/IndexCount が揃っているものだけ描画
                if (meshRenderer && meshRenderer->VertexBuffer && meshRenderer->IndexBuffer && meshRenderer->IndexCount > 0)
                {
                    // (6-1) 行列の算出：MVP（右から順番に適用されるため world * view * proj の順）
                    DirectX::XMMATRIX world = gameObject->Transform->GetWorldMatrix();
                    DirectX::XMMATRIX mvp = world * viewMatrix * projMatrix;

                    // (6-2) 法線行列：transpose(inverse(world))
                    DirectX::XMVECTOR det;
                    DirectX::XMMATRIX worldInv = DirectX::XMMatrixInverse(&det, world);
                    float detScalar = DirectX::XMVectorGetX(det);
                    if (!std::isfinite(detScalar) || fabsf(detScalar) < 1e-8f)
                    {
                        // 退化（特異行列）時の安全策：I でフォールバック
                        worldInv = DirectX::XMMatrixIdentity();
                    }
                    DirectX::XMMATRIX worldIT = DirectX::XMMatrixTranspose(worldInv);

                    // (6-3) 定数バッファへ書き込み（HLSL は row_major → 転置不要）
                    SceneConstantBuffer constantBufferData = {};
                    DirectX::XMStoreFloat4x4(&constantBufferData.mvp, mvp);
                    DirectX::XMStoreFloat4x4(&constantBufferData.world, world);
                    DirectX::XMStoreFloat4x4(&constantBufferData.worldIT, worldIT);

                    // 簡易ライト方向（World 空間）。見やすい陰影用に少し斜め上から。
                    DirectX::XMVECTOR L = DirectX::XMVector3Normalize(DirectX::XMVectorSet(0.0f, -1.0f, -1.0f, 0.0f));
                    DirectX::XMStoreFloat3(&constantBufferData.lightDir, L);
                    constantBufferData.pad = 0.0f; // 16B アライン用の穴埋め

                    // (6-4) Upload バッファ（永続マップ）へ memcpy
                    //       ※同一領域を GPU が参照中に書き換えるのは危険だが、
                    //         本実装は毎フレーム完全同期（WaitForPreviousFrame）で安全側。
                    memcpy(m_pCbvDataBegin + objectIndex * alignedConstantBufferSize,
                        &constantBufferData, sizeof(constantBufferData));

                    // (6-5) CBV の作成とバインド
                    //       ※毎フレーム CreateConstantBufferView は重い → 起動時一括生成が望ましい。
                    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                    cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress() + objectIndex * alignedConstantBufferSize;
                    cbvDesc.SizeInBytes = alignedConstantBufferSize; // 256 の倍数必須

                    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuCbvHandle(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
                    cpuCbvHandle.Offset(objectIndex, cbvDescriptorSize);
                    device->CreateConstantBufferView(&cbvDesc, cpuCbvHandle);

                    // ルート（テーブル）へセット：GPU 側のディスクリプタハンドルを使用する
                    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuCbvHandle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
                    gpuCbvHandle.Offset(objectIndex, cbvDescriptorSize);
                    commandList->SetGraphicsRootDescriptorTable(0, gpuCbvHandle);

                    // (6-6) VB/IB セット → ドロー
                    commandList->IASetVertexBuffers(0, 1, &meshRenderer->VertexBufferView);
                    commandList->IASetIndexBuffer(&meshRenderer->IndexBufferView);
                    commandList->DrawIndexedInstanced(meshRenderer->IndexCount, 1, 0, 0, 0);

                    objectIndex++; // 次のオブジェクトへスロットを進める
                }

                // 子ノードを再帰描画
                for (const auto& child : gameObject->GetChildren())
                    renderGameObject(child);
            };

        // ルートノードから開始（ツリー全体を走査）
        for (const auto& rootGo : m_CurrentScene->GetRootGameObjects())
            renderGameObject(rootGo);
    }
    else
    {
        // シーン未設定の通知。Silent fail より問題発見が速い。
        OutputDebugStringA("[D3D12Renderer WARNING] Render: No scene set to render.\n");
    }

    // (7) RenderTarget → Present へ遷移（Present 前に戻すことを忘れがち）
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    commandList->ResourceBarrier(1, &barrier);

    // (8) コマンドを閉じて実行 → Present
    hr = commandList->Close();
    if (FAILED(hr)) { LogHRESULTError(hr, "Render: CommandList Close failed"); return; }

    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present。引数 (SyncInterval=1) はほぼ vsync ON 相当。
    // tearing ON の場合や可変リフレッシュ時はこのパラメータ/Flag を調整。
    hr = swapChain->Present(1, 0);
    if (FAILED(hr)) { LogHRESULTError(hr, "Render: Present failed"); return; }

    // (9) 本実装はここで「完全同期」。CPU/GPU 並行性は失うが確実性は高い。
    WaitForPreviousFrame();
    m_frameCount++;
}

//======================================================================================
// クリーンアップ
// ・GPU 完了待ち → 永続マップ解除 → OS ハンドル解放
// ・ComPtr はスコープ外で自動 Release（明示は不要）
//======================================================================================
void D3D12Renderer::Cleanup()
{
    if (device) // device が nullptr の場合、既に破棄済みの可能性
    {
        // 進行中の GPU コマンドがあれば待つ（安全策）
        WaitForPreviousFrame();
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
    // ComPtr は自動解放。順序依存がある場合は明示リセットも検討。
}

//======================================================================================
// GPU 完了待ち：
// ・queue.Signal(fence,value) → まだ未達なら SetEventOnCompletion でイベント待ち
// ・本実装は毎フレーム同期（安全だが並列性は低い）
//======================================================================================
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
        // GPU がフェンス値に到達するまで待機。
        // 本実装はブロックするが、将来はフレームを進行させて非同期化したい。
        WaitForSingleObject(fenceEvent, INFINITE);
    }
    fenceValue++; // 次のフレームで使う値へ進める

    // Present 後の最新バックバッファ番号を取得（トリプルバッファ等で変わる）
    frameIndex = swapChain->GetCurrentBackBufferIndex();
}

//======================================================================================
// 単一メッシュの即時ドロー（PSO/ルート/CBV/RT設定済み前提のヘルパ）
// ・外から使うときは前提条件（ルートシグネチャ、ヒープ、ビューポート等）に注意。
//======================================================================================
void D3D12Renderer::DrawMesh(MeshRendererComponent* meshRenderer)
{
    if (!meshRenderer) return;

    commandList->IASetVertexBuffers(0, 1, &meshRenderer->VertexBufferView);
    commandList->IASetIndexBuffer(&meshRenderer->IndexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->DrawIndexedInstanced(meshRenderer->IndexCount, 1, 0, 0, 0);
}