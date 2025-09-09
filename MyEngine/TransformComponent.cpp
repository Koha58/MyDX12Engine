#include "TransformComponent.h"
#include <DirectXMath.h>

using namespace DirectX;

// ============================================================================
// TransformComponent
//  - ゲームオブジェクトの位置 (Position)、回転 (Rotation)、スケール (Scale)
//    を保持する基本コンポーネント。
//  - ここで扱う Rotation はオイラー角 (degree, X=Pitch, Y=Yaw, Z=Roll)。
//  - DirectXMath の行列を返すことで、レンダリングやカメラ計算に利用される。
// ============================================================================
TransformComponent::TransformComponent()
    : Component(ComponentType::Transform),
    Position(0.0f, 0.0f, 0.0f), // ワールド座標
    Rotation(0.0f, 0.0f, 0.0f), // X:Pitch, Y:Yaw, Z:Roll （単位: 度）
    Scale(1.0f, 1.0f, 1.0f)     // 拡大率（1=等倍）
{
}

// ----------------------------------------------------------------------------
// GetWorldMatrix
//  - Position/Rotation/Scale を合成したワールド行列を返す。
//  - 掛ける順序：Scale → RotationX → RotationY → RotationZ → Translation
//    （※この順序は「ローカル座標に拡縮 → 回転 → 平行移動」を意味する）
// ----------------------------------------------------------------------------
XMMATRIX TransformComponent::GetWorldMatrix() const
{
    // スケール行列
    XMMATRIX scaleMatrix = XMMatrixScaling(Scale.x, Scale.y, Scale.z);

    // 回転行列（オイラー角を個別に回転へ変換）
    XMMATRIX rotationX = XMMatrixRotationX(XMConvertToRadians(Rotation.x));
    XMMATRIX rotationY = XMMatrixRotationY(XMConvertToRadians(Rotation.y));
    XMMATRIX rotationZ = XMMatrixRotationZ(XMConvertToRadians(Rotation.z));

    // 平行移動行列
    XMMATRIX translationMatrix = XMMatrixTranslation(Position.x, Position.y, Position.z);

    // 最終ワールド行列
    return scaleMatrix * rotationX * rotationY * rotationZ * translationMatrix;
}

// ----------------------------------------------------------------------------
// GetForwardVector
//  - この Transform が向いている「前方向ベクトル」を返す。
//  - ローカル空間の (0,0,1) を回転行列で変換してワールド空間へ。
// ----------------------------------------------------------------------------
XMVECTOR TransformComponent::GetForwardVector() const
{
    // ローカル回転を行列に変換
    XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(Rotation.x), // Pitch
        XMConvertToRadians(Rotation.y), // Yaw
        XMConvertToRadians(Rotation.z)  // Roll
    );

    // (0,0,1) を変換 → ワールド前方向
    return XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rotation);
}

// ----------------------------------------------------------------------------
// GetRightVector
//  - この Transform の「右方向ベクトル」を返す。
//  - ローカル空間の (1,0,0) を回転してワールド空間へ。
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
//  - この Transform の「上方向ベクトル」を返す。
//  - ローカル空間の (0,1,0) を回転してワールド空間へ。
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
