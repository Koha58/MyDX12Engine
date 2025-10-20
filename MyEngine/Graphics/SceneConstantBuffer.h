#pragma once
#include <DirectXMath.h>

/*
================================================================================
SceneConstantBuffer.h
--------------------------------------------------------------------------------
�ړI�F
  - �V�F�[�_�[�ɓn���g1 �h���[�i�܂��� 1 �I�u�W�F�N�g�j�h���̒萔�f�[�^���`����B
  - HLSL �� cbuffer �� 1:1 �ɑΉ�����悤�A16 �o�C�g���E�ivec4 �P�ʁj�Ő��񂷂�B

�݌v�����F
  - DirectXMath �̍s��͍s�D��(XMFLOAT4x4)�����AHLSL ���� mul �����ɍ��킹��
    VS ���ň����i�T�^�I�ɂ� row-major �̂܂܎g�p�B�K�v�ɉ����� transpose ����j�B
  - �gworldIT�iWorld �s��̋t�]�u�j�h�͖@���ϊ��p�B���l�X�P�[�����܂ޏꍇ�ł�
    �������@�������𓾂邽�߂Ɏg���B
  - �o�b�t�@�T�C�Y�� 16 �̔{���ɑ�����i�����ł� 208B��224B �ۂ߂�z�肵��
    �A�b�v���[�h���� 256B �Ȃ� 256 �A���C�����g�Ɋۂ߂邱�Ƃ������j�B
    ���ۂ̃A�b�v���[�h���̓t���[�����\�[�X���� GetCBStride() �� 256B �P�ʂ�
    ���Ă���O��iD3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT = 256�j�B
�g�p��i���_�V�F�[�_�j�F
    cbuffer SceneCB : register(b0) {
      float4x4 gMVP;
      float4x4 gWorld;
      float4x4 gWorldIT;
      float3   gLightDir;
      float    _pad;
    };
    float4 VSMain(float3 pos : POSITION, float3 nrm : NORMAL) : SV_Position {
      float4 wp = mul(float4(pos,1), gWorld);
      float3 wn = normalize(mul(nrm, (float3x3)gWorldIT));
      return mul(wp, gMVP);
    }
================================================================================
*/
struct SceneConstantBuffer
{
    // 64B: Model-View-Projection�i���[�J�������[���h���r���[���v���W�F�N�V�����j
    //   ���_���N���b�v��Ԃ֕ϊ�����ŏI�s��BCPU ���� world*view*proj ��O�v�Z���ċl�߂�B
    DirectX::XMFLOAT4X4 mvp;      // offset 0, size 64

    // 64B: World �s��i���[�J�������[���h�j
    //   �f�o�b�O��p�X���Ƃ̍ė��p�i�e�p�AG-Buffer �p�Ȃǁj�Ōʂɂ��g����悤�ێ��B
    DirectX::XMFLOAT4X4 world;    // offset 64, size 64

    // 64B: World �s��̋t�]�u�iInverse Transpose�j
    //   �@���x�N�g���ϊ��p�B���l�X�P�[�����܂ރ��f���ł�������������ۂB
    DirectX::XMFLOAT4X4 worldIT;  // offset 128, size 64

    // 12B: ���C�g�����i���[���h��ԁA���K���𐄏��j
    //   ��F{0,-1,-1} �� normalize ���Ďg�p�B�s�N�Z���V�F�[�_�� Lambert/Phong �ɗ��p�B
    DirectX::XMFLOAT3   lightDir; // offset 192, size 12

    // 4B: �p�f�B���O�icbuffer �� 16B �P�ʂŋl�߂��邽�ߌ����߁j
    float pad = 0.0f;             // offset 204, size 4
};
