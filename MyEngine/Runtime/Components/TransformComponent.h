#pragma once
#include "Components/Component.h"
#include <DirectXMath.h>

/*
===============================================================================
 TransformComponent (Header)
-------------------------------------------------------------------------------
�ړI
- ���[���h��Ԃɂ�����u�ʒu(Position)�E��](Rotation)�E�g�k(Scale)�v��ێ����A
  �s��/�����x�N�g��/LookAt �Ȃǂ̊�{�ϊ����[�e�B���e�B��񋟂���B

���W�n�E�\���̖�
- ����n (Left-Handed) ��O��F+Z = �O / +X = �E / +Y = ��
- Rotation �́u�x(degree)�v�ŕێ�����i�s�񉻂��钼�O�Ƀ��W�A���֕ϊ��j�B
- ��]�������� X(Pitch) �� Y(Yaw) �� Z(Roll) �ɓ��ꂷ��B
  �� �s�񐶐�������x�N�g���̎Z�o�ł����̏����ɑ����邱�ƁB
    �i�����s��v�� 90�����Ƃ́g���сh�⎲����̌����j

����/�^�p�̃q���g
- �����x�N�g���� TransformNormal(w=0) �Łu��]�̂݁v��K�p���Ă��� Normalize�B
  TransformCoord(w=1) �͕��s�ړ��܂Ŋ܂ނ��ߕ����x�N�g���ɂ͕s�����B
- LookAt �́u�ʒu�ƖڕW������_�v�̂Ƃ��͉������Ȃ��iNaN/Inf �h�~�j�B
- Pitch�}90�� �t�߂̓W���o�����b�N�ɒ��ӁB�K�v�Ȃ�N�H�[�^�j�I���Ή���ʓr�����B
- �قƂ�ǂ� API �� const �ŕ���p�Ȃ��B�X���b�h�����̊O�������͌Ăяo�����ŁB
===============================================================================
*/

class TransformComponent : public Component
{
public:
    //=========================================================================
    // �t�B�[���h�i���[���h��ԁj
    //=========================================================================
    DirectX::XMFLOAT3 Position;  // �ʒu (x, y, z)
    DirectX::XMFLOAT3 Rotation;  // ��]�p�i�x�jPitch=X, Yaw=Y, Roll=Z
    DirectX::XMFLOAT3 Scale;     // �g�k (1,1,1)=���{

    //-------------------------------------------------------------------------
    // �R���X�g���N�^
    // ����l: Position=(0,0,0), Rotation=(0,0,0), Scale=(1,1,1)
    //-------------------------------------------------------------------------
    TransformComponent();

    //=========================================================================
    // �s��
    //=========================================================================
    /**
     * @brief ���[���h�s���Ԃ�
     * @details ������: Scale �� RotX(Pitch) �� RotY(Yaw) �� RotZ(Roll) �� Translate
     *          �i����n/�x�����W�A���ϊ��͎������ōs���j
     */
    DirectX::XMMATRIX GetWorldMatrix() const;

    //=========================================================================
    // �����x�N�g���i���[���h�j
    //   ���[�J���:
    //     Forward = (0,0,1), Right = (1,0,0), Up = (0,1,0)
    //   ��������:
    //     TransformNormal(w=0) �ŉ�]�݂̂�K�p���ANormalize ���ĕԂ��B
    //=========================================================================

    /// @brief �O����(+Z �)�x�N�g���i���[���h�j���擾
    DirectX::XMVECTOR GetForwardVector() const;

    /// @brief �E����(+X �)�x�N�g���i���[���h�j���擾
    DirectX::XMVECTOR GetRightVector() const;

    /// @brief �����(+Y �)�x�N�g���i���[���h�j���擾
    DirectX::XMVECTOR GetUpVector() const;

    //=========================================================================
    // LookAt�i�I�v�V���i���j
    //   �ړI: �w�肵�� target �������悤�� Rotation ��ݒ肷��B
    //   �O��: ����n(+Z�O)�BRotation �͓x�BRoll �͂����ł͕ύX���Ȃ��B
    //   �d�l:
    //     - dir = normalize(target - position)
    //     - yaw   = atan2(dir.x, dir.z)
    //     - pitch = atan2(dir.y, sqrt(dir.x^2 + dir.z^2))
    //   �[������:
    //     - �ʒu�ƖڕW���قړ���_�̏ꍇ�͉������Ȃ��iNaN�h�~�j�B
    //=========================================================================

    /**
     * @brief ���݂� Position ���� target �������悤�� Rotation ��ݒ�
     * @param target  �����_�i���[���h�j
     * @param worldUp ������i���� {0,1,0}�B�{�����ł� Roll �͌Œ�j
     */
    void LookAt(const DirectX::XMFLOAT3& target,
        const DirectX::XMFLOAT3& worldUp = { 0.f, 1.f, 0.f });

    /**
     * @brief �w�� Position �ֈړ�������� target ������
     * @param position �V�����ʒu�i���[���h�j
     * @param target   �����_�i���[���h�j
     * @param worldUp  ������i���[���h�j
     */
    void LookAt(const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT3& target,
        const DirectX::XMFLOAT3& worldUp);
};
