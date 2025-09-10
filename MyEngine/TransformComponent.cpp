#include "TransformComponent.h"
#include <DirectXMath.h>

using namespace DirectX;

// ============================================================================
// TransformComponent
//  - �Q�[���I�u�W�F�N�g�̈ʒu (Position)�A��] (Rotation)�A�X�P�[�� (Scale)
//    ��ێ������{�R���|�[�l���g�B
//  - �����ň��� Rotation �̓I�C���[�p (degree, X=Pitch, Y=Yaw, Z=Roll)�B
//  - DirectXMath �̍s���Ԃ����ƂŁA�����_�����O��J�����v�Z�ɗ��p�����B
// ============================================================================
TransformComponent::TransformComponent()
    : Component(ComponentType::Transform),
    Position(0.0f, 0.0f, 0.0f), // ���[���h���W
    Rotation(0.0f, 0.0f, 0.0f), // X:Pitch, Y:Yaw, Z:Roll �i�P��: �x�j
    Scale(1.0f, 1.0f, 1.0f)     // �g�嗦�i1=���{�j
{
}

// ----------------------------------------------------------------------------
// GetWorldMatrix
//  - Position/Rotation/Scale �������������[���h�s���Ԃ��B
//  - �|���鏇���FScale �� RotationX �� RotationY �� RotationZ �� Translation
//    �i�����̏����́u���[�J�����W�Ɋg�k �� ��] �� ���s�ړ��v���Ӗ�����j
// ----------------------------------------------------------------------------
XMMATRIX TransformComponent::GetWorldMatrix() const
{
    // �X�P�[���s��
    XMMATRIX scaleMatrix = XMMatrixScaling(Scale.x, Scale.y, Scale.z);

    // ��]�s��i�I�C���[�p���ʂɉ�]�֕ϊ��j
    XMMATRIX rotationX = XMMatrixRotationX(XMConvertToRadians(Rotation.x));
    XMMATRIX rotationY = XMMatrixRotationY(XMConvertToRadians(Rotation.y));
    XMMATRIX rotationZ = XMMatrixRotationZ(XMConvertToRadians(Rotation.z));

    // ���s�ړ��s��
    XMMATRIX translationMatrix = XMMatrixTranslation(Position.x, Position.y, Position.z);

    // �ŏI���[���h�s��
    return scaleMatrix * rotationX * rotationY * rotationZ * translationMatrix;
}

// ----------------------------------------------------------------------------
// GetForwardVector
//  - ���� Transform �������Ă���u�O�����x�N�g���v��Ԃ��B
//  - ���[�J����Ԃ� (0,0,1) ����]�s��ŕϊ����ă��[���h��ԂցB
// ----------------------------------------------------------------------------
XMVECTOR TransformComponent::GetForwardVector() const
{
    // ���[�J����]���s��ɕϊ�
    XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(Rotation.x), // Pitch
        XMConvertToRadians(Rotation.y), // Yaw
        XMConvertToRadians(Rotation.z)  // Roll
    );

    // (0,0,1) ��ϊ� �� ���[���h�O����
    return XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rotation);
}

// ----------------------------------------------------------------------------
// GetRightVector
//  - ���� Transform �́u�E�����x�N�g���v��Ԃ��B
//  - ���[�J����Ԃ� (1,0,0) ����]���ă��[���h��ԂցB
// ----------------------------------------------------------------------------
XMVECTOR TransformComponent::GetRightVector() const
{
    XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(Rotation.x),
        XMConvertToRadians(Rotation.y),
        XMConvertToRadians(Rotation.z)
    );
    return XMVector3TransformCoord(XMVectorSet(1, 0, 0, 0), rotation);
}

// ----------------------------------------------------------------------------
// GetUpVector
//  - ���� Transform �́u������x�N�g���v��Ԃ��B
//  - ���[�J����Ԃ� (0,1,0) ����]���ă��[���h��ԂցB
// ----------------------------------------------------------------------------
XMVECTOR TransformComponent::GetUpVector() const
{
    XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(Rotation.x),
        XMConvertToRadians(Rotation.y),
        XMConvertToRadians(Rotation.z)
    );
    return XMVector3TransformCoord(XMVectorSet(0, 1, 0, 0), rotation);
}
