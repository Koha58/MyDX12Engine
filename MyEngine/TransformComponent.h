#pragma once
#include "Component.h"
#include <DirectXMath.h>

// ===============================================================
// TransformComponent
// ---------------------------------------------------------------
// ・GameObject の位置/回転/スケールを管理するコンポーネント
// ・すべての GameObject は必ず 1 つ TransformComponent を持つ
// ・座標変換（ワールド行列）の計算を提供
// ===============================================================
class TransformComponent : public Component
{
public:
    // ----------------------------
    // メンバ変数
    // ----------------------------
    DirectX::XMFLOAT3 Position; // ワールド座標 (x, y, z)
    DirectX::XMFLOAT3 Rotation; // 回転 (ラジアン単位, pitch=yaw=roll or XYZオイラー角を想定)
    DirectX::XMFLOAT3 Scale;    // 拡縮 (x, y, z)

    // -----------------------------------------------------------
    // コンストラクタ
    // ・ComponentType を Transform に設定
    // ・Position=(0,0,0), Rotation=(0,0,0), Scale=(1,1,1) で初期化
    // -----------------------------------------------------------
    TransformComponent();

    // -----------------------------------------------------------
    // GetWorldMatrix
    // ・DirectXMath の行列を返す
    // ・順序: Scale → Rotation → Translation
    // ・描画時にワールド座標変換としてシェーダへ渡す
    // @return : XMMATRIX (ワールド変換行列)
    // -----------------------------------------------------------
    DirectX::XMMATRIX GetWorldMatrix() const;
};
