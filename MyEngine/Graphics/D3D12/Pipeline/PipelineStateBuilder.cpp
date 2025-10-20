#include "PipelineStateBuilder.h"
#include "d3dx12.h"
#include <d3dcompiler.h>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

/*
    �V�F�[�_�F�ŏ����� Lambert�i�g�U�j���C�e�B���O
    - �萔�o�b�t�@ b0 �� MVP / World / WorldIT / LightDir ��ێ�
    - VS�F�ʒu�ϊ� + �@���� WorldIT �Ő��K��
    - PS�FN�EL �̓��ςŃJ���[������
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
    �����F
      - ���[�g�V�O�l�`���iCBV b0 �̂݁j��g�ݗ���
      - �V�F�[�_�������^�C���ŃR���p�C���id3dcompiler�j
      - ���̓��C�A�E�g/���X�^/�u�����h/�[�x�X�e���V��������ݒ�� PSO �𐶐�
    �O��F
      - dev != nullptr
      - rtvFmt/dsvFmt �͎��ۂ̃����_�[�^�[�Q�b�g�ƍ��v���Ă��邱��
    ���ӁF
      - �����^�C���R���p�C���͊J���p�B���i�ł͎��O�R���p�C���ς݃V�F�[�_�̎g�p�����iDXC���j
      - �����ł� vs_5_0 / ps_5_0 ���g�p�iDX12 �Ȃ� 5.1 �ȏ�ł�OK�j
*/
bool BuildLambertPipeline(ID3D12Device* dev, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt, PipelineSet& outPipe)
{
    // ============================
    // 1) ���[�g�V�O�l�`���쐬
    // ============================
    // �P�� CBV (b0) ��S�X�e�[�W���Ńo�C���h
    CD3DX12_ROOT_PARAMETER root{};
    root.InitAsConstantBufferView(
        /*shaderRegister=*/0,    // b0
        /*registerSpace=*/0,     // �X�y�[�X0
        D3D12_SHADER_VISIBILITY_ALL);

    D3D12_ROOT_SIGNATURE_DESC rs{};
    rs.NumParameters = 1;
    rs.pParameters = &root;
    // IA �݂̂��g���i�s�v�ȃX�e�[�W�����ۂ��ăo���f�[�V�������ɂ�����j
    rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sig, err; // err �͎��s���Ƀ��b�Z�[�W������
    if (FAILED(D3D12SerializeRootSignature(
        &rs, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err)))
    {
        // ������ err �����Ă���΁A�l�ԉǂ̃G���[�����񂪓�����i�K�v�Ȃ烍�O�ցj
        return false;
    }

    if (FAILED(dev->CreateRootSignature(
        /*nodeMask=*/0, sig->GetBufferPointer(), sig->GetBufferSize(),
        IID_PPV_ARGS(&outPipe.root))))
    {
        return false;
    }

    // ============================
    // 2) �V�F�[�_���R���p�C��
    // ============================
    ComPtr<ID3DBlob> VS, PS, ce; // ce: �R���p�C���G���[�iblob�j�B���s���Ƀ��b�Z�[�W������

    // �J�����̓f�o�b�O/�œK���X�L�b�v�ŃR���p�C���iiterative�J�������j
    // ���i�r���h�ł� D3DCOMPILE_OPTIMIZATION_LEVEL3 �Ȃǂɐ؂�ւ���
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    if (FAILED(D3DCompile(
        kVS, std::strlen(kVS),
        /*sourceName=*/nullptr, /*defines=*/nullptr, /*include=*/nullptr,
        "main", "vs_5_0", compileFlags, 0, &VS, &ce)))
    {
        // ���s���� ce->GetBufferPointer() �ɃG���[������iASCII�j������
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
    // 3) ���̓��C�A�E�g
    // ============================
    // ���ӁF���_�\���̂� **���S��v** �����邱��
    // - float3 pos (POSITION)   : 0 �o�C�g�I�t�Z�b�g
    // - float3 normal (NORMAL)  : 12 �o�C�g�I�t�Z�b�g
    // - float4 color  (COLOR)   : 24 �o�C�g�I�t�Z�b�g
    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // ============================
    // 4) PSO �ݒ�
    // ============================
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.InputLayout = { layout, _countof(layout) };
    pso.pRootSignature = outPipe.root.Get();
    pso.VS = CD3DX12_SHADER_BYTECODE(VS.Get());
    pso.PS = CD3DX12_SHADER_BYTECODE(PS.Get());
    pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);      // ����: �J�����O�L��/�t�B��/�[�x�N���b�vON
    pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);           // ����: �A���t�@�u�����h�Ȃ�
    pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);   // ����l���x�[�X�Ɂc
    pso.DepthStencilState.DepthEnable = TRUE;                               // �[�xON
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;         // �������݂���
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;         // �߂����̂��O
    pso.SampleMask = UINT_MAX;                                    // �S�r�b�g�L��
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;      // �O�p�`���X�g��
    pso.NumRenderTargets = 1;                                           // MRT 1��
    pso.RTVFormats[0] = rtvFmt;                                      // �Ăяo������ RT �ƈ�v������
    pso.DSVFormat = dsvFmt;                                      // �Ăяo������ DSV �ƈ�v������
    pso.SampleDesc.Count = 1;                                           // MSAA �����i�K�v�Ȃ��v������j

    // ============================
    // 5) PSO �𐶐�
    // ============================
    if (FAILED(dev->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&outPipe.pso))))
        return false;

    return true;
}
