#include "CameraComponent.h"
#include "Scene/GameObject.h"
#include "TransformComponent.h"
#include <windows.h>

// =============================
// CameraComponent
// -----------------------------
// �E�Q�[�����J������\���R���|�[�l���g
// �ETransformComponent �̈ʒu�E�������Q�Ƃ��� View �s����X�V
// �E���e�s��iProjection�j���ێ�
// =============================

CameraComponent::CameraComponent(GameObject* owner, float fov, float aspect, float nearZ, float farZ)
    : Component(ComponentType::Camera),
    m_Owner(owner),
    m_FOV(fov),
    m_Aspect(aspect),
    m_NearZ(nearZ),
    m_FarZ(farZ)
{
    // �������ɓ��e�s���������
    UpdateProjectionMatrix();
    // View �s��͂Ƃ肠�����P�ʍs��ŏ�����
    m_View = XMMatrixIdentity();
}

// �L�����C�x���g�i�Q�[���G���W�����ŌĂ΂��j
void CameraComponent::OnEnable() { OutputDebugStringA("CameraComponent: OnEnable\n"); }
// �������C�x���g
void CameraComponent::OnDisable() { OutputDebugStringA("CameraComponent: OnDisable\n"); }
// �j���C�x���g
void CameraComponent::OnDestroy() { OutputDebugStringA("CameraComponent: OnDestroy\n"); }

// ���t���[���Ă΂��X�V����
void CameraComponent::Update(float /*deltaTime*/)
{
    // Unity��Camera���l�ATransform����View���X�V����C���[�W
    auto transform = m_Owner->GetComponent<TransformComponent>();
    if (!transform) return; // Transform��������Ή������Ȃ�

    // ���[���h���W�i�ʒu�j
    XMVECTOR pos = XMLoadFloat3(&transform->Position);
    // �O�����x�N�g��
    XMVECTOR forward = transform->GetForwardVector();
    // ������x�N�g��
    XMVECTOR up = transform->GetUpVector();

    // View �s����X�V
    UpdateViewMatrix(pos, forward, up);
}

// ���e�s��̍X�V�i����p�E�A�X�y�N�g��ENear/Far�N���b�v����v�Z�j
void CameraComponent::UpdateProjectionMatrix()
{
    m_Projection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(m_FOV), // ���W�A���ɕϊ�����FOV
        m_Aspect,                  // �A�X�y�N�g��
        m_NearZ,                   // ��O�̃N���b�v����
        m_FarZ                     // ���̃N���b�v����
    );
}

// View �s��̍X�V�i�J�����ʒu�E�O�����E���������Z�o�j
void CameraComponent::UpdateViewMatrix(const XMVECTOR& position, const XMVECTOR& forward, const XMVECTOR& up)
{
    m_View = XMMatrixLookToLH(position, forward, up);
}

// �O�����璼�� View ���w�肷��ꍇ�Ɏg�p
void CameraComponent::SetView(const DirectX::XMVECTOR& position,
    const DirectX::XMVECTOR& forward,
    const DirectX::XMVECTOR& up)
{
    m_View = XMMatrixLookToLH(position, forward, up);
}
