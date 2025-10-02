#pragma once
#include "Component.h"
#include <DirectXMath.h>

/*
-----------------------------------------------------------------------------
TransformComponent (�w�b�_)

�O��/��
- ����n: +Z = �O / +X = �E / +Y = ��
- Rotation �́u�x�idegree�j�v�ŕێ��i�s�񉻂Ń��W�A���֕ϊ��j
- ��]�������� X(Pitch) �� Y(Yaw) �� Z(Roll) �ɓ���
  �iGetWorldMatrix �������x�N�g���v�Z�����̑O��j
-----------------------------------------------------------------------------
*/

class TransformComponent : public Component
{
public:
    //=========================================================================
    // �t�B�[���h�i���ׂă��[���h��ԁj
    //=========================================================================

    DirectX::XMFLOAT3 Position;  // �ʒu (x, y, z)
    DirectX::XMFLOAT3 Rotation;  // ��]�p�i�x�jPitch=X, Yaw=Y, Roll=Z
    DirectX::XMFLOAT3 Scale;     // �g�k (1,1,1)=���{

    //-------------------------------------------------------------------------
    // �R���X�g���N�^
    //   ����l: Position=(0,0,0), Rotation=(0,0,0), Scale=(1,1,1)
    //-------------------------------------------------------------------------
    TransformComponent();

    //=========================================================================
    // �s��
    //=========================================================================

    /**
     * @brief ���[���h�s���Ԃ�
     * ������: Scale �� RotX(Pitch) �� RotY(Yaw) �� RotZ(Roll) �� Translate
     * ���̏����͑��̌v�Z�Ƃ��K����v�����邱�Ɓi�s����=90����тȂǂ̌����j�B
     */
    DirectX::XMMATRIX GetWorldMatrix() const;

    //=========================================================================
    // �����x�N�g���i���[���h�j
    //   ���[�J���:
    //     Forward = (0,0,1), Right = (1,0,0), Up = (0,1,0)
    //   ��������:
    //     TransformNormal(w=0) �ŉ�]�݂̂�K�p���ANormalize ���ĕԂ��B
    //=========================================================================

    /**
     * @brief �O����(+Z �)�x�N�g�������[���h��ԂŎ擾
     */
    DirectX::XMVECTOR GetForwardVector() const;

    /**
     * @brief �E����(+X �)�x�N�g�������[���h��ԂŎ擾
     */
    DirectX::XMVECTOR GetRightVector() const;

    /**
     * @brief �����(+Y �)�x�N�g�������[���h��ԂŎ擾
     */
    DirectX::XMVECTOR GetUpVector() const;

    //=========================================================================
    // LookAt�i�I�v�V���i���j
    //   �ړI: �w�肵�� target ������悤�� Rotation ��ݒ肷�郆�[�e�B���e�B�B
    //   �O��: ����n(+Z�O)�BRotation �͓x�BRoll �͂����ł͕ύX���Ȃ��B
    //   �d�l:
    //     - dir = normalize(target - position)
    //     - yaw   = atan2(dir.x, dir.z)
    //     - pitch = atan2(dir.y, sqrt(dir.x^2 + dir.z^2))
    //     - �ʒu�ƖڕW������_�i�قڃ[�������j�̏ꍇ�͉������Ȃ��iNaN�h�~�j�B
    //   ����: Pitch �}90�� �t�߂̓W���o�����b�N�ɒ��Ӂi�K�v�Ȃ�N�H�[�^�j�I�����j�B
    //=========================================================================

    /**
     * @brief ���݂� Position ���� target �������悤�� Rotation ��ݒ�
     * @param target  �����_�i���[���h�j
     * @param worldUp ������i�f�t�H���g {0,1,0}�B�{�����ł� Roll �͌Œ�j
     */
    void LookAt(const DirectX::XMFLOAT3& target,
        const DirectX::XMFLOAT3& worldUp = { 0.f, 1.f, 0.f });

    /**
     * @brief �w�� Position �ֈړ�������� target ������
     * @param position �V�����ʒu
     * @param target   �����_
     * @param worldUp  �����
     */
    void LookAt(const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT3& target,
        const DirectX::XMFLOAT3& worldUp);
};