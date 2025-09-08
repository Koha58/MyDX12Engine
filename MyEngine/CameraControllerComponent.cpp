#include "CameraControllerComponent.h"
#include "GameObject.h"
#include "Input.h"
#include <algorithm> // clamp

using namespace DirectX;

CameraControllerComponent::CameraControllerComponent(GameObject* owner, CameraComponent* camera)
    : Component(ComponentType::None), m_Camera(camera)
{
    m_Transform = owner->GetComponent<TransformComponent>().get();
}

void CameraControllerComponent::OnEnable()
{
    OutputDebugStringA("CameraControllerComponent: OnEnable\n");
}

void CameraControllerComponent::OnDisable()
{
    OutputDebugStringA("CameraControllerComponent: OnDisable\n");
}

void CameraControllerComponent::Update(float deltaTime)
{
    if (!m_Transform || !m_Camera) return;

    // --- ƒ}ƒEƒX“ü—Í‚Å‰ñ“] ---
    auto delta = Input::GetMouseDelta();
    m_Yaw += delta.x * m_MouseSensitivity;
    m_Pitch -= delta.y * m_MouseSensitivity;

    m_Pitch = std::clamp(m_Pitch, -89.0f, 89.0f);

    m_Transform->Rotation = { m_Pitch, m_Yaw, 0.0f };

    // --- WASDˆÚ“® ---
    XMVECTOR pos = XMLoadFloat3(&m_Transform->Position);
    XMVECTOR forward = m_Transform->GetForwardVector();
    XMVECTOR right = m_Transform->GetRightVector();
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    if (Input::GetKey(KeyCode::W)) pos += forward * m_MoveSpeed * deltaTime;
    if (Input::GetKey(KeyCode::S)) pos -= forward * m_MoveSpeed * deltaTime;
    if (Input::GetKey(KeyCode::A)) pos -= right * m_MoveSpeed * deltaTime;
    if (Input::GetKey(KeyCode::D)) pos += right * m_MoveSpeed * deltaTime;

    XMStoreFloat3(&m_Transform->Position, pos);
}
