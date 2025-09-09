#pragma once
#include "Component.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "Input.h"
#include <DirectXMath.h>

// ============================================================================
// CameraControllerComponent
//  - �}�E�X�ړ��ŃJ�����̌����𐧌䂵�AWASD + Space/Ctrl �ňړ�����
//    �u�t���[�J�����v�I�ȑ����񋟂���R���|�[�l���g�B
//  - Unity �� "Flythrough Camera" �� "FPS Camera" �ɋ߂������B
//  - Transform �𒼐ڏ��������邱�ƂŃJ�����𑀍삷��B
// ============================================================================
class CameraControllerComponent : public Component
{
public:
    // ------------------------------------------------------------------------
    // �R���X�g���N�^
    // @param owner  : ���̃R���|�[�l���g���A�^�b�`���� GameObject
    // @param camera : ����ΏۂƂȂ� CameraComponent
    // ------------------------------------------------------------------------
    CameraControllerComponent(GameObject* owner, CameraComponent* camera);

    // ���t���[���̍X�V����
    // - �}�E�X���͂ŉ�]���X�V (Yaw/Pitch)
    // - �L�[�{�[�h���� (WASD, Space, Ctrl) �ňړ�
    void Update(float deltaTime) override;

    // �L�������ꂽ����ɌĂ΂��
    void OnEnable() override;

    // ���������ꂽ����ɌĂ΂��
    void OnDisable() override;

private:
    CameraComponent* m_Camera = nullptr;        // ����Ώۂ̃J�����R���|�[�l���g
    TransformComponent* m_Transform = nullptr;  // �����I�u�W�F�N�g�� Transform

    // --- ��]����p ---
    float m_Yaw = 0.0f;    // ���������̉�]�p�x�i�x�j
    float m_Pitch = 0.0f;  // ���������̉�]�p�x�i�x�j

    // --- ����p�����[�^ ---
    float m_MoveSpeed = 5.0f;          // �ړ����x�iunits / �b�j
    float m_MouseSensitivity = 0.1f;   // �}�E�X���x�i�x / �s�N�Z���j
};
