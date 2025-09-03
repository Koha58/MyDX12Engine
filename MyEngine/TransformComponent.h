#pragma once
#include "Component.h"
#include <DirectXMath.h>

// --- TransformComponentクラス ---
// Componentから派生し、GameObjectの位置、回転、スケールを管理する
class TransformComponent : public Component
{
public:
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Rotation;
    DirectX::XMFLOAT3 Scale;

    TransformComponent();
    DirectX::XMMATRIX GetWorldMatrix() const;
};

