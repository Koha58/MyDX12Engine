#include "D3D12Renderer.h" // D3D12Rendererクラスのヘッダーファイルをインクルード

#include <stdexcept>   // std::runtime_errorを使用するためのヘッダー
#include <d3dcompiler.h> // HLSLシェーダーのコンパイルに使用するD3DCompile関数を提供
#include "d3dx12.h"    // D3D12構造体の便利なC++ラッパーを提供するMicrosoftのヘルパーライブラリ
#include <sstream>     // ワイド文字のデバッグメッセージを構築するためのstd::wstringstream
#include <comdef.h>    // _com_errorクラスを使用し、HRESULTエラーコードを人間が読めるメッセージに変換
#include <functional>  // std::functionを使用するためのヘッダー

#pragma comment(lib, "d3dcompiler.lib") // D3DCompilerライブラリをリンク
#pragma comment(lib, "d3d12.lib")    // Direct3D 12の主要ライブラリをリンク
#pragma comment(lib, "dxgi.lib")     // DXGIライブラリをリンク

using Microsoft::WRL::ComPtr; // ComPtrを簡潔に使うためのusingディレクティブ

namespace // このコンパイルユニット内でのみ使用されるヘルパー関数のための匿名名前空間
{
    // HRESULTエラーをVisual Studioのデバッグ出力ウィンドウにログするユーティリティ関数
    // _com_errorを使ってHRESULTを分かりやすいエラーメッセージに変換する
    void LogHRESULTError(HRESULT hr, const char* msg)
    {
        if (FAILED(hr)) // HRESULTが失敗コードであるかチェック
        {
            _com_error err(hr); // エラーの詳細を取得するため_com_errorオブジェクトを作成
            std::wstring errMsg = err.ErrorMessage(); // エラーメッセージをワイド文字列として取得

            std::wstringstream wss; // メッセージ構築のためワイド文字列ストリームを使用
            wss << L"[D3D12Renderer ERROR] " << msg << L" HRESULT=0x" << std::hex << hr << L" : " << errMsg << L"\n";
            OutputDebugStringW(wss.str().c_str()); // フォーマットされたエラーメッセージをデバッグ出力
        }
    }
}

// 最大描画オブジェクト数 (例として100個に設定)
const UINT MaxObjects = 100;

// D3D12Rendererクラスのコンストラクタ
D3D12Renderer::D3D12Renderer()
    : m_Width(0), m_Height(0), frameIndex(0), fenceValue(0), fenceEvent(nullptr), m_pCbvDataBegin(nullptr), m_frameCount(0), rtvDescriptorSize(0),
    m_ViewMatrix(DirectX::XMMatrixIdentity()),      // 単位行列で初期化
    m_ProjectionMatrix(DirectX::XMMatrixIdentity()), // 単位行列で初期化
    m_constantBufferData({}) // 構造体をゼロ初期化
{
    // メンバ変数を初期化
}

// D3D12Rendererクラスのデストラクタ
D3D12Renderer::~D3D12Renderer()
{
    Cleanup(); // リソースの解放処理を呼び出す
}

// レンダラーの初期化を行う関数
bool D3D12Renderer::Initialize(HWND hwnd, UINT width, UINT height)
{
    m_Width = width;   // ウィンドウの幅を設定
    m_Height = height; // ウィンドウの高さを設定

    // 各D3D12コンポーネントの作成を順に行う
    if (!CreateDevice()) return false;                 // デバイスの作成
    if (!CreateCommandQueue()) return false;           // コマンドキューの作成
    if (!CreateSwapChain(hwnd, width, height)) return false; // スワップチェインの作成
    if (!CreateRenderTargetViews()) return false;      // レンダーターゲットビューの作成
    if (!CreateDepthStencilBuffer(width, height)) return false; // デプスステンシルバッファの作成
    if (!CreateCommandAllocatorsAndList()) return false; // コマンドアロケータとコマンドリストの作成
    if (!CreatePipelineState()) return false;          // パイプラインステートオブジェクトの作成

    // --- 定数バッファの作成と初期データ設定 ---
    // 定数バッファのサイズをD3D12の要件に合わせてアライメントする
    const UINT alignedConstantBufferSize = (sizeof(SceneConstantBuffer) + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);

    // CBV（定数バッファビュー）ヒープのディスクリプタ数を最大オブジェクト数に設定
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = MaxObjects; // 各オブジェクトに対応するCBVを保持
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; // CBV、SRV、UAVタイプのヒープ
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // シェーダーからアクセス可能に設定

    // CBVディスクリプタヒープを作成
    HRESULT hr = device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateDescriptorHeap for CBV failed");
        return false;
    }

    // 定数バッファリソースのサイズを最大オブジェクト数とアライメントされた定数バッファサイズに基づいて設定
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD); // CPUからGPUへのアップロード用ヒープ
    D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedConstantBufferSize * MaxObjects); // 全オブジェクト分の定数バッファを格納

    // 定数バッファ用のコミット済みリソースを作成
    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, // GPUが読み取り可能な状態
        nullptr,
        IID_PPV_ARGS(&m_constantBuffer)
    );
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateCommittedResource for constant buffer failed");
        return false;
    }

    // 定数バッファをマップし、CPUからアクセス可能にする
    CD3DX12_RANGE readRange(0, 0); // CPUは読み込まないので範囲は0,0
    hr = m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "constantBuffer->Map failed");
        return false;
    }
    // CBVはレンダリングループでオブジェクトごとに動的に作成するため、ここでは何もしない

    // フェンスオブジェクトの作成
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateFence failed");
        return false;
    }
    fenceValue = 1; // フェンスの初期値
    // フェンスイベントを作成（GPUの処理完了を待つために使用）
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent == nullptr)
    {
        OutputDebugStringA("[D3D12Renderer ERROR] CreateEvent failed\n");
        return false;
    }

    return true; // 全ての初期化が成功
}

// D3D12デバイスの作成
bool D3D12Renderer::CreateDevice()
{
    HRESULT hr;
    ComPtr<IDXGIFactory4> factory;
    UINT createFactoryFlags = 0;

#ifdef _DEBUG // デバッグビルドの場合
    {
        ComPtr<ID3D12Debug> debugController;
        // デバッグレイヤーを有効にする
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG; // デバッグファクトリフラグを設定
        }
    }
#endif
    // DXGIファクトリを作成
    hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateDXGIFactory2 failed"); return false; }

    ComPtr<IDXGIAdapter1> hardwareAdapter;
    // ハードウェアアダプターを列挙し、D3D12デバイスが作成可能なアダプターを探す
    for (UINT adapterIndex = 0;; ++adapterIndex)
    {
        if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters1(adapterIndex, &hardwareAdapter)) break; // アダプターがもうない場合
        DXGI_ADAPTER_DESC1 desc;
        hardwareAdapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue; // ソフトウェアアダプターはスキップ
        // D3D12デバイスを作成
        hr = D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
        if (SUCCEEDED(hr)) break; // デバイス作成に成功したらループを抜ける
    }

    if (!device) { OutputDebugStringA("[D3D12Renderer ERROR] Failed to create D3D12 device\n"); return false; } // デバイスが作成できなかった場合
    return true;
}

// コマンドキューの作成
bool D3D12Renderer::CreateCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;     // 特になし
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;   // 直接実行タイプのコマンドキュー
    // コマンドキューを作成
    HRESULT hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommandQueue failed"); return false; }
    return true;
}

// スワップチェインの作成
bool D3D12Renderer::CreateSwapChain(HWND hwnd, UINT width, UINT height)
{
    ComPtr<IDXGIFactory4> factory;
    UINT createFactoryFlags = 0;

#ifdef _DEBUG
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG; // デバッグビルドの場合はデバッグファクトリフラグを設定
#endif
    // DXGIファクトリを作成
    HRESULT hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateDXGIFactory2 failed in CreateSwapChain"); return false; }

    // スワップチェインの記述を設定
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;                 // バッファ数を設定 (ダブルバッファリングなど)
    swapChainDesc.Width = width;                            // バッファの幅
    swapChainDesc.Height = height;                          // バッファの高さ
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;      // ピクセルフォーマット
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // レンダーターゲットとして使用
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // フリップディスクワッド方式
    swapChainDesc.SampleDesc.Count = 1;                     // マルチサンプリングなし
    swapChainDesc.SampleDesc.Quality = 0;

    ComPtr<IDXGISwapChain1> swapChain1;
    // ウィンドウハンドルに関連付けられたスワップチェインを作成
    hr = factory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1);
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateSwapChainForHwnd failed"); return false; }

    // IDXGISwapChain4インターフェースを取得
    hr = swapChain1.As(&swapChain);
    if (FAILED(hr)) { LogHRESULTError(hr, "swapChain1.As failed"); return false; }

    frameIndex = swapChain->GetCurrentBackBufferIndex(); // 現在のバックバッファのインデックスを取得
    return true;
}

// レンダーターゲットビュー（RTV）の作成
bool D3D12Renderer::CreateRenderTargetViews()
{
    // RTVヒープの記述を設定
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameCount;             // バッファの数だけRTVを持つ
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;   // RTVタイプのヒープ
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // シェーダーからアクセス不可

    // RTVディスクリプタヒープを作成
    HRESULT hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateDescriptorHeap for RTVs failed"); return false; }

    // RTVディスクリプタのサイズを取得 (ヒープ内のオフセット計算に使用)
    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // RTVヒープの先頭CPUディスクリプタハンドルを取得
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());

    // 各フレームバッファのRTVを作成
    for (UINT i = 0; i < FrameCount; i++)
    {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i])); // スワップチェインからバッファを取得
        if (FAILED(hr)) { LogHRESULTError(hr, "GetBuffer failed for render target"); return false; }
        // RTVを作成し、現在のrtvHandleの位置に配置
        device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, rtvDescriptorSize); // 次のRTVのためにハンドルをオフセット
    }
    return true;
}

// デプスステンシルバッファ（DSV）の作成
bool D3D12Renderer::CreateDepthStencilBuffer(UINT width, UINT height)
{
    HRESULT hr;
    // DSVヒープの記述を設定
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;                    // DSVは一つ
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;   // DSVタイプのヒープ
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // シェーダーからアクセス不可

    // DSVディスクリプタヒープを作成
    hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateDescriptorHeap for DSV failed"); return false; }

    // デプスステンシルバッファのリソースプロパティと記述を設定
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT); // デフォルトヒープ（GPUアクセス用）
    D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D32_FLOAT,                 // 32ビット浮動小数点フォーマット
        width, height, 1, 0, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL // デプスステンシルとして使用可能
    );

    // デプスステンシルバッファのクリア値
    D3D12_CLEAR_VALUE depthClearValue;
    depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthClearValue.DepthStencil.Depth = 1.0f;   // 深度を1.0にクリア
    depthClearValue.DepthStencil.Stencil = 0;    // ステンシルを0にクリア

    // デプスステンシルバッファ用のコミット済みリソースを作成
    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,      // デプス書き込み可能な状態
        &depthClearValue,
        IID_PPV_ARGS(&depthStencilBuffer)
    );
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommittedResource for DepthStencilBuffer failed"); return false; }

    // DSVの記述を設定
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    // DSVを作成
    device->CreateDepthStencilView(depthStencilBuffer.Get(), &dsvDesc, dsvHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
}

// コマンドアロケータとコマンドリストの作成
bool D3D12Renderer::CreateCommandAllocatorsAndList()
{
    HRESULT hr;
    // 各フレーム用にコマンドアロケータを作成
    for (UINT i = 0; i < FrameCount; i++)
    {
        hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[i]));
        if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommandAllocator failed"); return false; }
    }
    // コマンドリストを作成 (初期状態では最初のコマンドアロケータを使用)
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&commandList));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommandList failed"); return false; }

    commandList->Close(); // 初期状態ではコマンドリストを閉じておく (Recordコマンドを始める前にResetで開く)
    return true;
}

// パイプラインステートオブジェクト（PSO）とルートシグネチャの作成
bool D3D12Renderer::CreatePipelineState()
{
    // ルートシグネチャの作成
    // ディスクリプタレンジを定義 (CBVを1つ、シェーダーレジスタ0番にバインド)
    CD3DX12_DESCRIPTOR_RANGE ranges[1] = {}; // ここで配列全体をゼロ初期化
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // CBVを1つ、レジスタb0に

    // ルートパラメータを定義 (ディスクリプタテーブルを1つ)
    CD3DX12_ROOT_PARAMETER rootParameters[1] = {}; // ここで配列全体をゼロ初期化
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL); // 全てのシェーダーから見えるように

    // ルートシグネチャの記述を設定
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = _countof(rootParameters); // パラメータの数
    rootSignatureDesc.pParameters = rootParameters;             // パラメータの配列
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; // IAの入力レイアウトを許可

    ComPtr<ID3DBlob> signatureBlob; // シリアライズされたルートシグネチャを格納
    ComPtr<ID3DBlob> errorBlob;     // エラーメッセージを格納

    // ルートシグネチャをシリアライズ (バイナリ形式に変換)
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer()); // エラーメッセージがあればデバッグ出力
        LogHRESULTError(hr, "D3D12SerializeRootSignature failed");
        return false;
    }

    // シリアライズされたデータからルートシグネチャを作成
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateRootSignature failed"); return false; }

    // シェーダーソースコードを定義
    const char* vsSource = R"(
cbuffer cb0 : register(b0) // 定数バッファ0
{
    row_major float4x4 g_mvp; // モデルビュープロジェクション行列
};
struct VSInput
{
    float3 pos : POSITION; // 位置
    float4 color : COLOR;  // 色
};
struct PSInput
{
    float4 pos : SV_POSITION; // 頂点シェーダーからの出力位置
    float4 color : COLOR;     // 頂点シェーダーからの出力色
};
PSInput main(VSInput input)
{
    PSInput output;
    // 頂点位置をMVP行列で変換
    output.pos = mul(float4(input.pos, 1.0f), g_mvp);
    output.color = input.color; // 色をそのまま渡す
    return output;
}
)";
    const char* psSource = R"(
struct PSInput
{
    float4 pos : SV_POSITION; // ピクセルシェーダーへの入力位置
    float4 color : COLOR;     // ピクセルシェーダーへの入力色
};
float4 main(PSInput input) : SV_TARGET // レンダーターゲットへの出力
{
    return input.color; // 入力色をそのまま出力
}
)";
    ComPtr<ID3DBlob> vertexShader; // 頂点シェーダーのコンパイル結果
    ComPtr<ID3DBlob> pixelShader;  // ピクセルシェーダーのコンパイル結果
    ComPtr<ID3DBlob> compileErrors; // シェーダーコンパイルエラー

    // 頂点シェーダーをコンパイル
    hr = D3DCompile(vsSource, strlen(vsSource), nullptr, nullptr, nullptr, "main", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vertexShader, &compileErrors);
    if (FAILED(hr)) {
        if (compileErrors) OutputDebugStringA((char*)compileErrors->GetBufferPointer());
        LogHRESULTError(hr, "Vertex shader compilation failed"); return false;
    }
    // ピクセルシェーダーをコンパイル
    hr = D3DCompile(psSource, strlen(psSource), nullptr, nullptr, nullptr, "main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &pixelShader, &compileErrors);
    if (FAILED(hr)) {
        if (compileErrors) OutputDebugStringA((char*)compileErrors->GetBufferPointer());
        LogHRESULTError(hr, "Pixel shader compilation failed"); return false;
    }

    // 入力レイアウト記述 (頂点データフォーマット)
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // 位置 (float3)
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // 色 (float4)
    };

    // PSO（パイプラインステートオブジェクト）の設定
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) }; // 入力レイアウト
    psoDesc.pRootSignature = rootSignature.Get();                              // 使用するルートシグネチャ
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());                  // 頂点シェーダー
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());                   // ピクセルシェーダー
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);          // デフォルトのラスタライザーステート
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);                    // デフォルトのブレンドステート
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);     // デフォルトのデプスステンシルステート
    psoDesc.DepthStencilState.DepthEnable = TRUE;                              // 深度テストを有効にする
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;     // 深度バッファへの書き込みを有効にする
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;          // 深度比較関数をLessに設定
    psoDesc.DepthStencilState.StencilEnable = FALSE;                           // ステンシルテストを無効にする
    psoDesc.SampleMask = UINT_MAX;                                             // サンプルマスクを全て有効に
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;    // プリミティブタイプは三角形リスト
    psoDesc.NumRenderTargets = 1;                                              // レンダーターゲットは1つ
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;                        // レンダーターゲットのフォーマット
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;                                 // デプスステンシルビューのフォーマット
    psoDesc.SampleDesc.Count = 1;                                              // マルチサンプリングなし
    psoDesc.SampleDesc.Quality = 0;

    // グラフィックスパイプラインステートを作成
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateGraphicsPipelineState failed"); return false; }
    return true;
}

// メッシュレンダラーコンポーネントのリソースを作成する関数
bool D3D12Renderer::CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> meshRenderer)
{
    // 無効なメッシュレンダラーコンポーネントまたは空のメッシュデータの場合は警告を出して終了
    if (!meshRenderer || meshRenderer->m_MeshData.Indices.empty())
    {
        OutputDebugStringA("[D3D12Renderer WARNING] CreateMeshRendererResources: Invalid MeshRendererComponent or empty mesh data.\n");
        return false;
    }
    HRESULT hr;
    // アップロードヒープのプロパティを設定 (CPUから書き込み、GPUから読み取り)
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // 頂点バッファのサイズを計算
    const UINT vertexBufferSize = (UINT)meshRenderer->m_MeshData.Vertices.size() * sizeof(Vertex);
    // 頂点バッファリソースの記述を設定
    D3D12_RESOURCE_DESC vertexResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

    // 頂点バッファ用のコミット済みリソースを作成
    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &vertexResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, // GPUが読み取り可能な状態
        nullptr,
        IID_PPV_ARGS(&meshRenderer->VertexBuffer) // メッシュレンダラーに頂点バッファを格納
    );
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommittedResource for vertex buffer failed in CreateMeshRendererResources"); return false; }

    UINT8* pVertexDataBegin = nullptr;
    CD3DX12_RANGE readRange(0, 0); // CPUは読み込まないので範囲は0,0

    // 頂点バッファをマップし、CPUからアクセス可能にする
    hr = meshRenderer->VertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    if (FAILED(hr)) { LogHRESULTError(hr, "Vertex buffer Map failed in CreateMeshRendererResources"); return false; }
    // 頂点データをバッファにコピー
    memcpy(pVertexDataBegin, meshRenderer->m_MeshData.Vertices.data(), vertexBufferSize);
    meshRenderer->VertexBuffer->Unmap(0, nullptr); // バッファのマップを解除

    // 頂点バッファビュー (VBV) を設定
    meshRenderer->VertexBufferView.BufferLocation = meshRenderer->VertexBuffer->GetGPUVirtualAddress(); // GPU仮想アドレス
    meshRenderer->VertexBufferView.StrideInBytes = sizeof(Vertex);                                     // 1頂点あたりのバイト数
    meshRenderer->VertexBufferView.SizeInBytes = vertexBufferSize;                                     // バッファ全体のサイズ

    // インデックスバッファのサイズを計算
    const UINT indexBufferSize = (UINT)meshRenderer->m_MeshData.Indices.size() * sizeof(unsigned int);
    // インデックスバッファリソースの記述を設定
    D3D12_RESOURCE_DESC indexResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

    // インデックスバッファ用のコミット済みリソースを作成
    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &indexResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, // GPUが読み取り可能な状態
        nullptr,
        IID_PPV_ARGS(&meshRenderer->IndexBuffer) // メッシュレンダラーにインデックスバッファを格納
    );
    if (FAILED(hr)) { LogHRESULTError(hr, "CreateCommittedResource for index buffer failed in CreateMeshRendererResources"); return false; }

    UINT8* pIndexDataBegin = nullptr;
    // インデックスバッファをマップし、CPUからアクセス可能にする
    hr = meshRenderer->IndexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin));
    if (FAILED(hr)) { LogHRESULTError(hr, "Index buffer Map failed in CreateMeshRendererResources"); return false; }
    // インデックスデータをバッファにコピー
    memcpy(pIndexDataBegin, meshRenderer->m_MeshData.Indices.data(), indexBufferSize);
    meshRenderer->IndexBuffer->Unmap(0, nullptr); // バッファのマップを解除

    // インデックスバッファビュー (IBV) を設定
    meshRenderer->IndexBufferView.BufferLocation = meshRenderer->IndexBuffer->GetGPUVirtualAddress(); // GPU仮想アドレス
    meshRenderer->IndexBufferView.Format = DXGI_FORMAT_R32_UINT;                                     // インデックスのフォーマット (32ビット符号なし整数)
    meshRenderer->IndexBufferView.SizeInBytes = indexBufferSize;                                     // バッファ全体のサイズ

    // インデックスカウントも設定
    meshRenderer->IndexCount = (UINT)meshRenderer->m_MeshData.Indices.size();

    return true;
}

// レンダリング処理を実行する関数
void D3D12Renderer::Render()
{
    HRESULT hr;

    // 現在のフレームのコマンドアロケータをリセット (再利用可能にする)
    hr = commandAllocators[frameIndex]->Reset();
    if (FAILED(hr)) { LogHRESULTError(hr, "Render: CommandAllocator Reset failed"); return; }
    // コマンドリストをリセット (新しいコマンドを記録できるようにする)
    hr = commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get());
    if (FAILED(hr)) { LogHRESULTError(hr, "Render: CommandList Reset failed"); return; }

    // カメラのビュー行列とプロジェクション行列を計算
    using namespace DirectX;
    // ビュー行列: カメラの位置(0,0,-5)から原点(0,0,0)を見て、上方向は(0,1,0)
    XMMATRIX viewMatrix = XMMatrixLookAtLH(XMVectorSet(0.0f, 0.0f, -5.0f, 1.0f), XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));
    // プロジェクション行列: 視野角45度、アスペクト比、ニアクリップ面0.1、ファークリップ面100.0
    XMMATRIX projMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), (FLOAT)m_Width / (FLOAT)m_Height, 0.1f, 100.0f);

    // 現在のレンダーターゲットとデプスステンシルビューのCPUディスクリプタハンドルを取得
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());

    // リソースバリアを設定し、レンダーターゲットをPresent状態からRender Target状態に遷移させる
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    commandList->ResourceBarrier(1, &barrier);

    const float clearColor[] = { 0.2f, 0.2f, 0.4f, 1.0f }; // クリア色 (濃い青)
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle); // レンダーターゲットとデプスステンシルを設定
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr); // 深度バッファをクリア
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr); // レンダーターゲットをクリア

    commandList->SetGraphicsRootSignature(rootSignature.Get()); // ルートシグネチャを設定
    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() };      // CBVヒープを設定
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // ビューポートを設定
    D3D12_VIEWPORT viewport;
    viewport.TopLeftX = 0.0f; viewport.TopLeftY = 0.0f;
    viewport.Width = (float)m_Width; viewport.Height = (float)m_Height;
    viewport.MinDepth = 0.0f; viewport.MaxDepth = 1.0f;
    commandList->RSSetViewports(1, &viewport);

    // シザー矩形を設定
    D3D12_RECT scissorRect;
    scissorRect.left = 0; scissorRect.top = 0;
    scissorRect.right = m_Width; scissorRect.bottom = m_Height;
    commandList->RSSetScissorRects(1, &scissorRect);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // プリミティブトポロジを三角形リストに設定

    // --- シーンのGameObjectを走査して描画 ---
    if (m_CurrentScene) // 現在シーンが設定されている場合
    {
        int objectIndex = 0; // オブジェクトのインデックス
        // 定数バッファのアライメント済みサイズ
        const UINT alignedConstantBufferSize = (sizeof(SceneConstantBuffer) + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
        // CBVディスクリプタのサイズ
        const UINT cbvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // GameObjectを再帰的にレンダリングするラムダ関数
        std::function<void(std::shared_ptr<GameObject>)> renderGameObject =
            [&](std::shared_ptr<GameObject> gameObject)
            {
                if (!gameObject || objectIndex >= MaxObjects) return; // 無効なオブジェクトか、最大オブジェクト数を超えた場合はスキップ

                std::shared_ptr<MeshRendererComponent> meshRenderer = gameObject->GetComponent<MeshRendererComponent>();

                // メッシュレンダラーがあり、頂点/インデックスバッファが有効で、インデックスが0より大きい場合
                if (meshRenderer && meshRenderer->VertexBuffer && meshRenderer->IndexBuffer && meshRenderer->IndexCount > 0)
                {
                    // 1. オブジェクトのワールド行列を計算し、定数バッファの専用領域に書き込む
                    DirectX::XMMATRIX world = gameObject->Transform->GetWorldMatrix(); // オブジェクトのワールド行列を取得
                    DirectX::XMMATRIX mvp = world * viewMatrix * projMatrix;           // MVP行列を計算

                    SceneConstantBuffer constantBufferData;
                    // MVP行列を転置して定数バッファデータに格納
                    DirectX::XMStoreFloat4x4(&constantBufferData.mvp, DirectX::XMMatrixTranspose(mvp));

                    // 定数バッファのCPUポインタが有効な場合、正しいメモリ位置にデータをコピー
                    if (m_pCbvDataBegin) {
                        memcpy(m_pCbvDataBegin + objectIndex * alignedConstantBufferSize, &constantBufferData, sizeof(constantBufferData));
                    }

                    // 2. オブジェクトごとのCBVディスクリプタを作成
                    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                    // 定数バッファ内のこのオブジェクトのデータ開始位置
                    cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress() + objectIndex * alignedConstantBufferSize;
                    cbvDesc.SizeInBytes = alignedConstantBufferSize; // 定数バッファのサイズ

                    // CPUディスクリプタハンドルを取得し、オフセットした位置にCBVを作成
                    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuCbvHandle(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
                    cpuCbvHandle.Offset(objectIndex, cbvDescriptorSize);
                    device->CreateConstantBufferView(&cbvDesc, cpuCbvHandle);

                    // 3. ルートパラメータにCBVを設定
                    // GPUディスクリプタハンドルも同様にオフセット
                    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuCbvHandle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
                    gpuCbvHandle.Offset(objectIndex, cbvDescriptorSize);

                    commandList->SetGraphicsRootDescriptorTable(0, gpuCbvHandle); // ルートパラメータ0番にCBVディスクリプタテーブルを設定

                    // 4. 頂点/インデックスバッファを設定し、描画
                    commandList->IASetVertexBuffers(0, 1, &meshRenderer->VertexBufferView); // 頂点バッファを設定
                    commandList->IASetIndexBuffer(&meshRenderer->IndexBufferView);         // インデックスバッファを設定
                    commandList->DrawIndexedInstanced(meshRenderer->IndexCount, 1, 0, 0, 0); // 描画コール (インデックス付きインスタンス描画)

                    objectIndex++; // 次のオブジェクトのためにインデックスをインクリメント
                }

                // 子オブジェクトを再帰的に描画
                for (const auto& child : gameObject->GetChildren()) {
                    renderGameObject(child);
                }
            };

        // シーンのルートGameObjectから描画を開始
        for (const auto& rootGo : m_CurrentScene->GetRootGameObjects()) {
            renderGameObject(rootGo);
        }
    }
    else // シーンが設定されていない場合
    {
        OutputDebugStringA("[D3D12Renderer WARNING] Render: No scene set to render.\n");
    }
    // --- シーンのGameObjectを走査して描画 ここまで ---

    // リソースバリアを設定し、レンダーターゲットをRender Target状態からPresent状態に遷移させる
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    commandList->ResourceBarrier(1, &barrier);

    hr = commandList->Close(); // コマンドリストを閉じる
    if (FAILED(hr)) { LogHRESULTError(hr, "Render: CommandList Close failed"); return; }
    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists); // コマンドリストを実行

    hr = swapChain->Present(1, 0); // スワップチェインをプレゼンテーション (画面に表示)
    if (FAILED(hr)) { LogHRESULTError(hr, "Render: Present failed"); return; }

    WaitForPreviousFrame(); // 前のフレームのGPU処理が完了するのを待機
    m_frameCount++; // フレームカウントをインクリメント
}

// リソースのクリーンアップ処理
void D3D12Renderer::Cleanup()
{
    if (device) // デバイスが有効な場合
    {
        WaitForPreviousFrame(); // GPUコマンドの完了を待機
    }
    if (fenceEvent) {
        CloseHandle(fenceEvent); // フェンスイベントハンドルを閉じる
        fenceEvent = nullptr;
    }
    if (m_constantBuffer && m_pCbvDataBegin) {
        m_constantBuffer->Unmap(0, nullptr); // 定数バッファのマップを解除
        m_pCbvDataBegin = nullptr;
    }
    // ComPtrは自動的にリリースされるため、明示的なリリースは不要
}

// 前のフレームのGPU処理が完了するのを待機する関数
void D3D12Renderer::WaitForPreviousFrame()
{
    // コマンドキューにフェンスシグナルをキューイング (現在のコマンド実行後にフェンス値を設定する)
    HRESULT hr = commandQueue->Signal(fence.Get(), fenceValue);
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "WaitForPreviousFrame: commandQueue->Signal failed");
        throw std::runtime_error("commandQueue->Signal failed"); // 致命的なエラーとして例外をスロー
    }

    // フェンスの完了値が現在のフェンス値よりも小さい場合 (GPUがまだそのフェンスに到達していない場合)
    if (fence->GetCompletedValue() < fenceValue)
    {
        // フェンスが現在のフェンス値に到達したときにイベントを発生させるように設定
        hr = fence->SetEventOnCompletion(fenceValue, fenceEvent);
        if (FAILED(hr))
        {
            LogHRESULTError(hr, "WaitForPreviousFrame: fence->SetEventOnCompletion failed");
            throw std::runtime_error("fence->SetEventOnCompletion failed"); // 致命的なエラーとして例外をスロー
        }
        // イベントがシグナルされるまで待機 (GPUの処理が完了するまでブロック)
        WaitForSingleObject(fenceEvent, INFINITE);
    }
    fenceValue++; // 次のフレームのためにフェンス値をインクリメント
    frameIndex = swapChain->GetCurrentBackBufferIndex(); // 次に表示されるバックバッファのインデックスを更新
}

void D3D12Renderer::DrawMesh(MeshRendererComponent* meshRenderer)
{
    if (!meshRenderer) return;

    m_CommandList->IASetVertexBuffers(0, 1, &meshRenderer->VertexBufferView);
    m_CommandList->IASetIndexBuffer(&meshRenderer->IndexBufferView);
    m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_CommandList->DrawIndexedInstanced(meshRenderer->IndexCount, 1, 0, 0, 0);
}
