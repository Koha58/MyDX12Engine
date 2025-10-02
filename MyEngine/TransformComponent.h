#pragma once
#include "Component.h"
#include <DirectXMath.h>

/*
-----------------------------------------------------------------------------
TransformComponent (ヘッダ)

前提/約束
- 左手系: +Z = 前 / +X = 右 / +Y = 上
- Rotation は「度（degree）」で保持（行列化でラジアンへ変換）
- 回転合成順は X(Pitch) → Y(Yaw) → Z(Roll) に統一
  （GetWorldMatrix も方向ベクトル計算もこの前提）
-----------------------------------------------------------------------------
*/

class TransformComponent : public Component
{
public:
    //=========================================================================
    // フィールド（すべてワールド空間）
    //=========================================================================

    DirectX::XMFLOAT3 Position;  // 位置 (x, y, z)
    DirectX::XMFLOAT3 Rotation;  // 回転角（度）Pitch=X, Yaw=Y, Roll=Z
    DirectX::XMFLOAT3 Scale;     // 拡縮 (1,1,1)=等倍

    //-------------------------------------------------------------------------
    // コンストラクタ
    //   既定値: Position=(0,0,0), Rotation=(0,0,0), Scale=(1,1,1)
    //-------------------------------------------------------------------------
    TransformComponent();

    //=========================================================================
    // 行列
    //=========================================================================

    /**
     * @brief ワールド行列を返す
     * 合成順: Scale → RotX(Pitch) → RotY(Yaw) → RotZ(Roll) → Translate
     * この順序は他の計算とも必ず一致させること（不整合=90°飛びなどの原因）。
     */
    DirectX::XMMATRIX GetWorldMatrix() const;

    //=========================================================================
    // 方向ベクトル（ワールド）
    //   ローカル基準:
    //     Forward = (0,0,1), Right = (1,0,0), Up = (0,1,0)
    //   実装注意:
    //     TransformNormal(w=0) で回転のみを適用し、Normalize して返す。
    //=========================================================================

    /**
     * @brief 前方向(+Z 基準)ベクトルをワールド空間で取得
     */
    DirectX::XMVECTOR GetForwardVector() const;

    /**
     * @brief 右方向(+X 基準)ベクトルをワールド空間で取得
     */
    DirectX::XMVECTOR GetRightVector() const;

    /**
     * @brief 上方向(+Y 基準)ベクトルをワールド空間で取得
     */
    DirectX::XMVECTOR GetUpVector() const;

    //=========================================================================
    // LookAt（オプショナル）
    //   目的: 指定した target を見るように Rotation を設定するユーティリティ。
    //   前提: 左手系(+Z前)。Rotation は度。Roll はここでは変更しない。
    //   仕様:
    //     - dir = normalize(target - position)
    //     - yaw   = atan2(dir.x, dir.z)
    //     - pitch = atan2(dir.y, sqrt(dir.x^2 + dir.z^2))
    //     - 位置と目標が同一点（ほぼゼロ距離）の場合は何もしない（NaN防止）。
    //   注意: Pitch ±90° 付近はジンバルロックに注意（必要ならクォータニオン化）。
    //=========================================================================

    /**
     * @brief 現在の Position から target を向くように Rotation を設定
     * @param target  注視点（ワールド）
     * @param worldUp 上方向（デフォルト {0,1,0}。本実装では Roll は固定）
     */
    void LookAt(const DirectX::XMFLOAT3& target,
        const DirectX::XMFLOAT3& worldUp = { 0.f, 1.f, 0.f });

    /**
     * @brief 指定 Position へ移動した上で target を向く
     * @param position 新しい位置
     * @param target   注視点
     * @param worldUp  上方向
     */
    void LookAt(const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT3& target,
        const DirectX::XMFLOAT3& worldUp);
};