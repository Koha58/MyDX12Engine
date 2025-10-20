#pragma once
#include "Component.h"
#include <DirectXMath.h>

class GameObject;

/*
===============================================================================
 CameraComponent.h
 役割:
   - ゲーム内の「視点」を表すコンポーネント。
   - View 行列（位置・向き）と Projection 行列（透視投影のパラメータ）を保持し、
     レンダラ側がそれを参照して描画する。

 設計メモ:
   - 角度は度(degree)で保持し、DirectXMath の行列生成では内部でラジアンに変換する想定。
   - 行列やベクトルはヘッダの汚染を避けるため DirectX:: を**完全修飾**で使用。
   - アスペクト比は「描画対象の幅/高さ」。ウィンドウ全体ではなく**クライアント領域**を使うこと。
   - 座標系は DirectX の左手系 (LH) 前提（XMMatrixPerspectiveFovLH / XMMatrixLookToLH を使用）。

 典型的な利用手順:
   1) GameObject に CameraComponent を AddComponent する
   2) 必要に応じて SetFOV / SetAspect で投影を更新
   3) 毎フレーム Update() で、所有 GameObject の Transform から View を更新
===============================================================================
*/
class CameraComponent : public Component {
public:
    //--------------------------------------------------------------------------
    // コンストラクタ
    // @param owner  : このカメラを所有する GameObject（Transform を参照する）
    // @param fov    : 視野角（度）。一般的には 45～75 程度
    // @param aspect : アスペクト比（幅/高さ）。クライアント領域から算出して渡す
    // @param nearZ  : ニアクリップ面の距離（>0）。極端に小さすぎると深度精度が落ちる
    // @param farZ   : ファークリップ面の距離（nearZ より大きい）
    //--------------------------------------------------------------------------
    CameraComponent(GameObject* owner,
        float fov = 60.0f,
        float aspect = 16.0f / 9.0f,
        float nearZ = 0.1f,
        float farZ = 1000.0f);

    //--------------------------------------------------------------------------
    // フレーム更新
    //  - 所有 GameObject の Transform（位置/向き）から View 行列を計算する
    //  - deltaTime は今のところ未使用（将来のモーションブラー/カメラアニメ用）
    //--------------------------------------------------------------------------
    void Update(float deltaTime) override;

    // ライフサイクル通知（必要に応じてログ等に利用）
    void OnEnable() override;
    void OnDisable() override;
    void OnDestroy() override;

    //--------------------------------------------------------------------------
    // 行列取得（レンダラ側が参照）
    //  - 返す参照はこのコンポーネントの内部バッファ（所有権は移動しない）
    //--------------------------------------------------------------------------
    const DirectX::XMMATRIX& GetViewMatrix() const { return m_View; }
    const DirectX::XMMATRIX& GetProjectionMatrix() const { return m_Projection; }

    //--------------------------------------------------------------------------
    // 投影パラメータ設定
    //  - 値を変えると自動で Projection 行列が再計算される
    //--------------------------------------------------------------------------
    void SetFOV(float fov) { m_FOV = fov;     UpdateProjectionMatrix(); }
    void SetAspect(float aspect) { m_Aspect = aspect; UpdateProjectionMatrix(); }

    //--------------------------------------------------------------------------
    // View 行列を**直接**指定したい場合に使用（LookTo 形式）
    //  - position : カメラ位置（ワールド）
    //  - forward  : 注視方向（ワールド、正規化推奨）
    //  - up       : 上方向（通常は (0,1,0)）
    // ※ 通常は Transform から自動計算されるため不要。固定カメラ用途などに。
    //--------------------------------------------------------------------------
    void SetView(const DirectX::XMVECTOR& position,
        const DirectX::XMVECTOR& forward,
        const DirectX::XMVECTOR& up);

private:
    //--------------------------------------------------------------------------
    // 内部: View 行列を更新（左手系の LookTo）
    //--------------------------------------------------------------------------
    void UpdateViewMatrix(const DirectX::XMVECTOR& position,
        const DirectX::XMVECTOR& forward,
        const DirectX::XMVECTOR& up);

    //--------------------------------------------------------------------------
    // 内部: Projection 行列を更新（左手系の PerspectiveFov）
    //  - 透視投影の基本式に基づき、m_FOV/m_Aspect/m_NearZ/m_FarZ から再計算
    //--------------------------------------------------------------------------
    void UpdateProjectionMatrix();

private:
    // 所有者（Transform を参照するために必要。nullptr の場合は Update で何もしない）
    GameObject* m_Owner = nullptr;

    // 投影パラメータ（必要に応じて Editor 側から操作）
    float                 m_FOV = 60.0f;       // 視野角[deg]
    float                 m_Aspect = 16.0f / 9.0f;  // 幅/高さ
    float                 m_NearZ = 0.1f;        // 近クリップ
    float                 m_FarZ = 1000.0f;     // 遠クリップ

    // 計算済み行列（レンダラが毎フレ参照）
    DirectX::XMMATRIX     m_View;        // カメラ座標系（ワールド→ビュー）
    DirectX::XMMATRIX     m_Projection;  // 透視投影（ビュー→クリップ）
};
