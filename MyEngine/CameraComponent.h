#pragma once
#include "Component.h"
#include <DirectXMath.h>
using namespace DirectX;

class GameObject;

class CameraComponent : public Component
{
public:
    CameraComponent(GameObject* owner, float fov = 60.0f, float aspect = 16.0f / 9.0f,
        float nearZ = 0.1f, float farZ = 1000.0f);

    void Update(float deltaTime) override;
    void OnEnable() override;
    void OnDisable() override;
    void OnDestroy() override;

    const XMMATRIX& GetViewMatrix() const { return m_View; }
    const XMMATRIX& GetProjectionMatrix() const { return m_Projection; }

    void SetFOV(float fov) { m_FOV = fov; UpdateProjectionMatrix(); }
    void SetAspect(float aspect) { m_Aspect = aspect; UpdateProjectionMatrix(); }
    void SetView(const DirectX::XMVECTOR& position,
        const DirectX::XMVECTOR& forward,
        const DirectX::XMVECTOR& up);

private:
    void UpdateViewMatrix(const XMVECTOR& position, const XMVECTOR& forward, const XMVECTOR& up);
    void UpdateProjectionMatrix();

private:
    GameObject* m_Owner = nullptr;
    float m_FOV;
    float m_Aspect;
    float m_NearZ;
    float m_FarZ;
    XMMATRIX m_View;
    XMMATRIX m_Projection;
};
