#include "TransformComponent.h"
#include <DirectXMath.h>

using namespace DirectX;

TransformComponent::TransformComponent()
    : Component(ComponentType::Transform),
    Position(0.0f, 0.0f, 0.0f),
    Rotation(0.0f, 0.0f, 0.0f),
    Scale(1.0f, 1.0f, 1.0f)
{
}

XMMATRIX TransformComponent::GetWorldMatrix() const
{
    XMMATRIX scaleMatrix = XMMatrixScaling(Scale.x, Scale.y, Scale.z);

    XMMATRIX rotationX = XMMatrixRotationX(XMConvertToRadians(Rotation.x));
    XMMATRIX rotationY = XMMatrixRotationY(XMConvertToRadians(Rotation.y));
    XMMATRIX rotationZ = XMMatrixRotationZ(XMConvertToRadians(Rotation.z));

    XMMATRIX translationMatrix = XMMatrixTranslation(Position.x, Position.y, Position.z);

    return scaleMatrix * rotationX * rotationY * rotationZ * translationMatrix;
}

XMVECTOR TransformComponent::GetForwardVector() const
{
    XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(Rotation.x),
        XMConvertToRadians(Rotation.y),
        XMConvertToRadians(Rotation.z));
    return XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rotation);
}

XMVECTOR TransformComponent::GetRightVector() const
{
    XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(Rotation.x),
        XMConvertToRadians(Rotation.y),
        XMConvertToRadians(Rotation.z));
    return XMVector3TransformCoord(XMVectorSet(1, 0, 0, 0), rotation);
}

XMVECTOR TransformComponent::GetUpVector() const
{
    XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(Rotation.x),
        XMConvertToRadians(Rotation.y),
        XMConvertToRadians(Rotation.z));
    return XMVector3TransformCoord(XMVectorSet(0, 1, 0, 0), rotation);
}
