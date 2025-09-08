#pragma once
#include "Component.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "Input.h"
#include <DirectXMath.h>

class CameraControllerComponent : public Component
{
public:
    CameraControllerComponent(GameObject* owner, CameraComponent* camera);

    void Update(float deltaTime) override;
    void OnEnable() override;
    void OnDisable() override;

private:
    CameraComponent* m_Camera;
    TransformComponent* m_Transform;

    float m_Yaw = 0.0f;
    float m_Pitch = 0.0f;

    float m_MoveSpeed = 5.0f;
    float m_MouseSensitivity = 0.1f;
};
