#include "PipelineStateBuilder.h"
#include "d3dx12.h"
#include <d3dcompiler.h>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

/*
    シェーダ：最小限の Lambert（拡散）ライティング
    - 定数バッファ b0 に MVP / World / WorldIT / LightDir を保持
    - VS：位置変換 + 法線を WorldIT で正規化
    - PS：N・L の内積でカラーを減衰
*/
static const char* kVS = R"(
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

static const char* kPS = R"(
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

/*
    BuildLambertPipeline
    役割：
      - ルートシグネチャ（CBV b0 のみ）を組み立て
      - シェーダをランタイムでコンパイル（d3dcompiler）
      - 入力レイアウト/ラスタ/ブレンド/深度ステンシルを既定設定で PSO を生成
    前提：
      - dev != nullptr
      - rtvFmt/dsvFmt は実際のレンダーターゲットと合致していること
    注意：
      - ランタイムコンパイルは開発用。製品では事前コンパイル済みシェーダの使用推奨（DXC等）
      - ここでは vs_5_0 / ps_5_0 を使用（DX12 なら 5.1 以上でもOK）
*/
bool BuildLambertPipeline(ID3D12Device* dev, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt, PipelineSet& outPipe)
{
    // ============================
    // 1) ルートシグネチャ作成
    // ============================
    // 単一 CBV (b0) を全ステージ可視でバインド
    CD3DX12_ROOT_PARAMETER root{};
    root.InitAsConstantBufferView(
        /*shaderRegister=*/0,    // b0
        /*registerSpace=*/0,     // スペース0
        D3D12_SHADER_VISIBILITY_ALL);

    D3D12_ROOT_SIGNATURE_DESC rs{};
    rs.NumParameters = 1;
    rs.pParameters = &root;
    // IA のみを使う（不要なステージを拒否してバリデーションを緩くする）
    rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sig, err; // err は失敗時にメッセージが入る
    if (FAILED(D3D12SerializeRootSignature(
        &rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err)))
    {
        // ここで err が取れていれば、人間可読のエラー文字列が得られる（必要ならログへ）
        return false;
    }

    if (FAILED(dev->CreateRootSignature(
        /*nodeMask=*/0, sig->GetBufferPointer(), sig->GetBufferSize(),
        IID_PPV_ARGS(&outPipe.root))))
    {
        return false;
    }

    // ============================
    // 2) シェーダをコンパイル
    // ============================
    ComPtr<ID3DBlob> VS, PS, ce; // ce: コンパイルエラー（blob）。失敗時にメッセージが入る

    // 開発時はデバッグ/最適化スキップでコンパイル（iterative開発向け）
    // 製品ビルドでは D3DCOMPILE_OPTIMIZATION_LEVEL3 などに切り替える
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    if (FAILED(D3DCompile(
        kVS, std::strlen(kVS),
        /*sourceName=*/nullptr, /*defines=*/nullptr, /*include=*/nullptr,
        "main", "vs_5_0", compileFlags, 0, &VS, &ce)))
    {
        // 失敗時は ce->GetBufferPointer() にエラー文字列（ASCII）が入る
        return false;
    }

    if (FAILED(D3DCompile(
        kPS, std::strlen(kPS),
        /*sourceName=*/nullptr, /*defines=*/nullptr, /*include=*/nullptr,
        "main", "ps_5_0", compileFlags, 0, &PS, &ce)))
    {
        return false;
    }

    // ============================
    // 3) 入力レイアウト
    // ============================
    // 注意：頂点構造体と **完全一致** させること
    // - float3 pos (POSITION)   : 0 バイトオフセット
    // - float3 normal (NORMAL)  : 12 バイトオフセット
    // - float4 color  (COLOR)   : 24 バイトオフセット
    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // ============================
    // 4) PSO 設定
    // ============================
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.InputLayout = { layout, _countof(layout) };
    pso.pRootSignature = outPipe.root.Get();
    pso.VS = CD3DX12_SHADER_BYTECODE(VS.Get());
    pso.PS = CD3DX12_SHADER_BYTECODE(PS.Get());
    pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);      // 既定: カリング有り/フィル/深度クリップON
    pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);           // 既定: アルファブレンドなし
    pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);   // 既定値をベースに…
    pso.DepthStencilState.DepthEnable = TRUE;                               // 深度ON
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;         // 書き込みあり
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;         // 近いものが前
    pso.SampleMask = UINT_MAX;                                    // 全ビット有効
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;      // 三角形リスト等
    pso.NumRenderTargets = 1;                                           // MRT 1枚
    pso.RTVFormats[0] = rtvFmt;                                      // 呼び出し側の RT と一致させる
    pso.DSVFormat = dsvFmt;                                      // 呼び出し側の DSV と一致させる
    pso.SampleDesc.Count = 1;                                           // MSAA 無し（必要なら一致させる）

    // ============================
    // 5) PSO を生成
    // ============================
    if (FAILED(dev->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&outPipe.pso))))
        return false;

    return true;
}
