#include "PipelineStateBuilder.h"
#include "d3dx12.h"
#include <d3dcompiler.h>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

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

bool BuildLambertPipeline(ID3D12Device* dev, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt, PipelineSet& outPipe)
{
    // RootSig (CBV b0)
    CD3DX12_ROOT_PARAMETER root{}; root.InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
    D3D12_ROOT_SIGNATURE_DESC rs{}; rs.NumParameters = 1; rs.pParameters = &root;
    rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sig, err;
    if (FAILED(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err)))
        return false;
    if (FAILED(dev->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(),
        IID_PPV_ARGS(&outPipe.root))))
        return false;

    // Shaders
    ComPtr<ID3DBlob> VS, PS, ce;
    if (FAILED(D3DCompile(kVS, std::strlen(kVS), nullptr, nullptr, nullptr, "main", "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &VS, &ce)))
        return false;
    if (FAILED(D3DCompile(kPS, std::strlen(kPS), nullptr, nullptr, nullptr, "main", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &PS, &ce)))
        return false;

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.InputLayout = { layout, _countof(layout) };
    pso.pRootSignature = outPipe.root.Get();
    pso.VS = CD3DX12_SHADER_BYTECODE(VS.Get());
    pso.PS = CD3DX12_SHADER_BYTECODE(PS.Get());
    pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = rtvFmt;
    pso.DSVFormat = dsvFmt;
    pso.SampleDesc.Count = 1;

    if (FAILED(dev->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&outPipe.pso))))
        return false;

    return true;
}
