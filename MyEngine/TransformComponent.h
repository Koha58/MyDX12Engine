#pragma once
#include "Component.h"
#include <DirectXMath.h>

// ===============================================================
// TransformComponent
//  - GameObject の位置(Position)・回転(Rotation)・拡縮(Scale)を管理するコンポーネント。
//  - 3D空間でのワールド行列を生成する機能を提供する。
//  - Unity の Transform と同様の役割。
// ===============================================================
class TransformComponent : public Component
{
public:
    // ------------------- メンバ変数 -------------------

    DirectX::XMFLOAT3 Position;
    // ワールド座標系における位置
    // (x, y, z) の3次元座標値

    DirectX::XMFLOAT3 Rotation;
    // 回転角度（度数法）
    // pitch (x軸回転), yaw (y軸回転), roll (z軸回転)
    // → 例えば yaw を変えると水平方向の向きが変わる

    DirectX::XMFLOAT3 Scale;
    // 拡縮
    // (1,1,1) で等倍、(2,2,2) で2倍の大きさ

    // ------------------- コンストラクタ -------------------

    TransformComponent();
    // デフォルト値:
    // Position = (0,0,0), Rotation = (0,0,0), Scale = (1,1,1)

    // ------------------- 行列計算 -------------------

    DirectX::XMMATRIX GetWorldMatrix() const;
    // ワールド行列を返す
    // ・Scale → RotationX → RotationY → RotationZ → Translation の順で合成
    // ・これをシェーダーに渡すことでオブジェクトを正しい位置に描画できる

    // ------------------- 方向ベクトル取得 -------------------
    // Unity の Transform と同じように、オブジェクトの「向き」をベクトルで取得できる。

    DirectX::XMVECTOR GetForwardVector() const;
    // 前方向 (Z+ 基準) をワールド空間に変換して返す
    // カメラの前進やオブジェクトの移動方向に利用

    DirectX::XMVECTOR GetRightVector() const;
    // 右方向 (X+ 基準) をワールド空間に変換して返す
    // ストレイフ移動やローカル座標での横移動に利用

    DirectX::XMVECTOR GetUpVector() const;
    // 上方向 (Y+ 基準) をワールド空間に変換して返す
    // ジャンプやカメラの「上方向ベクトル」に利用
};
