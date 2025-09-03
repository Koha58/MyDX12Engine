#include "TransformComponent.h"
#include <DirectXMath.h>

TransformComponent::TransformComponent()
    : Component(ComponentType::Transform),
    Position(0.0f, 0.0f, 0.0f),
    Rotation(0.0f, 0.0f, 0.0f),
    Scale(1.0f, 1.0f, 1.0f)
{
}

DirectX::XMMATRIX TransformComponent::GetWorldMatrix() const
{
    using namespace DirectX;
    XMMATRIX scaleMatrix = XMMatrixScaling(Scale.x, Scale.y, Scale.z);
    XMMATRIX rotationX = XMMatrixRotationX(Rotation.x);
    XMMATRIX rotationY = XMMatrixRotationY(Rotation.y);
    XMMATRIX rotationZ = XMMatrixRotationZ(Rotation.z);
    XMMATRIX translationMatrix = XMMatrixTranslation(Position.x, Position.y, Position.z);
    return scaleMatrix * rotationX * rotationY * rotationZ * translationMatrix;
}
