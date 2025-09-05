#include "TransformComponent.h"
#include <DirectXMath.h>

// ===============================================================
// コンストラクタ
// ---------------------------------------------------------------
// ・ComponentType を Transform に設定
// ・位置は (0,0,0)、回転は (0,0,0)、スケールは (1,1,1) で初期化
//   → 新規作成した GameObject は原点に等倍で配置される
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
// ・現在の Position / Rotation / Scale からワールド行列を構築して返す
// ・描画時に頂点をワールド座標へ変換するために使用
// 
// 【処理順序】
//   1) Scale 行列
//   2) RotationX → RotationY → RotationZ
//   3) Translation 行列
//
//   頂点 v に対して
//     v' = v * Scale * RotX * RotY * RotZ * Translation
//   となる。
// 
//   DirectXMath では行列積が「右から左」に適用されるため、
//   実際の変換順序は Scale → RotateX → RotateY → RotateZ → Translate になる。
// 
// 【注意点】
//   ・Rotation はラジアン角で保持する
//   ・ここではオイラー角をそのまま XYZ の順で適用している
//     （ジンバルロック等の問題があるので、将来的にクォータニオン対応も検討可）
// ===============================================================
DirectX::XMMATRIX TransformComponent::GetWorldMatrix() const
{
    using namespace DirectX;

    // 拡縮
    XMMATRIX scaleMatrix = XMMatrixScaling(Scale.x, Scale.y, Scale.z);

    // 回転 (X→Y→Z の順に適用)
    XMMATRIX rotationX = XMMatrixRotationX(Rotation.x);
    XMMATRIX rotationY = XMMatrixRotationY(Rotation.y);
    XMMATRIX rotationZ = XMMatrixRotationZ(Rotation.z);

    // 平行移動
    XMMATRIX translationMatrix = XMMatrixTranslation(Position.x, Position.y, Position.z);

    // 行列合成 (S * Rx * Ry * Rz * T)
    return scaleMatrix * rotationX * rotationY * rotationZ * translationMatrix;
}
