#include "CameraComponent.h"
#include "Scene/GameObject.h"
#include "TransformComponent.h"
#include <windows.h>

/*
    CameraComponent
    ----------------------------------------------------------------------------
    �����F
      - �Q�[�����́u���_�v��\���R���|�[�l���g�B
      - GameObject ������ TransformComponent�i�ʒu/��]/�X�P�[���j����
        ���t���[�� View �s����X�V����B
      - ����p(FOV)/�A�X�y�N�g��/�j�A�E�t�@�[�N���b�v���� Projection �s��𐶐��E�ێ�����B
    �g�����̓T�^�F
      - ���������� FOV/Aspect/Near/Far ��n�� �� UpdateProjectionMatrix() �������ŌĂ΂��B
      - Update() �� Transform ��ǂ݁ALookTo �� View ���X�V����i�J�����̌����� Transform �ɏ]���j�B
      - �����O������C�ӂ� View �𒼐ڐݒ肵�����ꍇ�� SetView() ���g���B
*/

// =============================
// �R���X�g���N�^
// =============================
CameraComponent::CameraComponent(GameObject* owner, float fov, float aspect, float nearZ, float farZ)
    : Component(ComponentType::Camera)
    , m_Owner(owner)
    , m_FOV(fov)
    , m_Aspect(aspect)
    , m_NearZ(nearZ)
    , m_FarZ(farZ)
{
    // ��������� Projection ������Ă����i�`��Œ����ɎQ�Ɖ\�Ɂj
    UpdateProjectionMatrix();

    // View �͈�U Identity�iTransform �ɂ�� Update() �ōX�V�����j
    m_View = DirectX::XMMatrixIdentity();
}

// =============================
// ���C�t�T�C�N���̃t�b�N�i�f�o�b�O���O�̂݁j
// =============================
void CameraComponent::OnEnable() { OutputDebugStringA("CameraComponent: OnEnable\n"); }
void CameraComponent::OnDisable() { OutputDebugStringA("CameraComponent: OnDisable\n"); }
void CameraComponent::OnDestroy() { OutputDebugStringA("CameraComponent: OnDestroy\n"); }

// =============================
// Update : Transform ���� View ���X�V
// =============================
void CameraComponent::Update(float /*deltaTime*/)
{
    auto transform = m_Owner->GetComponent<TransformComponent>();
    if (!transform) return; // Transform �������Ȃ�J�����̌����͍X�V�s�\

    // �ʒu
    DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&transform->Position);
    // �O�����^������iTransform ���̉�]����Z�o�j
    DirectX::XMVECTOR forward = transform->GetForwardVector();
    DirectX::XMVECTOR up = transform->GetUpVector();

    UpdateViewMatrix(pos, forward, up);
}

// =============================
// ���e�s��̍X�V�iLH�j
// =============================
void CameraComponent::UpdateProjectionMatrix()
{
    m_Projection = DirectX::XMMatrixPerspectiveFovLH(
        DirectX::XMConvertToRadians(m_FOV), // FOV[deg] �� [rad]
        m_Aspect,                           // ��ʂ̉�/�c
        m_NearZ,                            // �j�A�N���b�v
        m_FarZ                              // �t�@�[�N���b�v
    );
}

// =============================
// View �s��̍X�V�iLookToLH�j
// =============================
void CameraComponent::UpdateViewMatrix(const DirectX::XMVECTOR& position,
    const DirectX::XMVECTOR& forward,
    const DirectX::XMVECTOR& up)
{
    m_View = DirectX::XMMatrixLookToLH(position, forward, up);
}

// =============================
// View �𒼐ڎw��iLookToLH�j
// =============================
void CameraComponent::SetView(const DirectX::XMVECTOR& position,
    const DirectX::XMVECTOR& forward,
    const DirectX::XMVECTOR& up)
{
    m_View = DirectX::XMMatrixLookToLH(position, forward, up);
}
