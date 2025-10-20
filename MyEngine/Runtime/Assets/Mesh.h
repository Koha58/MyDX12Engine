#pragma once
#include <vector>
#include <DirectXMath.h>

/*
===============================================================================
 Vertex / MeshData
-------------------------------------------------------------------------------
�ړI:
  - �Œ���̃W�I���g���\���i���_�z��{�C���f�b�N�X�z��j���G���W�������ŋ��L���邽�߂�
    ���ʃf�[�^�\�����`����B

�݌v����:
  - DirectXMath �̌^ (XMFLOAT3/4) ���g���ACPU ���ł� �g�v���[���ȍ\���̔z��h �Ƃ���
    ���̂܂܃A�b�v���[�h�ł���`�ɂ��Ă���B
  - HLSL ���̃��C�A�E�g�ɍ��킹�邱�Ɓi��: POSITION/ NORMAL/ COLOR�j�B
  - �@���͍���n(+Z�O)�ł̖ʂ̕\�����iCW/CCW�j�ƃJ�����O�ݒ�ɒ��ӁB
  - �C���f�b�N�X�� 32bit�iunsigned int�j�B���_���� 65535 �ȉ��ŏ\���Ȃ� 16bit �����B
===============================================================================
*/

// ============================================================================
// ���_�f�[�^
//  - �ʒu(Position)�A�@��(Normal)�A���_�J���[(Color) ������{�t�H�[�}�b�g�B
//  - HLSL �� InputLayout �ƈ�v�����邱�Ɓi��FPOSITION, NORMAL, COLOR�j�B
//    VS����F
//      struct VSInput { float3 pos:POSITION; float3 nrm:NORMAL; float4 col:COLOR; };
// ============================================================================
struct Vertex
{
    DirectX::XMFLOAT3 Position; // ���_�ʒu�i���[�J�����W�j
    DirectX::XMFLOAT3 Normal;   // ���_�@���i���[�J����ԁA���K�������j
    DirectX::XMFLOAT4 Color;    // ���_�J���[�iRGBA, 0..1�j
};

// ============================================================================
// ���b�V���f�[�^
//  - 1�̕`��Ώہi�T�u���b�V�������j���\�����钸�_�z��ƃC���f�b�N�X�z��B
//  - Indices �͎O�p�`���X�g�z��i3��1�g���C�A���O���j�B
//    * ����n�{�ʂ̕\�����iCW/CCW�j�� PSO �� RasterizerState.CullMode �ƍ��킹��B
//  - �����T�u���b�V�����~�����ꍇ�́A���̍\���̂𕡐����� or �T�u���b�V������ǉ�����B
// ============================================================================
struct MeshData
{
    std::vector<Vertex>       Vertices; // ���_���X�g�iPosition/Normal/Color�j
    std::vector<unsigned int> Indices;  // �C���f�b�N�X�i�O�p�`���X�g�O��, 32bit�j
};
