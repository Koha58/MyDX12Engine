#pragma once
#include "Component.h"
#include <DirectXMath.h>

// ===============================================================
// TransformComponent
// �EGameObject �̈ʒu/��]/�X�P�[�����Ǘ�
// �Eforward/right/up �x�N�g�����擾�ł���悤�Ɋg��
// ===============================================================
class TransformComponent : public Component
{
public:
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Rotation; // pitch (x), yaw (y), roll (z) in degrees
    DirectX::XMFLOAT3 Scale;

    TransformComponent();

    DirectX::XMMATRIX GetWorldMatrix() const;

    // Unity���̕����x�N�g���擾�֐�
    DirectX::XMVECTOR GetForwardVector() const;
    DirectX::XMVECTOR GetRightVector() const;
    DirectX::XMVECTOR GetUpVector() const;
};
