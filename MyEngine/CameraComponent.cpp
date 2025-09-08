#include "CameraComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include <windows.h>

CameraComponent::CameraComponent(GameObject* owner, float fov, float aspect, float nearZ, float farZ)
    : Component(ComponentType::Camera),
    m_Owner(owner),
    m_FOV(fov),
    m_Aspect(aspect),
    m_NearZ(nearZ),
    m_FarZ(farZ)
{
    UpdateProjectionMatrix();
    m_View = XMMatrixIdentity();
}

void CameraComponent::OnEnable() { OutputDebugStringA("CameraComponent: OnEnable\n"); }
void CameraComponent::OnDisable() { OutputDebugStringA("CameraComponent: OnDisable\n"); }
void CameraComponent::OnDestroy() { OutputDebugStringA("CameraComponent: OnDestroy\n"); }

void CameraComponent::Update(float /*deltaTime*/)
{
    // UnityのCameraは毎フレームTransformからViewを更新するイメージ
    auto transform = m_Owner->GetComponent<TransformComponent>();
    if (!transform) return;

    XMVECTOR pos = XMLoadFloat3(&transform->Position);
    XMVECTOR forward = transform->GetForwardVector();
    XMVECTOR up = transform->GetUpVector();

    UpdateViewMatrix(pos, forward, up);
}

void CameraComponent::UpdateProjectionMatrix()
{
    m_Projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FOV), m_Aspect, m_NearZ, m_FarZ);
}

void CameraComponent::UpdateViewMatrix(const XMVECTOR& position, const XMVECTOR& forward, const XMVECTOR& up)
{
    m_View = XMMatrixLookToLH(position, forward, up);
}

void CameraComponent::SetView(const DirectX::XMVECTOR& position,
    const DirectX::XMVECTOR& forward,
    const DirectX::XMVECTOR& up)
{
    m_View = XMMatrixLookToLH(position, forward, up);
}
