#pragma once
#include "Component.h"
#include <DirectXMath.h>

// ===============================================================
// TransformComponent
// ・GameObject の位置/回転/スケールを管理
// ・forward/right/up ベクトルも取得できるように拡張
// ===============================================================
class TransformComponent : public Component
{
public:
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Rotation; // pitch (x), yaw (y), roll (z) in degrees
    DirectX::XMFLOAT3 Scale;

    TransformComponent();

    DirectX::XMMATRIX GetWorldMatrix() const;

    // Unity風の方向ベクトル取得関数
    DirectX::XMVECTOR GetForwardVector() const;
    DirectX::XMVECTOR GetRightVector() const;
    DirectX::XMVECTOR GetUpVector() const;
};
