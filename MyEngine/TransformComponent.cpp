#include "TransformComponent.h"
#include <DirectXMath.h>

// ===============================================================
// �R���X�g���N�^
// ---------------------------------------------------------------
// �EComponentType �� Transform �ɐݒ�
// �E�ʒu�� (0,0,0)�A��]�� (0,0,0)�A�X�P�[���� (1,1,1) �ŏ�����
//   �� �V�K�쐬���� GameObject �͌��_�ɓ��{�Ŕz�u�����
// ===============================================================
TransformComponent::TransformComponent()
    : Component(ComponentType::Transform),
    Position(0.0f, 0.0f, 0.0f),
    Rotation(0.0f, 0.0f, 0.0f),
    Scale(1.0f, 1.0f, 1.0f)
{
}

// ===============================================================
// GetWorldMatrix
// ---------------------------------------------------------------
// �E���݂� Position / Rotation / Scale ���烏�[���h�s����\�z���ĕԂ�
// �E�`�掞�ɒ��_�����[���h���W�֕ϊ����邽�߂Ɏg�p
// 
// �y���������z
//   1) Scale �s��
//   2) RotationX �� RotationY �� RotationZ
//   3) Translation �s��
//
//   ���_ v �ɑ΂���
//     v' = v * Scale * RotX * RotY * RotZ * Translation
//   �ƂȂ�B
// 
//   DirectXMath �ł͍s��ς��u�E���獶�v�ɓK�p����邽�߁A
//   ���ۂ̕ϊ������� Scale �� RotateX �� RotateY �� RotateZ �� Translate �ɂȂ�B
// 
// �y���ӓ_�z
//   �ERotation �̓��W�A���p�ŕێ�����
//   �E�����ł̓I�C���[�p�����̂܂� XYZ �̏��œK�p���Ă���
//     �i�W���o�����b�N���̖�肪����̂ŁA�����I�ɃN�H�[�^�j�I���Ή��������j
// ===============================================================
DirectX::XMMATRIX TransformComponent::GetWorldMatrix() const
{
    using namespace DirectX;

    // �g�k
    XMMATRIX scaleMatrix = XMMatrixScaling(Scale.x, Scale.y, Scale.z);

    // ��] (X��Y��Z �̏��ɓK�p)
    XMMATRIX rotationX = XMMatrixRotationX(Rotation.x);
    XMMATRIX rotationY = XMMatrixRotationY(Rotation.y);
    XMMATRIX rotationZ = XMMatrixRotationZ(Rotation.z);

    // ���s�ړ�
    XMMATRIX translationMatrix = XMMatrixTranslation(Position.x, Position.y, Position.z);

    // �s�񍇐� (S * Rx * Ry * Rz * T)
    return scaleMatrix * rotationX * rotationY * rotationZ * translationMatrix;
}
