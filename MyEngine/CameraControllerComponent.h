#pragma once
#include "Component.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "Input.h"
#include <DirectXMath.h>

// ============================================================================
// CameraControllerComponent
//  - マウス移動でカメラの向きを制御し、WASD + Space/Ctrl で移動する
//    「フリーカメラ」的な操作を提供するコンポーネント。
//  - Unity の "Flythrough Camera" や "FPS Camera" に近い挙動。
//  - Transform を直接書き換えることでカメラを操作する。
// ============================================================================
class CameraControllerComponent : public Component
{
public:
    // ------------------------------------------------------------------------
    // コンストラクタ
    // @param owner  : このコンポーネントをアタッチする GameObject
    // @param camera : 操作対象となる CameraComponent
    // ------------------------------------------------------------------------
    CameraControllerComponent(GameObject* owner, CameraComponent* camera);

    // 毎フレームの更新処理
    // - マウス入力で回転を更新 (Yaw/Pitch)
    // - キーボード入力 (WASD, Space, Ctrl) で移動
    void Update(float deltaTime) override;

    // 有効化された直後に呼ばれる
    void OnEnable() override;

    // 無効化された直後に呼ばれる
    void OnDisable() override;

private:
    CameraComponent* m_Camera = nullptr;        // 制御対象のカメラコンポーネント
    TransformComponent* m_Transform = nullptr;  // 所属オブジェクトの Transform

    // --- 回転制御用 ---
    float m_Yaw = 0.0f;    // 水平方向の回転角度（度）
    float m_Pitch = 0.0f;  // 垂直方向の回転角度（度）

    // --- 操作パラメータ ---
    float m_MoveSpeed = 5.0f;          // 移動速度（units / 秒）
    float m_MouseSensitivity = 0.1f;   // マウス感度（度 / ピクセル）
};
