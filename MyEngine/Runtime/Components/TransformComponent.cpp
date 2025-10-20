#include "Components/TransformComponent.h"
#include <DirectXMath.h>
#include <cmath>        // std::atan2, std::sqrt

using namespace DirectX;

/*
===============================================================================
TransformComponent.cpp

座標系と約束事
- 左手系 (Left-Handed) 前提：+Z 前方, +X 右, +Y 上。
- Rotation は「度(°)」で保持する。行列化する直前にラジアンへ変換。
- 回転の合成順序は X(Pitch) → Y(Yaw) → Z(Roll) に“固定”する。
  （実装内のあらゆる回転行列生成がこの順序であることを保証）

方針
- 回転行列は必ず MakeRotationXYZ() を通す（順序の食い違い事故を防止）。
- 方向ベクトル (Forward/Right/Up) は TransformNormal(w=0) → Normalize。
  ※ TransformCoord は平行移動を含むため方向ベクトル算出には不適。
- LookAt は左手系の“向き”を逆算して Pitch/Yaw を設定（Roll は変更しない）。

注意点
- 位置と目標がほぼ同一点のときの LookAt は無効（NaN/Inf を避けるため何もしない）。
- Pitch が ±90° 近傍でジンバルロック気味になるのはオイラー角の宿命。
  必要に応じてクォータニオン設計に切り替え可能。
===============================================================================
*/

// ──────────────────────────────────────────────────────────────
// ヘルパ：一貫した回転行列生成（X→Y→Z の順）
// どこからでも“必ず”これを使うことで順序のズレを防止。
// ──────────────────────────────────────────────────────────────
static inline XMMATRIX MakeRotationXYZ(float pitchDeg, float yawDeg, float rollDeg)
{
    // 度 -> ラジアンへ変換
    const float rx = XMConvertToRadians(pitchDeg);
    const float ry = XMConvertToRadians(yawDeg);
    const float rz = XMConvertToRadians(rollDeg);

    // 個別の軸回転行列を作成
    const XMMATRIX Rx = XMMatrixRotationX(rx); // Pitch (上下)
    const XMMATRIX Ry = XMMatrixRotationY(ry); // Yaw   (左右)
    const XMMATRIX Rz = XMMatrixRotationZ(rz); // Roll  (ひねり)

    // 合成順序は固定（X→Y→Z）
    return Rx * Ry * Rz;
}

// ============================================================================
// コンストラクタ：平行移動=0、回転=0（度）、スケール=1 で初期化
// ============================================================================
TransformComponent::TransformComponent()
    : Component(ComponentType::Transform),
    Position(0.0f, 0.0f, 0.0f),
    Rotation(0.0f, 0.0f, 0.0f),   // X:Pitch, Y:Yaw, Z:Roll（いずれも度）
    Scale(1.0f, 1.0f, 1.0f)
{
}

// ----------------------------------------------------------------------------
// GetWorldMatrix
// ローカル -> ワールドの変換行列を返す。
// 左手系の一般的な合成：S * R(X→Y→Z) * T
// ※ 回転順序は MakeRotationXYZ と合わせて一貫性を担保。
// ----------------------------------------------------------------------------
XMMATRIX TransformComponent::GetWorldMatrix() const
{
    // スケーリング行列（各軸独立）
    const XMMATRIX S = XMMatrixScaling(Scale.x, Scale.y, Scale.z);

    // 回転行列（度→ラジアン化を含む、順序は X→Y→Z に固定）
    const XMMATRIX R = MakeRotationXYZ(Rotation.x, Rotation.y, Rotation.z);

    // 平行移動行列
    const XMMATRIX T = XMMatrixTranslation(Position.x, Position.y, Position.z);

    // 合成して返す
    return S * R * T;
}

// ----------------------------------------------------------------------------
// GetForwardVector
// ローカルの (0,0,1) を回転だけでワールドへ変換し、正規化して返す。
// TransformNormal(w=0) を使うことで平行移動の影響を除去。
// ----------------------------------------------------------------------------
XMVECTOR TransformComponent::GetForwardVector() const
{
    const XMMATRIX R = MakeRotationXYZ(Rotation.x, Rotation.y, Rotation.z);
    XMVECTOR v = XMVector3TransformNormal(XMVectorSet(0.f, 0.f, 1.f, 0.f), R);
    return XMVector3Normalize(v);
}

// ----------------------------------------------------------------------------
// GetRightVector
// ローカルの (1,0,0) を回転だけでワールドへ変換し、正規化して返す。
// ----------------------------------------------------------------------------
XMVECTOR TransformComponent::GetRightVector() const
{
    const XMMATRIX R = MakeRotationXYZ(Rotation.x, Rotation.y, Rotation.z);
    XMVECTOR v = XMVector3TransformNormal(XMVectorSet(1.f, 0.f, 0.f, 0.f), R);
    return XMVector3Normalize(v);
}

// ----------------------------------------------------------------------------
// GetUpVector
// ローカルの (0,1,0) を回転だけでワールドへ変換し、正規化して返す。
// ----------------------------------------------------------------------------
XMVECTOR TransformComponent::GetUpVector() const
{
    const XMMATRIX R = MakeRotationXYZ(Rotation.x, Rotation.y, Rotation.z);
    XMVECTOR v = XMVector3TransformNormal(XMVectorSet(0.f, 1.f, 0.f, 0.f), R);
    return XMVector3Normalize(v);
}

// ----------------------------------------------------------------------------
// LookAt(target)
// - 位置はそのまま、向きだけ target を向くように Rotation(度) を設定。
// - 左手系における “dir からオイラー角(ピッチ/ヨー) を逆算” する。
//   yaw   = atan2(dir.x, dir.z)
//   pitch = atan2(dir.y, sqrt(dir.x^2 + dir.z^2))
// - 目標点と同一点（ほぼゼロ距離）のときは何もしない（NaN/Inf 抑止）。
// - worldUp はここでは未使用（Roll を固定する設計のため）。必要があれば拡張可。
// ----------------------------------------------------------------------------
void TransformComponent::LookAt(const XMFLOAT3& target, const XMFLOAT3& /*worldUp*/)
{
    // 現在位置 p と目標位置 t をロード
    XMVECTOR p = XMLoadFloat3(&Position);
    XMVECTOR t = XMLoadFloat3(&target);

    // 方向ベクトル d = t - p
    XMVECTOR d = XMVectorSubtract(t, p);

    // ゼロ距離（あるいは非常に近い）なら向きは定義できないので早期 return
    if (XMVector3Less(XMVector3LengthSq(d), XMVectorReplicate(1e-8f)))
        return;

    // 正規化方向 dir = (x,y,z)
    XMVECTOR dir = XMVector3Normalize(d);
    const float x = XMVectorGetX(dir);
    const float y = XMVectorGetY(dir);
    const float z = XMVectorGetZ(dir);

    // 左手系のヨー/ピッチ逆算（範囲：yaw ∈ [-pi,pi], pitch ∈ [-pi/2,pi/2]）
    const float yawRad = std::atan2(x, z);
    const float pitchRad = std::atan2(y, std::sqrt(x * x + z * z));

    // ラジアン -> 度
    Rotation.x = XMConvertToDegrees(pitchRad); // Pitch（上向きが＋）
    Rotation.y = XMConvertToDegrees(yawRad);   // Yaw（右回りが＋）
    // Rotation.z（Roll）は保持（ここで勝手に 0 にしない）
}

// ----------------------------------------------------------------------------
// LookAt(position, target)
// 位置も同時に設定してから LookAt(target) を適用。
// ----------------------------------------------------------------------------
void TransformComponent::LookAt(const XMFLOAT3& position,
    const XMFLOAT3& target,
    const XMFLOAT3& worldUp)
{
    // 位置を先に反映
    Position = position;

    // 方向のみ算出して回転を設定（上向きはここでは使わない設計）
    LookAt(target, worldUp);
}
