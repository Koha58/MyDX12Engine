#pragma once
#include "Component.h"
#include <DirectXMath.h>

// --- TransformComponent�N���X ---
// Component����h�����AGameObject�̈ʒu�A��]�A�X�P�[�����Ǘ�����
class TransformComponent : public Component
{
public:
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Rotation;
    DirectX::XMFLOAT3 Scale;

    TransformComponent();
    DirectX::XMMATRIX GetWorldMatrix() const;
};

