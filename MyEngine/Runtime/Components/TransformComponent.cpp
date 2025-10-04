#include "Components/TransformComponent.h"
#include <DirectXMath.h>
#include <cmath>        // std::atan2, std::sqrt

using namespace DirectX;

/*
-------------------------------------------------------------------------------
TransformComponent.cpp — 簡易メモ

前提
- 左手系(+Z 前, +X 右, +Y 上)。
- 回転は「度」で保持。行列にする時だけラジアンへ。
- 回転合成順は必ず X(Pitch) → Y(Yaw) → Z(Roll) に統一。

実装指針
- 回転行列は MakeRotationXYZ() で一元化（順序の取り違え防止）。
- 方向ベクトル(Forward/Right/Up)は TransformNormal(w=0)→Normalize。
  ※ TransformCoord は平行移動を含むので使わない。

LookAt の要点（左手系）
- dir = normalize(target - position)。
- yaw   = atan2(dir.x, dir.z)
- pitch = atan2(dir.y, sqrt(dir.x^2 + dir.z^2))
- 位置と目標が同一点（|dir|≈0）のときは何もしない（NaN 防止）。

備考
- 上記の統一を崩すと「90°ごとに跳ぶ」等の不一致が起きやすい。
- Pitch ±90° 付近はジンバルロックに注意（必要なら quaternion 検討）。
-------------------------------------------------------------------------------
*/

// ──────────────────────────────────────────────────────────────
// ヘルパー：回転行列の一貫性を担保（X→Y→Z の順で合成）
// どこからでも必ずこの関数を使って回転行列を作ること。
// ──────────────────────────────────────────────────────────────
static inline XMMATRIX MakeRotationXYZ(float pitchDeg, float yawDeg, float rollDeg)
{
    // 各軸の回転を「度→ラジアン」に変換してから個別の回転行列を生成
    const XMMATRIX Rx = XMMatrixRotationX(XMConvertToRadians(pitchDeg)); // Pitch (X)
    const XMMATRIX Ry = XMMatrixRotationY(XMConvertToRadians(yawDeg));   // Yaw   (Y)
    const XMMATRIX Rz = XMMatrixRotationZ(XMConvertToRadians(rollDeg));  // Roll  (Z)

    // ←この順番（X→Y→Z）を全箇所で統一すること！
    return Rx * Ry * Rz;
}

// ============================================================================
// TransformComponent
//  - Position/Rotation(度)/Scale を保持
//  - 左手系(+Z 前) 前提で実装
//  - Rotation は「度」で保持し、行列化の直前だけラジアン変換
// ============================================================================
TransformComponent::TransformComponent()
    : Component(ComponentType::Transform),
    Position(0.0f, 0.0f, 0.0f),
    Rotation(0.0f, 0.0f, 0.0f),  // X:Pitch, Y:Yaw, Z:Roll（度）
    Scale(1.0f, 1.0f, 1.0f)
{
}

// ----------------------------------------------------------------------------
// GetWorldMatrix
//  - 「ローカル → ワールド」変換の核となる行列。
//  - S * R(X→Y→Z) * T で合成。
//    ※ここでも、回転順序は必ず MakeRotationXYZ と同じになるよう統一。
// ----------------------------------------------------------------------------
XMMATRIX TransformComponent::GetWorldMatrix() const
{
    const XMMATRIX S = XMMatrixScaling(Scale.x, Scale.y, Scale.z);
    const XMMATRIX R = MakeRotationXYZ(Rotation.x, Rotation.y, Rotation.z); // ←統一ポイント
    const XMMATRIX T = XMMatrixTranslation(Position.x, Position.y, Position.z);

    // 最終的なワールド行列（左手系）
    return S * R * T;
}

// ----------------------------------------------------------------------------
// GetForwardVector
//  - ローカル(0,0,1) を「回転のみ」でワールドへ。
//  - TransformNormal（w=0）＋ Normalize を使用（平行移動の影響を除去）。
// ----------------------------------------------------------------------------
XMVECTOR TransformComponent::GetForwardVector() const
{
    const XMMATRIX R = MakeRotationXYZ(Rotation.x, Rotation.y, Rotation.z); // ←統一
    // w=0 の方向ベクトルを回転。最後に正規化しておくと精度面で安心。
    XMVECTOR v = XMVector3TransformNormal(XMVectorSet(0.f, 0.f, 1.f, 0.f), R);
    return XMVector3Normalize(v);
}

// ----------------------------------------------------------------------------
// GetRightVector : ローカル(1,0,0) → ワールド
// ----------------------------------------------------------------------------
XMVECTOR TransformComponent::GetRightVector() const
{
    const XMMATRIX R = MakeRotationXYZ(Rotation.x, Rotation.y, Rotation.z); // ←統一
    XMVECTOR v = XMVector3TransformNormal(XMVectorSet(1.f, 0.f, 0.f, 0.f), R);
    return XMVector3Normalize(v);
}

// ----------------------------------------------------------------------------
// GetUpVector : ローカル(0,1,0) → ワールド
// ----------------------------------------------------------------------------
XMVECTOR TransformComponent::GetUpVector() const
{
    const XMMATRIX R = MakeRotationXYZ(Rotation.x, Rotation.y, Rotation.z); // ←統一
    XMVECTOR v = XMVector3TransformNormal(XMVectorSet(0.f, 1.f, 0.f, 0.f), R);
    return XMVector3Normalize(v);
}

// ----------------------------------------------------------------------------
// LookAt(target)
//  - 位置は保持したまま、向きだけ target を向くように Rotation(度) を設定。
//  - 左手系(+Z 前)想定の逆算式：
//      forward = (sinYaw * cosPitch,  sinPitch,  cosYaw * cosPitch)
//      yaw     = atan2(x, z)
//      pitch   = atan2(y, sqrt(x^2 + z^2))
//  - 「位置と目標が同一点」の場合は方向が定義できないため何もしない。
// ----------------------------------------------------------------------------
void TransformComponent::LookAt(const XMFLOAT3& target, const XMFLOAT3& /*worldUp*/)
{
    // 現在位置と目標位置をベクトルへ
    XMVECTOR p = XMLoadFloat3(&Position);
    XMVECTOR t = XMLoadFloat3(&target);

    // ゼロ距離ガード：同一点（あるいは極端に近い）の場合は早期 return
    // これが無いと正規化で NaN/Inf 化し、回転角が「飛ぶ」原因になる。
    XMVECTOR d = XMVectorSubtract(t, p);
    if (XMVector3Less(XMVector3LengthSq(d), XMVectorReplicate(1e-8f)))
        return;

    // 位置→目標 方向の正規化ベクトル dir=(x,y,z)
    XMVECTOR dir = XMVector3Normalize(d);
    const float x = XMVectorGetX(dir);
    const float y = XMVectorGetY(dir);
    const float z = XMVectorGetZ(dir);

    // atan2 を使用：asin より端で安定。x,z の並びにも注意（左手系想定で yaw=atan2(x,z)）
    const float yawRad = std::atan2(x, z);                              // [-pi, pi]
    const float pitchRad = std::atan2(y, std::sqrt(x * x + z * z));       // [-pi/2, pi/2]

    // 度に戻して格納
    Rotation.x = XMConvertToDegrees(pitchRad);  // Pitch（上向き＋）
    Rotation.y = XMConvertToDegrees(yawRad);    // Yaw（右回り＋）
    // Rotation.z（Roll）はここでは変更しない。必要なら 0 固定の運用も可。
}

// ----------------------------------------------------------------------------
// LookAt(position, target)
//  - 位置も同時に設定してから LookAt(target) を適用。
// ----------------------------------------------------------------------------
void TransformComponent::LookAt(const XMFLOAT3& position,
    const XMFLOAT3& target,
    const XMFLOAT3& worldUp)
{
    Position = position;
    LookAt(target, worldUp);  // 上の処理に委譲（ゼロ距離ガード含む）
}
