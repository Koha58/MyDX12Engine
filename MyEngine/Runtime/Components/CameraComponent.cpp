#include "CameraComponent.h"
#include "Scene/GameObject.h"
#include "TransformComponent.h"
#include <windows.h>

/*
    CameraComponent
    ----------------------------------------------------------------------------
    役割：
      - ゲーム内の「視点」を表すコンポーネント。
      - GameObject が持つ TransformComponent（位置/回転/スケール）から
        毎フレーム View 行列を更新する。
      - 視野角(FOV)/アスペクト比/ニア・ファークリップから Projection 行列を生成・保持する。
    使い方の典型：
      - 初期化時に FOV/Aspect/Near/Far を渡す → UpdateProjectionMatrix() が内部で呼ばれる。
      - Update() は Transform を読み、LookTo で View を更新する（カメラの向きは Transform に従う）。
      - もし外部から任意の View を直接設定したい場合は SetView() を使う。
*/

// =============================
// コンストラクタ
// =============================
CameraComponent::CameraComponent(GameObject* owner, float fov, float aspect, float nearZ, float farZ)
    : Component(ComponentType::Camera)
    , m_Owner(owner)
    , m_FOV(fov)
    , m_Aspect(aspect)
    , m_NearZ(nearZ)
    , m_FarZ(farZ)
{
    // 生成直後に Projection を作っておく（描画で直ちに参照可能に）
    UpdateProjectionMatrix();

    // View は一旦 Identity（Transform により Update() で更新される）
    m_View = DirectX::XMMatrixIdentity();
}

// =============================
// ライフサイクルのフック（デバッグログのみ）
// =============================
void CameraComponent::OnEnable() { OutputDebugStringA("CameraComponent: OnEnable\n"); }
void CameraComponent::OnDisable() { OutputDebugStringA("CameraComponent: OnDisable\n"); }
void CameraComponent::OnDestroy() { OutputDebugStringA("CameraComponent: OnDestroy\n"); }

// =============================
// Update : Transform から View を更新
// =============================
void CameraComponent::Update(float /*deltaTime*/)
{
    auto transform = m_Owner->GetComponent<TransformComponent>();
    if (!transform) return; // Transform が無いならカメラの向きは更新不能

    // 位置
    DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&transform->Position);
    // 前方向／上方向（Transform 側の回転から算出）
    DirectX::XMVECTOR forward = transform->GetForwardVector();
    DirectX::XMVECTOR up = transform->GetUpVector();

    UpdateViewMatrix(pos, forward, up);
}

// =============================
// 投影行列の更新（LH）
// =============================
void CameraComponent::UpdateProjectionMatrix()
{
    m_Projection = DirectX::XMMatrixPerspectiveFovLH(
        DirectX::XMConvertToRadians(m_FOV), // FOV[deg] → [rad]
        m_Aspect,                           // 画面の横/縦
        m_NearZ,                            // ニアクリップ
        m_FarZ                              // ファークリップ
    );
}

// =============================
// View 行列の更新（LookToLH）
// =============================
void CameraComponent::UpdateViewMatrix(const DirectX::XMVECTOR& position,
    const DirectX::XMVECTOR& forward,
    const DirectX::XMVECTOR& up)
{
    m_View = DirectX::XMMatrixLookToLH(position, forward, up);
}

// =============================
// View を直接指定（LookToLH）
// =============================
void CameraComponent::SetView(const DirectX::XMVECTOR& position,
    const DirectX::XMVECTOR& forward,
    const DirectX::XMVECTOR& up)
{
    m_View = DirectX::XMMatrixLookToLH(position, forward, up);
}
