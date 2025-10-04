#include "CameraComponent.h"
#include "Scene/GameObject.h"
#include "TransformComponent.h"
#include <windows.h>

// =============================
// CameraComponent
// -----------------------------
// ・ゲーム内カメラを表すコンポーネント
// ・TransformComponent の位置・向きを参照して View 行列を更新
// ・投影行列（Projection）も保持
// =============================

CameraComponent::CameraComponent(GameObject* owner, float fov, float aspect, float nearZ, float farZ)
    : Component(ComponentType::Camera),
    m_Owner(owner),
    m_FOV(fov),
    m_Aspect(aspect),
    m_NearZ(nearZ),
    m_FarZ(farZ)
{
    // 生成時に投影行列を初期化
    UpdateProjectionMatrix();
    // View 行列はとりあえず単位行列で初期化
    m_View = XMMatrixIdentity();
}

// 有効化イベント（ゲームエンジン側で呼ばれる）
void CameraComponent::OnEnable() { OutputDebugStringA("CameraComponent: OnEnable\n"); }
// 無効化イベント
void CameraComponent::OnDisable() { OutputDebugStringA("CameraComponent: OnDisable\n"); }
// 破棄イベント
void CameraComponent::OnDestroy() { OutputDebugStringA("CameraComponent: OnDestroy\n"); }

// 毎フレーム呼ばれる更新処理
void CameraComponent::Update(float /*deltaTime*/)
{
    // UnityのCamera同様、TransformからViewを更新するイメージ
    auto transform = m_Owner->GetComponent<TransformComponent>();
    if (!transform) return; // Transformが無ければ何もしない

    // ワールド座標（位置）
    XMVECTOR pos = XMLoadFloat3(&transform->Position);
    // 前方向ベクトル
    XMVECTOR forward = transform->GetForwardVector();
    // 上方向ベクトル
    XMVECTOR up = transform->GetUpVector();

    // View 行列を更新
    UpdateViewMatrix(pos, forward, up);
}

// 投影行列の更新（視野角・アスペクト比・Near/Farクリップから計算）
void CameraComponent::UpdateProjectionMatrix()
{
    m_Projection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(m_FOV), // ラジアンに変換したFOV
        m_Aspect,                  // アスペクト比
        m_NearZ,                   // 手前のクリップ距離
        m_FarZ                     // 奥のクリップ距離
    );
}

// View 行列の更新（カメラ位置・前方向・上方向から算出）
void CameraComponent::UpdateViewMatrix(const XMVECTOR& position, const XMVECTOR& forward, const XMVECTOR& up)
{
    m_View = XMMatrixLookToLH(position, forward, up);
}

// 外部から直接 View を指定する場合に使用
void CameraComponent::SetView(const DirectX::XMVECTOR& position,
    const DirectX::XMVECTOR& forward,
    const DirectX::XMVECTOR& up)
{
    m_View = XMMatrixLookToLH(position, forward, up);
}
