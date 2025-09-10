#pragma once
#include "Component.h"
#include <DirectXMath.h>
using namespace DirectX;

class GameObject;

// ============================================================================
// CameraComponent
//  - �Q�[�����E���u�ǂ̂悤�Ɍ��邩�v���`����R���|�[�l���g�B
//  - View �s��i�J�����ʒu�E�����j�AProjection �s��i�������e�̐ݒ�j��ێ��B
//  - GameObject �ɃA�^�b�`���Ďg�p���邱�Ƃ�O��B
// ============================================================================
class CameraComponent : public Component
{
public:
    // ------------------------------------------------------------------------
    // �R���X�g���N�^
    // @param owner : ���̃J���������L���� GameObject
    // @param fov   : ����p (Field of View, degree)
    // @param aspect: �A�X�y�N�g�� (��/����)
    // @param nearZ : �j�A�N���b�v�ʂ̋���
    // @param farZ  : �t�@�[�N���b�v�ʂ̋���
    // ------------------------------------------------------------------------
    CameraComponent(GameObject* owner,
        float fov = 60.0f,
        float aspect = 16.0f / 9.0f,
        float nearZ = 0.1f,
        float farZ = 1000.0f);

    // ���t���[���̍X�V����
    // GameObject �� Transform ���Q�Ƃ��� View �s����Čv�Z����B
    void Update(float deltaTime) override;

    // ���C�t�T�C�N���C�x���g
    void OnEnable() override;   // �L��������ɌĂ΂��
    void OnDisable() override;  // ����������ɌĂ΂��
    void OnDestroy() override;  // �j�����O�ɌĂ΂��

    // --- �s��擾 ---
    // �����_���[���g�� View/Projection �s����Q�Ƃł���悤�ɂ���B
    const XMMATRIX& GetViewMatrix() const { return m_View; }
    const XMMATRIX& GetProjectionMatrix() const { return m_Projection; }

    // --- �v���p�e�B�ݒ� ---
    // �l��ύX����������ōČv�Z���� Projection �s��ɔ��f�B
    void SetFOV(float fov) { m_FOV = fov; UpdateProjectionMatrix(); }
    void SetAspect(float aspect) { m_Aspect = aspect; UpdateProjectionMatrix(); }

    // �O������ʒu�E�����E��x�N�g���𒼐ړn���� View �s���ݒ肷�邱�Ƃ��\�B
    void SetView(const DirectX::XMVECTOR& position,
        const DirectX::XMVECTOR& forward,
        const DirectX::XMVECTOR& up);

private:
    // ���������p: View �s����X�V����
    // @param position : �J�����̈ʒu�i���[���h���W�j
    // @param forward  : �J�����̑O�����x�N�g���i���[���h��ԁj
    // @param up       : ������x�N�g��
    void UpdateViewMatrix(const XMVECTOR& position,
        const XMVECTOR& forward,
        const XMVECTOR& up);

    // ���������p: Projection �s����X�V����
    // ����p / �A�X�y�N�g�� / �j�A�E�t�@�[�N���b�v �����ɓ������e�s����쐬�B
    void UpdateProjectionMatrix();

private:
    // ------------------------------------------------------------------------
    // �����o�ϐ�
    // ------------------------------------------------------------------------
    GameObject* m_Owner = nullptr; // ���L���� GameObject�iTransform �����Q�Ƃ���j

    float m_FOV;     // ����p (degree)
    float m_Aspect;  // �A�X�y�N�g��
    float m_NearZ;   // �j�A�N���b�v����
    float m_FarZ;    // �t�@�[�N���b�v����

    XMMATRIX m_View;       // View �s��i�J�������W�n�j
    XMMATRIX m_Projection; // Projection �s��i�������e�j
};
