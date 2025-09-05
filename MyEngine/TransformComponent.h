#pragma once
#include "Component.h"
#include <DirectXMath.h>

// ===============================================================
// TransformComponent
// ---------------------------------------------------------------
// �EGameObject �̈ʒu/��]/�X�P�[�����Ǘ�����R���|�[�l���g
// �E���ׂĂ� GameObject �͕K�� 1 �� TransformComponent ������
// �E���W�ϊ��i���[���h�s��j�̌v�Z���
// ===============================================================
class TransformComponent : public Component
{
public:
    // ----------------------------
    // �����o�ϐ�
    // ----------------------------
    DirectX::XMFLOAT3 Position; // ���[���h���W (x, y, z)
    DirectX::XMFLOAT3 Rotation; // ��] (���W�A���P��, pitch=yaw=roll or XYZ�I�C���[�p��z��)
    DirectX::XMFLOAT3 Scale;    // �g�k (x, y, z)

    // -----------------------------------------------------------
    // �R���X�g���N�^
    // �EComponentType �� Transform �ɐݒ�
    // �EPosition=(0,0,0), Rotation=(0,0,0), Scale=(1,1,1) �ŏ�����
    // -----------------------------------------------------------
    TransformComponent();

    // -----------------------------------------------------------
    // GetWorldMatrix
    // �EDirectXMath �̍s���Ԃ�
    // �E����: Scale �� Rotation �� Translation
    // �E�`�掞�Ƀ��[���h���W�ϊ��Ƃ��ăV�F�[�_�֓n��
    // @return : XMMATRIX (���[���h�ϊ��s��)
    // -----------------------------------------------------------
    DirectX::XMMATRIX GetWorldMatrix() const;
};
