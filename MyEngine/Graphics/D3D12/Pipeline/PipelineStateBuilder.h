#pragma once
// ============================================================================
// PipelineStateBuilder.h
// �ړI�F
//   - �gLambert �V�F�[�f�B���O�p�h�̍ŏ����� RootSignature / PSO ���ЂƂ܂Ƃ߂ɂ��A
//     �Ăяo������ 1 �֐��ŃZ�b�g�A�b�v�ł���悤�ɂ���w�b�_�B
//   - �����i.cpp�j���� HLSL �̑g�ݍ��݁iD3DCompile�j�� PSO �\�z���s���B
// �^�p�����F
//   - ���̓��C�A�E�g�� POSITION(float3) / NORMAL(float3) / COLOR(float4) ��z��B
//   - ���[�g�V�O�l�`���� CBV(b0) �� 1 �{�����iVS/PS ���L�j�Ɍ���B
//   - �[�x�͊���� ON�iLESS�A�������݂���j�B�K�v�Ȃ� .cpp ���Œ����B
//   - RTV �� 1 ���̂݁A�t�H�[�}�b�g�͌Ăяo�����Ɏw��B
// ============================================================================

#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>

// ----------------------------------------------------------------------------
// PipelineSet
// �����FBuildLambertPipeline �őg�ݏグ�� RootSignature �� PSO �̎󂯎M�B
// ���ӁFComPtr �Ȃ̂ŎQ�ƃJ�E���g�͎����Ǘ��BDestroy �͕s�v�B
// ----------------------------------------------------------------------------
struct PipelineSet {
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root;  // CBV(b0) 1 �{�̊Ȉ� RootSig
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;   // Lambert �p Graphics PSO
};

// ----------------------------------------------------------------------------
// BuildLambertPipeline
// �����FLambert �p HLSL�iVS/PS�j������ŃR���p�C�� �� RootSig �� PSO ���쐬�B
// �߂�l�Ftrue=���� / false=���s�iD3DCompile/PSO �쐬�̂ǂꂩ�����s�j
//
// �����F
//  - dev       : D3D12 �f�o�C�X
//  - rtvFormat : �o�̓J���[�� DXGI �t�H�[�}�b�g�i��FDXGI_FORMAT_R8G8B8A8_UNORM�j
//  - dsvFormat : �[�x�X�e���V���̃t�H�[�}�b�g�i��FDXGI_FORMAT_D32_FLOAT�j
//  - out       : �쐬���� RootSignature / PSO ���i�[�i�������̂ݗL���j
//
// ���҂���o�C���h�F
//  - VS/PS �Ƃ� CBV(b0) �Ɉȉ������ҁF
//      row_major float4x4 g_mvp;
//      row_major float4x4 g_world;
//      row_major float4x4 g_worldIT;
//      float3 g_lightDir; float pad;
//
// ���҂�����̓��C�A�E�g�F
//  - POSITION : float3 (offset 0)
//  - NORMAL   : float3 (offset 12)
//  - COLOR    : float4 (offset 24)
//
// ���p��F
//   PipelineSet pipe{};
//   if (!BuildLambertPipeline(dev, rtvFmt, dsvFmt, pipe)) { /* �G���[���� */ }
//   cmd->SetGraphicsRootSignature(pipe.root.Get());
//   cmd->SetPipelineState(pipe.pso.Get());
// ----------------------------------------------------------------------------
bool BuildLambertPipeline(ID3D12Device* dev,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat,
    PipelineSet& out);
