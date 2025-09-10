#include "CameraControllerComponent.h"
#include "GameObject.h"
#include "Input.h"
#include <algorithm> // std::clamp

using namespace DirectX;

// =============================
// CameraControllerComponent
// -----------------------------
// �EFPS���C�N�ȃJ��������i�}�E�X�ŉ�]�AWASD/Space/Ctrl�ŕ��s�ړ��j
// �ETransformComponent �𒼐ڍX�V����i�ʒu�E�I�C���[��]�j
// �ECameraComponent ���̂� View �s��� Transform ����v�Z����z��
// =============================
CameraControllerComponent::CameraControllerComponent(GameObject* owner, CameraComponent* camera)
    : Component(ComponentType::None), m_Camera(camera)
{
    // Owner �� Transform ���L���b�V���i���t���[���������Ȃ��j
    // GetComponent �� shared_ptr ��Ԃ��z��̂��� .get() �Ő��|�C���^��
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

    // =============================
    // 1) �}�E�X���͂Ŏ��_��]
    // -----------------------------
    // �EYaw�i�����j�Ƀ}�E�XX�APitch�i�����j�Ƀ}�E�XY�𔽉f
    // �EPitch �͐^��/�^���t�߂ł̔��]��h������ �}89�� �ɃN�����v
    // �E�����ł́u�x���v�ŕێ����Ă���O��iTransform ���œK�X���W�A���ϊ��j
    // =============================
    auto delta = Input::GetMouseDelta();          // �t���[�����̃}�E�X�ړ��ʁi�s�N�Z���j
    m_Yaw += delta.x * m_MouseSensitivity;      // �E�h���b�O�� +Yaw
    m_Pitch -= delta.y * m_MouseSensitivity;      // ��h���b�O�� �������i�����ɍ��킹�ă}�C�i�X�j

    m_Pitch = std::clamp(m_Pitch, -89.0f, 89.0f); // �^��/�^���𒴂��Ȃ�

    // Transform �̃I�C���[�p�i�x���j�𒼐ڍX�V�iZ��]�͖��g�p�j
    m_Transform->Rotation = { m_Pitch, m_Yaw, 0.0f };

    // =============================
    // 2) �L�[���͂ňړ�
    // -----------------------------
    // �EW/S �őO��AA/D �ō��E�ASpace/Ctrl �ŏ㉺
    // �Eforward/right �͌��݂̉�]�Ɋ�Â������x�N�g���i���K���z��j
    // �E�ړ��̓��[���h��Ԃ� up(0,1,0) ���̗p�i���[�������j
    // =============================
    XMVECTOR pos = XMLoadFloat3(&m_Transform->Position);
    XMVECTOR forward = m_Transform->GetForwardVector(); // �J�����̑O�����i��]���f�ς݁j
    XMVECTOR right = m_Transform->GetRightVector();   // �J�����̉E����
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);         // ���E������i���[�����l�����Ȃ��j

    // �O��ړ�
    if (Input::GetKey(KeyCode::W)) pos += forward * m_MoveSpeed * deltaTime;
    if (Input::GetKey(KeyCode::S)) pos -= forward * m_MoveSpeed * deltaTime;

    // ���E�ړ��i�X�g���C�t�j
    if (Input::GetKey(KeyCode::A)) pos -= right * m_MoveSpeed * deltaTime;
    if (Input::GetKey(KeyCode::D)) pos += right * m_MoveSpeed * deltaTime;

    // �㉺�ړ��i���~�j
    if (Input::GetKey(KeyCode::Space))        pos += up * m_MoveSpeed * deltaTime;
    if (Input::GetKey(KeyCode::LeftControl))  pos -= up * m_MoveSpeed * deltaTime;
    if (Input::GetKey(KeyCode::RightControl)) pos -= up * m_MoveSpeed * deltaTime;

    // �ʒu�� Transform �ɏ����߂�
    XMStoreFloat3(&m_Transform->Position, pos);
}