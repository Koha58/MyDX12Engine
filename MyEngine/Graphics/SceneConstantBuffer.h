#pragma once
#include <DirectXMath.h>

// ============================================================================
// SceneConstantBuffer
//  - �V�[���S�̂Ŏg�p����萔�f�[�^���i�[����\���́B
//  - GPU (HLSL �V�F�[�_�[���� cbufffer) �� 1:1 �Ή������邽�߁A
//    �T�C�Y�⃁���o�̕��т� 16�o�C�g���E�ɑ�����K�v������B
// ============================================================================
struct SceneConstantBuffer
{
    DirectX::XMFLOAT4X4 mvp;      // 64�o�C�g
    // Model-View-Projection �s��
    // �E���[�J�����W �� ���[���h �� �r���[ �� �v���W�F�N�V�����ϊ����܂Ƃ߂��s��
    // �E���_�V�F�[�_�[�Œ��_���N���b�v��Ԃɕϊ�����̂Ɏg�p

    DirectX::XMFLOAT4X4 world;    // 64�o�C�g (�݌v 128)
    // ���[���h�s��
    // �E���[�J�����W �� ���[���h���W�ւ̕ϊ�
    // �E���[���h��Ԃł̈ʒu/��]/�X�P�[����\��

    DirectX::XMFLOAT4X4 worldIT;  // 64�o�C�g (�݌v 192)
    // ���[���h�s��̋t�]�u (Inverse Transpose)
    // �E�@���x�N�g���𐳂������[���h��Ԃ֕ϊ����邽�߂Ɏg�p
    // �E�s�񂪃X�P�[�����O���܂ޏꍇ�ł�������������ێ�����

    DirectX::XMFLOAT3   lightDir; // 12�o�C�g (�݌v 204)
    // ���[���h��Ԃł̃��C�g�̕����x�N�g��
    // �E��: (0, -1, -1) �Ȃ�u�΂߉��������������v

    float pad;                    // 4�o�C�g (�݌v 208)
    // 16�o�C�g���E�ɑ����邽�߂̃p�f�B���O
    // HLSL �� cbuffer �� 16 �o�C�g�P�ʂŔz�u����邽�ߕK�{
};
