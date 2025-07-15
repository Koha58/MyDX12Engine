#include "D3D12Renderer.h" // ヘッダーファイルをインクルードします。

#include <stdexcept>        // std::runtime_error のために使用され、致命的なエラーの際に使用されます。
#include <d3dcompiler.h>    // HLSLシェーダーをランタイムでコンパイルするためのD3DCompile関数を提供します。
#include "d3dx12.h"         // MicrosoftのD3D12ヘルパーライブラリで、D3D12構造体に対する便利なC++ラッパーを提供します。
#include <sstream>          // ワイド文字のデバッグメッセージを構築するためのstd::wstringstream。
#include <comdef.h>         // _com_error のために使用され、HRESULTエラーコードを人間が読めるメッセージに変換するのに役立ちます。
#include <functional>       // std::function のために追加

#pragma comment(lib, "d3dcompiler.lib") // D3DCompilerライブラリをプロジェクトにリンクします。
// ★追加: Direct3D 12 の主要ライブラリ
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


using Microsoft::WRL::ComPtr; // ComPtr を使用するための便利な using ディレクティブ。

namespace // このコンパイルユニットにローカルなヘルパー関数のための匿名名前空間。
{
    // HRESULTエラーをVisual Studioのデバッグ出力ウィンドウにログするためのユーティリティ関数。
    // _com_error を使用してHRESULTを分かりやすいエラーメッセージに変換します。
    // @param hr: DirectX API呼び出しによって返されたHRESULT (エラーコード)。
    // @param msg: エラーが発生した場所を説明するカスタム文字列。
    void LogHRESULTError(HRESULT hr, const char* msg)
    {
        if (FAILED(hr)) // HRESULTが失敗を示しているかチェック。
        {
            _com_error err(hr); // エラー詳細を取得するために_com_errorオブジェクトを作成。
            std::wstring errMsg = err.ErrorMessage(); // エラーメッセージをワイド文字列として取得。

            std::wstringstream wss; // メッセージ構築のためにワイド文字列ストリームを使用。
            wss << L"[D3D12Renderer ERROR] " << msg << L" HRESULT=0x" << std::hex << hr << L" : " << errMsg << L"\n";
            OutputDebugStringW(wss.str().c_str()); // フォーマットされたエラーメッセージを出力。
        }
    }
}

D3D12Renderer::D3D12Renderer()
    : m_Width(0), m_Height(0), frameIndex(0), fenceValue(0), fenceEvent(nullptr), m_pCbvDataBegin(nullptr), m_frameCount(0)
{
    // コンストラクタでメンバを初期化
    // ComPtrは自動的にnullptrで初期化される
    // DirectX::XMMATRIX はデフォルトコンストラクタを持たないため、初期化リストで設定しない
}

D3D12Renderer::~D3D12Renderer()
{
    // Cleanup() で適切なクリーンアップが処理される
}

// D3D12Renderer を初期化します。この関数は、一連のプライベートメソッドを呼び出し、
// レンダリングパイプライン全体を論理的な順序でセットアップします。
// @param hwnd: アプリケーションウィンドウのハンドル。
// @param width: クライアント領域の幅。
// @param height: クライアント領域の高さ。
// @return: 成功した場合は `true`、失敗した場合は `false`。
bool D3D12Renderer::Initialize(HWND hwnd, UINT width, UINT height)
{
    // ウィンドウサイズをクラスメンバーに保存
    m_Width = width;
    m_Height = height;

    // ステップ1: Direct3D 12デバイスを作成します。これはGPUへの主要なインターフェースです。
    if (!CreateDevice()) return false;
    // ステップ2: コマンドキューを作成します。GPU実行のためにコマンドがここに送信されます。
    if (!CreateCommandQueue()) return false;
    // ステップ3: スワップチェインを作成します。レンダリングされたフレームを画面に表示する処理を管理します。
    if (!CreateSwapChain(hwnd, width, height)) return false;
    // ステップ4: レンダーターゲットビューを作成します。これにより、スワップチェインのバックバッファにレンダリングできます。
    if (!CreateRenderTargetViews()) return false;
    // ★追加: 深度バッファの作成
    if (!CreateDepthStencilBuffer(width, height)) return false;

    // ステップ5: コマンドアロケーターとコマンドリストを作成します。GPUコマンドの記録に必要です。
    if (!CreateCommandAllocatorsAndList()) return false;
    // ステップ6: パイプラインステートオブジェクト (PSO) とルートシグネチャを作成します。レンダリング方法を定義します。
    if (!CreatePipelineState()) return false;

    // CreateGeometryBuffers() は削除され、CreateMeshRendererResources に置き換えられました。

    // --- 定数バッファの作成と初期データ設定 ---
    // CBV ヒープディスクリプタの記述
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = 1; // 1つの定数バッファ
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // シェーダーからアクセス可能

    HRESULT hr = device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateDescriptorHeap for CBV failed");
        return false;
    }

    // 定数バッファリソースの記述
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // 定数バッファのサイズを256バイトの倍数にアラインする
    // D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT は256です。
    // (A + N - 1) & ~(N - 1) は、AをNの倍数に切り上げる一般的な方法です。
    const UINT alignedConstantBufferSize = (sizeof(SceneConstantBuffer) + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
    D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedConstantBufferSize);

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_constantBuffer)
    );
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateCommittedResource for constant buffer failed");
        return false;
    }

    // 定数バッファをマップしてCPUからアクセス可能にする
    UINT8* pCbvDataBegin = nullptr;
    CD3DX12_RANGE readRange(0, 0); // CPUは読み込まない
    hr = m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pCbvDataBegin));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "constantBuffer->Map failed");
        return false;
    }
    m_pCbvDataBegin = pCbvDataBegin;

    // CBV を作成
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
    // CBVディスクリプタのサイズもアライン済みのサイズを使用する
    cbvDesc.SizeInBytes = alignedConstantBufferSize;
    device->CreateConstantBufferView(&cbvDesc, m_cbvHeap->GetCPUDescriptorHandleForHeapStart());

    // 初期MVP行列を単位行列に設定 (後で更新)
    DirectX::XMStoreFloat4x4(&m_constantBufferData.mvp, DirectX::XMMatrixIdentity());
    memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
    // --- 定数バッファの作成と初期データ設定ここまで ---

    // ステップ7: CPU-GPU同期のためのフェンスオブジェクトを作成します。
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateFence failed");
        return false;
    }
    fenceValue = 1;
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent == nullptr)
    {
        OutputDebugStringA("[D3D12Renderer ERROR] CreateEvent failed\n");
        return false;
    }

    return true; // 初期化成功。
}

// Direct3D 12デバイスを作成し、適切なハードウェアアダプタを選択します。
bool D3D12Renderer::CreateDevice()
{
    HRESULT hr;
    ComPtr<IDXGIFactory4> factory;
    UINT createFactoryFlags = 0;

#ifdef _DEBUG
    {
        ComPtr<ID3D12Debug> debugController;
        // D3D12GetDebugInterface は d3d12.lib に含まれる
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    // CreateDXGIFactory2 は dxgi.lib に含まれる
    hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateDXGIFactory2 failed");
        return false;
    }

    ComPtr<IDXGIAdapter1> hardwareAdapter;
    for (UINT adapterIndex = 0;; ++adapterIndex)
    {
        if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters1(adapterIndex, &hardwareAdapter))
            break;

        DXGI_ADAPTER_DESC1 desc;
        hardwareAdapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        // D3D12CreateDevice は d3d12.lib に含まれる
        hr = D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
        if (SUCCEEDED(hr))
            break;
    }

    if (!device)
    {
        OutputDebugStringA("[D3D12Renderer ERROR] Failed to create D3D12 device\n");
        return false;
    }

    return true;
}

// Direct3D 12コマンドキューを作成します。
bool D3D12Renderer::CreateCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    HRESULT hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateCommandQueue failed");
        return false;
    }

    return true;
}

bool D3D12Renderer::CreateSwapChain(HWND hwnd, UINT width, UINT height)
{
    ComPtr<IDXGIFactory4> factory;
    UINT createFactoryFlags = 0;

#ifdef _DEBUG
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    HRESULT hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateDXGIFactory2 failed in CreateSwapChain");
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;

    ComPtr<IDXGISwapChain1> swapChain1;
    hr = factory->CreateSwapChainForHwnd(
        commandQueue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1);
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateSwapChainForHwnd failed");
        return false;
    }

    hr = swapChain1.As(&swapChain);
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "swapChain1.As failed");
        return false;
    }

    frameIndex = swapChain->GetCurrentBackBufferIndex();

    return true;
}

// レンダーターゲットビュー (RTV) のためのディスクリプタヒープを作成し、
// スワップチェイン内の各バックバッファ用にRTVを生成します。
bool D3D12Renderer::CreateRenderTargetViews()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateDescriptorHeap for RTVs failed");
        return false;
    }

    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());

    for (UINT i = 0; i < FrameCount; i++)
    {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
        if (FAILED(hr))
        {
            LogHRESULTError(hr, "GetBuffer failed for render target");
            return false;
        }

        device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, rtvDescriptorSize);
    }

    return true;
}

// ★追加関数: 深度バッファの作成
bool D3D12Renderer::CreateDepthStencilBuffer(UINT width, UINT height)
{
    HRESULT hr;

    // 1. DSVディスクリプタヒープの作成
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1; // 1つの深度バッファのみ
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // シェーダーから見えない
    hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateDescriptorHeap for DSV failed");
        return false;
    }

    // 2. 深度ステンシルバッファリソースの作成
    // リソースのプロパティ (GPUプライベートメモリに配置)
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    // リソースの記述 (テクスチャとして定義)
    D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D32_FLOAT, // 深度バッファのフォーマット。これはPSOのDSVFormatと一致させる
        width, height, 1, 0, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL // 深度ステンシルとして使用することを許可
    );

    // 深度バッファのクリア値
    D3D12_CLEAR_VALUE depthClearValue;
    depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthClearValue.DepthStencil.Depth = 1.0f; // 深度値を1.0f (最も遠い) でクリア
    depthClearValue.DepthStencil.Stencil = 0;

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, // 初期状態は深度書き込み可能
        &depthClearValue, // リソース作成時にクリア値を指定
        IID_PPV_ARGS(&depthStencilBuffer)
    );
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateCommittedResource for DepthStencilBuffer failed");
        return false;
    }

    // 3. 深度ステンシルビュー (DSV) の作成
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // DSVのフォーマットも一致させる
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    device->CreateDepthStencilView(depthStencilBuffer.Get(), &dsvDesc, dsvHeap->GetCPUDescriptorHandleForHeapStart());

    return true;
}

bool D3D12Renderer::CreateCommandAllocatorsAndList()
{
    HRESULT hr;

    for (UINT i = 0; i < FrameCount; i++)
    {
        hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[i]));
        if (FAILED(hr))
        {
            LogHRESULTError(hr, "CreateCommandAllocator failed");
            return false;
        }
    }

    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&commandList));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateCommandList failed");
        return false;
    }

    commandList->Close();
    return true;
}


bool D3D12Renderer::CreatePipelineState()
{
    // --- ルートシグネチャの作成 ---
    // ルートシグネチャをディスクリプタテーブルを使用するように変更
    CD3DX12_DESCRIPTOR_RANGE ranges[1];
    // ranges[0]: CBV (定数バッファビュー) をレジスタ b0 に1つ定義
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

    CD3DX12_ROOT_PARAMETER rootParameters[1];
    // rootParameters[0]: ディスクリプタテーブルとして設定。上記の ranges[0] を含む。
    // シェーダーからは、ディスクリプタテーブル内のディスクリプタを参照してリソースにアクセスする。
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    // D3D12SerializeRootSignature は d3dcompiler.lib に含まれる
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        LogHRESULTError(hr, "D3D12SerializeRootSignature failed");
        return false;
    }

    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateRootSignature failed");
        return false;
    }

    // --- シェーダーソース (HLSL文字列) ---
    const char* vsSource = R"(
cbuffer cb0 : register(b0)
{
    row_major float4x4 g_mvp;
};

struct VSInput
{
    float3 pos : POSITION;
    float4 color : COLOR;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};

PSInput main(VSInput input)
{
    PSInput output;
    output.pos = mul(float4(input.pos, 1.0f), g_mvp);
    output.color = input.color;
    return output;
}
)";

    const char* psSource = R"(
struct PSInput
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};

float4 main(PSInput input) : SV_TARGET
{
    return input.color;
}
)";

    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    ComPtr<ID3DBlob> compileErrors;

    hr = D3DCompile(vsSource, strlen(vsSource), nullptr, nullptr, nullptr, "main", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vertexShader, &compileErrors);
    if (FAILED(hr))
    {
        if (compileErrors)
            OutputDebugStringA((char*)compileErrors->GetBufferPointer());
        LogHRESULTError(hr, "Vertex shader compilation failed");
        return false;
    }

    hr = D3DCompile(psSource, strlen(psSource), nullptr, nullptr, nullptr, "main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &pixelShader, &compileErrors);
    if (FAILED(hr))
    {
        if (compileErrors)
            OutputDebugStringA((char*)compileErrors->GetBufferPointer());
        LogHRESULTError(hr, "Pixel shader compilation failed");
        return false;
    }

    // --- 入力レイアウト記述 ---
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // --- パイプラインステートオブジェクト (PSO) 設定 ---
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    // ★変更: 深度ステンシルステートを有効にする
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // デフォルト設定を使用
    psoDesc.DepthStencilState.DepthEnable = TRUE; // 深度テストを有効に
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // 深度バッファへの書き込みを有効に
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS; // 手前のオブジェクトを描画する
    psoDesc.DepthStencilState.StencilEnable = FALSE; // ステンシルは無効

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    // ★追加: 深度ステンシルフォーマット
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // または DXGI_FORMAT_D24_UNORM_S8_UINT など

    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateGraphicsPipelineState failed");
        return false;
    }

    return true;
}

// D3D12Renderer::CreateMeshRendererResources の実装
bool D3D12Renderer::CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> meshRenderer)
{
    // meshRenderer->m_MeshData はフレンドクラス宣言によりアクセス可能
    if (!meshRenderer || meshRenderer->m_MeshData.Indices.empty())
    {
        OutputDebugStringA("[D3D12Renderer WARNING] CreateMeshRendererResources: Invalid MeshRendererComponent or empty mesh data.\n");
        return false;
    }

    HRESULT hr;
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD); // アップロードヒープを使用

    // 頂点バッファの作成
    const UINT vertexBufferSize = (UINT)meshRenderer->m_MeshData.Vertices.size() * sizeof(Vertex); // Mesh.hでVertexを定義している
    D3D12_RESOURCE_DESC vertexResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &vertexResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&meshRenderer->VertexBuffer)
    );
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateCommittedResource for vertex buffer failed in CreateMeshRendererResources");
        return false;
    }

    // 頂点データをマップしてコピー
    UINT8* pVertexDataBegin = nullptr;
    CD3DX12_RANGE readRange(0, 0);
    hr = meshRenderer->VertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "Vertex buffer Map failed in CreateMeshRendererResources");
        return false;
    }
    memcpy(pVertexDataBegin, meshRenderer->m_MeshData.Vertices.data(), vertexBufferSize);
    meshRenderer->VertexBuffer->Unmap(0, nullptr);

    // 頂点バッファビューの設定
    meshRenderer->VertexBufferView.BufferLocation = meshRenderer->VertexBuffer->GetGPUVirtualAddress();
    meshRenderer->VertexBufferView.StrideInBytes = sizeof(Vertex);
    meshRenderer->VertexBufferView.SizeInBytes = vertexBufferSize;

    // インデックスバッファの作成
    const UINT indexBufferSize = (UINT)meshRenderer->m_MeshData.Indices.size() * sizeof(unsigned int); // unsigned int を使用
    D3D12_RESOURCE_DESC indexResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &indexResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&meshRenderer->IndexBuffer)
    );
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "CreateCommittedResource for index buffer failed in CreateMeshRendererResources");
        return false;
    }

    // インデックスデータをマップしてコピー
    UINT8* pIndexDataBegin = nullptr;
    hr = meshRenderer->IndexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin));
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "Index buffer Map failed in CreateMeshRendererResources");
        return false;
    }
    memcpy(pIndexDataBegin, meshRenderer->m_MeshData.Indices.data(), indexBufferSize);
    meshRenderer->IndexBuffer->Unmap(0, nullptr);

    // インデックスバッファビューの設定
    meshRenderer->IndexBufferView.BufferLocation = meshRenderer->IndexBuffer->GetGPUVirtualAddress();
    meshRenderer->IndexBufferView.Format = DXGI_FORMAT_R32_UINT; // unsigned int のため DXGI_FORMAT_R32_UINT
    meshRenderer->IndexBufferView.SizeInBytes = indexBufferSize;

    // IndexCount は MeshRendererComponent::SetMesh で設定されるのでここでは不要
    // meshRenderer->IndexCount = (UINT)meshRenderer->m_MeshData.Indices.size();

    return true;
}


// 記録されたコマンドをGPUで実行し、結果を表示します。
void D3D12Renderer::Render()
{
    HRESULT hr;

    hr = commandAllocators[frameIndex]->Reset();
    if (FAILED(hr)) { LogHRESULTError(hr, "Render: CommandAllocator Reset failed"); return; }
    hr = commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get());
    if (FAILED(hr)) { LogHRESULTError(hr, "Render: CommandList Reset failed"); return; }

    // --- MVP行列の計算と定数バッファへの書き込み ---
    using namespace DirectX;

    // カメラのビューとプロジェクション行列を先に計算しておく (またはCameraComponentから取得)
    XMVECTOR eye = XMVectorSet(0.0f, 0.0f, -5.0f, 1.0f); // カメラを-Z軸に少し引く
    XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
    m_ViewMatrix = XMMatrixLookAtLH(eye, at, up);

    m_ProjectionMatrix = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(45.0f),
        (FLOAT)m_Width / (FLOAT)m_Height,
        0.1f,
        100.0f
    );

    // レンダリング開始前処理
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);

    // ★追加: DSVハンドルを取得
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());

    // リソースバリア (レンダーターゲットの状態遷移はそのまま)
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    commandList->ResourceBarrier(1, &barrier);

    const float clearColor[] = { 0.2f, 0.2f, 0.4f, 1.0f };

    // ★変更: OMSetRenderTargets で深度バッファも設定
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle); // DSVも渡す

    // ★追加: 深度バッファもクリア
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    commandList->SetGraphicsRootSignature(rootSignature.Get());

    // CBV ヒープを設定
    if (m_cbvHeap) {
        ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() };
        commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    }
    else {
        OutputDebugStringA("[D3D12Renderer WARNING] Render: m_cbvHeap is NULL. Skipping CBV setup in render.\n");
    }

    D3D12_VIEWPORT viewport;
    viewport.TopLeftX = 0.0f; viewport.TopLeftY = 0.0f;
    viewport.Width = (float)m_Width; viewport.Height = (float)m_Height;
    viewport.MinDepth = 0.0f; viewport.MaxDepth = 1.0f;
    commandList->RSSetViewports(1, &viewport);

    D3D12_RECT scissorRect;
    scissorRect.left = 0; scissorRect.top = 0;
    scissorRect.right = m_Width; scissorRect.bottom = m_Height;
    commandList->RSSetScissorRects(1, &scissorRect);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // --- シーンのGameObjectを走査して描画 ---
    if (m_CurrentScene)
    {
        // ここでシーン内のすべてのGameObjectを再帰的に走査し、
        // MeshRendererComponentを持つものを見つけて描画します。
        // より効率的なレンダリングのためには、コンポーネントを型ごとに配列に格納するなど、
        // 別の構造を検討すべきですが、ここではシンプルな再帰を使用します。
        std::function<void(std::shared_ptr<GameObject>)> renderGameObject =
            [&](std::shared_ptr<GameObject> gameObject)
            {
                if (!gameObject) return;

                // メッシュレンダラーコンポーネントを取得
                std::shared_ptr<MeshRendererComponent> meshRenderer = gameObject->GetComponent<MeshRendererComponent>();

                if (meshRenderer && meshRenderer->VertexBuffer && meshRenderer->IndexBuffer && meshRenderer->IndexCount > 0)
                {
                    // ワールド行列を計算
                    XMMATRIX world = gameObject->Transform->GetWorldMatrix();
                    XMMATRIX mvp = world * m_ViewMatrix * m_ProjectionMatrix;
                    XMStoreFloat4x4(&m_constantBufferData.mvp, XMMatrixTranspose(mvp));
                    if (m_pCbvDataBegin) {
                        memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
                    }

                    // CBVをルートパラメータに設定
                    commandList->SetGraphicsRootDescriptorTable(0, m_cbvHeap->GetGPUDescriptorHandleForHeapStart());

                    // 頂点/インデックスバッファを設定
                    commandList->IASetVertexBuffers(0, 1, &meshRenderer->VertexBufferView);
                    commandList->IASetIndexBuffer(&meshRenderer->IndexBufferView);

                    // 描画コマンド
                    commandList->DrawIndexedInstanced(meshRenderer->IndexCount, 1, 0, 0, 0);
                }

                // 子オブジェクトを再帰的に描画
                for (const auto& child : gameObject->GetChildren())
                {
                    renderGameObject(child);
                }
            };

        for (const auto& rootGo : m_CurrentScene->GetRootGameObjects())
        {
            renderGameObject(rootGo);
        }
    }
    else
    {
        OutputDebugStringA("[D3D12Renderer WARNING] Render: No scene set to render.\n");
    }
    // --- シーンのGameObjectを走査して描画 ここまで ---

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    commandList->ResourceBarrier(1, &barrier);

    hr = commandList->Close();
    if (FAILED(hr)) { LogHRESULTError(hr, "Render: CommandList Close failed"); return; }
    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    hr = swapChain->Present(1, 0);
    if (FAILED(hr)) { LogHRESULTError(hr, "Render: Present failed"); return; }

    WaitForPreviousFrame();
    m_frameCount++;
}

void D3D12Renderer::Cleanup()
{
    // GPUの作業が全て完了するまで待機（重要）
    if (device) // デバイスがまだ有効な場合のみ待機を試みる
    {
        WaitForPreviousFrame();
    }

    if (fenceEvent) {
        CloseHandle(fenceEvent);
        fenceEvent = nullptr; // ハンドルを閉じた後にnullに設定
    }

    // 定数バッファがマップされている場合はマップ解除
    if (m_constantBuffer && m_pCbvDataBegin) {
        m_constantBuffer->Unmap(0, nullptr);
        m_pCbvDataBegin = nullptr;
    }

    // ComPtrはデストラクタで自動的に解放されるため、明示的なReset()は通常不要ですが、
    // デバッグ目的や即時解放が必要な場合は使用できます。
    // device.Reset();
    // ... 他のComPtrも同様 ...
}

void D3D12Renderer::WaitForPreviousFrame()
{
    // GPUが現在のフェンス値をシグナルするまで待機するコマンドキューにコマンドを追加します。
    HRESULT hr = commandQueue->Signal(fence.Get(), fenceValue);
    if (FAILED(hr))
    {
        LogHRESULTError(hr, "WaitForPreviousFrame: commandQueue->Signal failed");
        throw std::runtime_error("commandQueue->Signal failed");
    }

    // フェンス値が到達したかどうかをチェックします。
    // フェンス値が到達していない場合は、イベントハンドラがシグナルされるまで待機します。
    if (fence->GetCompletedValue() < fenceValue)
    {
        hr = fence->SetEventOnCompletion(fenceValue, fenceEvent);
        if (FAILED(hr))
        {
            LogHRESULTError(hr, "WaitForPreviousFrame: fence->SetEventOnCompletion failed");
            throw std::runtime_error("fence->SetEventOnCompletion failed");
        }
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    // 次のフレームのためにフェンス値をインクリメントします。
    fenceValue++;
    frameIndex = swapChain->GetCurrentBackBufferIndex();
}