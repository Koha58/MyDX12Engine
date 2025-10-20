#pragma once
#include "Components/Component.h"
#include <DirectXMath.h>

/*
===============================================================================
 TransformComponent (Header)
-------------------------------------------------------------------------------
目的
- ワールド空間における「位置(Position)・回転(Rotation)・拡縮(Scale)」を保持し、
  行列/方向ベクトル/LookAt などの基本変換ユーティリティを提供する。

座標系・表現の約束
- 左手系 (Left-Handed) を前提：+Z = 前 / +X = 右 / +Y = 上
- Rotation は「度(degree)」で保持する（行列化する直前にラジアンへ変換）。
- 回転合成順は X(Pitch) → Y(Yaw) → Z(Roll) に統一する。
  ※ 行列生成や方向ベクトルの算出でもこの順序に揃えること。
    （順序不一致は 90°ごとの“跳び”や軸ずれの原因）

実装/運用のヒント
- 方向ベクトルは TransformNormal(w=0) で「回転のみ」を適用してから Normalize。
  TransformCoord(w=1) は平行移動まで含むため方向ベクトルには不向き。
- LookAt は「位置と目標が同一点」のときは何もしない（NaN/Inf 防止）。
- Pitch±90° 付近はジンバルロックに注意。必要ならクォータニオン対応を別途検討。
- ほとんどの API は const で副作用なし。スレッド化時の外部同期は呼び出し側で。
===============================================================================
*/

class TransformComponent : public Component
{
public:
    //=========================================================================
    // フィールド（ワールド空間）
    //=========================================================================
    DirectX::XMFLOAT3 Position;  // 位置 (x, y, z)
    DirectX::XMFLOAT3 Rotation;  // 回転角（度）Pitch=X, Yaw=Y, Roll=Z
    DirectX::XMFLOAT3 Scale;     // 拡縮 (1,1,1)=等倍

    //-------------------------------------------------------------------------
    // コンストラクタ
    // 既定値: Position=(0,0,0), Rotation=(0,0,0), Scale=(1,1,1)
    //-------------------------------------------------------------------------
    TransformComponent();

    //=========================================================================
    // 行列
    //=========================================================================
    /**
     * @brief ワールド行列を返す
     * @details 合成順: Scale → RotX(Pitch) → RotY(Yaw) → RotZ(Roll) → Translate
     *          （左手系/度→ラジアン変換は実装側で行う）
     */
    DirectX::XMMATRIX GetWorldMatrix() const;

    //=========================================================================
    // 方向ベクトル（ワールド）
    //   ローカル基準:
    //     Forward = (0,0,1), Right = (1,0,0), Up = (0,1,0)
    //   実装注意:
    //     TransformNormal(w=0) で回転のみを適用し、Normalize して返す。
    //=========================================================================

    /// @brief 前方向(+Z 基準)ベクトル（ワールド）を取得
    DirectX::XMVECTOR GetForwardVector() const;

    /// @brief 右方向(+X 基準)ベクトル（ワールド）を取得
    DirectX::XMVECTOR GetRightVector() const;

    /// @brief 上方向(+Y 基準)ベクトル（ワールド）を取得
    DirectX::XMVECTOR GetUpVector() const;

    //=========================================================================
    // LookAt（オプショナル）
    //   目的: 指定した target を向くように Rotation を設定する。
    //   前提: 左手系(+Z前)。Rotation は度。Roll はここでは変更しない。
    //   仕様:
    //     - dir = normalize(target - position)
    //     - yaw   = atan2(dir.x, dir.z)
    //     - pitch = atan2(dir.y, sqrt(dir.x^2 + dir.z^2))
    //   ゼロ距離:
    //     - 位置と目標がほぼ同一点の場合は何もしない（NaN防止）。
    //=========================================================================

    /**
     * @brief 現在の Position から target を向くように Rotation を設定
     * @param target  注視点（ワールド）
     * @param worldUp 上方向（既定 {0,1,0}。本実装では Roll は固定）
     */
    void LookAt(const DirectX::XMFLOAT3& target,
        const DirectX::XMFLOAT3& worldUp = { 0.f, 1.f, 0.f });

    /**
     * @brief 指定 Position へ移動した上で target を向く
     * @param position 新しい位置（ワールド）
     * @param target   注視点（ワールド）
     * @param worldUp  上方向（ワールド）
     */
    void LookAt(const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT3& target,
        const DirectX::XMFLOAT3& worldUp);
};
