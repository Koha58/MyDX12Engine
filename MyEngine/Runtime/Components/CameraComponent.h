#pragma once
#include "Component.h"
#include <DirectXMath.h>
using namespace DirectX;

class GameObject;

// ============================================================================
// CameraComponent
//  - ゲーム世界を「どのように見るか」を定義するコンポーネント。
//  - View 行列（カメラ位置・向き）、Projection 行列（透視投影の設定）を保持。
//  - GameObject にアタッチして使用することを前提。
// ============================================================================
class CameraComponent : public Component
{
public:
    // ------------------------------------------------------------------------
    // コンストラクタ
    // @param owner : このカメラを所有する GameObject
    // @param fov   : 視野角 (Field of View, degree)
    // @param aspect: アスペクト比 (幅/高さ)
    // @param nearZ : ニアクリップ面の距離
    // @param farZ  : ファークリップ面の距離
    // ------------------------------------------------------------------------
    CameraComponent(GameObject* owner,
        float fov = 60.0f,
        float aspect = 16.0f / 9.0f,
        float nearZ = 0.1f,
        float farZ = 1000.0f);

    // 毎フレームの更新処理
    // GameObject の Transform を参照して View 行列を再計算する。
    void Update(float deltaTime) override;

    // ライフサイクルイベント
    void OnEnable() override;   // 有効化直後に呼ばれる
    void OnDisable() override;  // 無効化直後に呼ばれる
    void OnDestroy() override;  // 破棄直前に呼ばれる

    // --- 行列取得 ---
    // レンダラーが使う View/Projection 行列を参照できるようにする。
    const XMMATRIX& GetViewMatrix() const { return m_View; }
    const XMMATRIX& GetProjectionMatrix() const { return m_Projection; }

    // --- プロパティ設定 ---
    // 値を変更したら内部で再計算して Projection 行列に反映。
    void SetFOV(float fov) { m_FOV = fov; UpdateProjectionMatrix(); }
    void SetAspect(float aspect) { m_Aspect = aspect; UpdateProjectionMatrix(); }

    // 外部から位置・方向・上ベクトルを直接渡して View 行列を設定することも可能。
    void SetView(const DirectX::XMVECTOR& position,
        const DirectX::XMVECTOR& forward,
        const DirectX::XMVECTOR& up);

private:
    // 内部処理用: View 行列を更新する
    // @param position : カメラの位置（ワールド座標）
    // @param forward  : カメラの前方向ベクトル（ワールド空間）
    // @param up       : 上方向ベクトル
    void UpdateViewMatrix(const XMVECTOR& position,
        const XMVECTOR& forward,
        const XMVECTOR& up);

    // 内部処理用: Projection 行列を更新する
    // 視野角 / アスペクト比 / ニア・ファークリップ を元に透視投影行列を作成。
    void UpdateProjectionMatrix();

private:
    // ------------------------------------------------------------------------
    // メンバ変数
    // ------------------------------------------------------------------------
    GameObject* m_Owner = nullptr; // 所有する GameObject（Transform 情報を参照する）

    float m_FOV;     // 視野角 (degree)
    float m_Aspect;  // アスペクト比
    float m_NearZ;   // ニアクリップ距離
    float m_FarZ;    // ファークリップ距離

    XMMATRIX m_View;       // View 行列（カメラ座標系）
    XMMATRIX m_Projection; // Projection 行列（透視投影）
};
