#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <DirectXMath.h>
#include "Component.h"

// ����������������������������������������������������������������������������������������������������������������������������������������������������������
// Forward Declarations�iheavy include ������ăr���h���ԒZ�k�j
// ����������������������������������������������������������������������������������������������������������������������������������������������������������
class GameObject;
class CameraComponent;
class TransformComponent;

// ============================================================================
// CameraInputState
//  - 1�t���[���ɂ�������͂́g�����h���X�i�b�v�V���b�g�Ƃ��ďW��B
//  - Update �̐擪�� ReadInput() �ɂ��\�z �� �e�n���h���֒l�n���B
//  - �u���͎擾�v�Ɓu���샍�W�b�N�v�𕪗����āA�f�o�b�O��e�X�g��e�Ղɂ���B
// ============================================================================
struct CameraInputState
{
    // ���� Buttons / Modifiers ������������������������������������������������
    bool  lmb = false;  // Left Mouse Button   �c �I�[�r�b�g�iAlt ���p���j
    bool  mmb = false;  // Middle Mouse Button �c �p��
    bool  rmb = false;  // Right Mouse Button  �c �t���C�i��]�{�ړ��j
    bool  alt = false;  // Alt                 �c �I�[�r�b�g/�h���[�̏C���L�[
    bool  ctrl = false;  // Ctrl                �c �t���C�ړ��̉����C���L�[

    // ���� Mouse delta / Wheel ������������������������������������������������
    float dx = 0.0f; // �}�E�X�ړ��� X�ipx / frame�j  +:�E / -:��
    float dy = 0.0f; // �}�E�X�ړ��� Y�ipx / frame�j  +:�� / -:��i��Win �����j
    float wheel = 0.0f; // �X�N���[���ʁi�����ˑ��F+�O / -�� ��z��j
};

// ============================================================================
// CameraTuning
//  - ���ׂĂ̊��x�E���x����ӏ��ɏW��B�O��UI����̒������e�ՁB
//  - �P�ʁFlookSpeed �� �g�x/px�h�A���� �g���[���h�P��/px or �b�h�B
// ============================================================================
struct CameraTuning
{
    float lookSpeed = 0.15f; // ������]���x�i�x/px�j
    float panSpeed = 0.01f; // �p�����x�i���[���h/px�j
    float dollySpeed = 0.10f; // Alt+RMB �h���[���x�i����/px�j
    float wheelSpeed = 0.50f; // �z�C�[���h���[�i����/�m�b�`�j
    float flySpeed = 3.0f;  // �t���C��{���x�iunits/sec�j
    float flyBoost = 3.0f;  // Ctrl �ł̉����{��
};

// ============================================================================
// CameraControllerComponent
//  - Unity ���C�N�ȃt���[�J���������񋟂���R���|�[�l���g�B
//  - Transform �𒼐ڏ��������iCamera �� Transform ���� View ��g�ޑz��j�B
//  - ����n�E�I�C���[�p�ideg�j�FRotation = {Pitch(X), Yaw(Y), Roll(Z)} ���̗p�B
//  - ImGui ���}�E�X���L���iio.WantCaptureMouse==true�j�͈�ؑ��삵�Ȃ��B
// ----------------------------------------------------------------------------
// �y����ꗗ�iUnity�����̊��o�j�z
//   �}�E�X�F
//     �EMMB�i���{�^���j+�h���b�O    �c �p���i���E�E�㉺�j
//     �ERMB�i�E�{�^���j+�h���b�O    �c ���_�̉�]�i�t���C���[�h�j
//     �EAlt + LMB�i���{�^���j+�h���b�O �c �I�[�r�b�g�i��ʒ����������_�𒆐S�ɉ�]�j
//     �E�z�C�[��                     �c �h���[�i�O��ړ��GFOV�ł͂Ȃ��ʒu�ړ��j
//   �L�[�i�t���C�� = RMB�������ɗL���j:
//     �EW / S  �c �O�i / ��ށi���[�J�� forward�j
//     �EA / D  �c �� / �E�i���[�J�� right�j
//     �EQ / E  �c ���~ / �㏸�i���[���h up�j
//     �ECtrl   �c ���x�u�[�X�g�iflyBoost �{�j
// ----------------------------------------------------------------------------
// �y�����t���[�z�i�e�t���[���j
//   1) ���͂� ReadInput() �ŃX�i�b�v�V���b�g��
//   2) ���[�h�D��x���ɏ����i�p�� �� �I�[�r�b�g �� �h���[ �� �z�C�[�� �� �t���C�j
//      �� �ǂꂩ�̃��[�h���������炻�̃t���[���� return�i�����h�~�j
// ----------------------------------------------------------------------------
// �y�݌v�����z
//   �E��������́g�p�������ԁh���ۂ�h�����߁A�������t���[���͎p����ύX���Ȃ�
//     �i*JustStarted �t���O�ŃX�L�b�v / �������_�� Transform ��ۑ������������j�B
//   �E���E/�㉺�̔��]�� HandleOrbit / HandleFly �� yaw/pitch �v�Z�̕����Œ����\�B
// ============================================================================
class CameraControllerComponent : public Component
{
public:
    // ������������������������������������������������������������������������������������������������������������������������������������������
    // ctor
    //   @owner  : �{�R���|�[�l���g���ڂ��� GameObject�iTransform ���L���b�V���j
    //   @camera : ����ΏۃJ�����iFOV ���̎Q�Ɨp�B�p���� Transform ����j
    // ������������������������������������������������������������������������������������������������������������������������������������������
    CameraControllerComponent(GameObject* owner, CameraComponent* camera);

    // ������������������������������������������������������������������������������������������������������������������������������������������
    // Update�i���t���[���j
    //   1) ���͎��W�iReadInput�j
    //   2) �D�揇���[�h�����i�p�����I�[�r�b�g���h���[���z�C�[�����t���C�j
    //   3) �ǂꂩ�����������瑁�� return�i���[�h�����h�~�j
    // ������������������������������������������������������������������������������������������������������������������������������������������
    void Update(float deltaTime) override;

    // �f�o�b�O���O�p�i�C�Ӂj
    void OnEnable() override;
    void OnDisable() override;

    // ������������������������������������������������������������������������������������������������������������������������������������������
    // Tuning �A�N�Z�X�i�O��UI���犴�x�E���x�𒲐��������ꍇ�Ɂj
    // ������������������������������������������������������������������������������������������������������������������������������������������
    CameraTuning& Tuning() { return m_cfg; }
    const CameraTuning& Tuning() const { return m_cfg; }

private:
    // === ���͂̃X�i�b�v�V���b�g�� =================================================
    CameraInputState ReadInput() const;

    // === �e���[�h�����itrue ��Ԃ����獡�t���[���͏��������j====================
    bool HandlePan(const CameraInputState& in);           // MMB�F��ʕ��s�ړ�
    bool HandleOrbit(const CameraInputState& in);           // Alt+LMB�F�����_���S�̉�]
    bool HandleDolly(const CameraInputState& in);           // Alt+RMB�F�����_�܂ł̋����ύX
    bool HandleWheel(const CameraInputState& in);           // Wheel�F�O��h���[
    bool HandleFly(const CameraInputState& in, float dt); // RMB(+WASD/QE)�F�t���C

private:
    // ���� Orbit �p������ԁiAlt+LMB�j������������������������������������������������������������������������������
    bool                  m_prevAltLmb = false;           // Alt+LMB �̑O�t���[�����
    bool                  m_orbitActive = false;           // �I�[�r�b�g��
    bool                  m_orbitJustStarted = false;           // ��������1F�͌����ڌŒ�
    DirectX::XMFLOAT3     m_orbitPivot = { 0,0,0 };         // ��]�̒��S�i�Œ�j
    float                 m_orbitYaw0 = 0.0f;            // �������̊ Yaw�ideg�j
    float                 m_orbitPitch0 = 0.0f;            // �������̊ Pitch�ideg�j
    float                 m_orbitAccX = 0.0f;            // �����ȍ~�̗݌v�h���b�O X
    float                 m_orbitAccY = 0.0f;            // �����ȍ~�̗݌v�h���b�O Y

    // ���� Fly �p������ԁiRMB�j��������������������������������������������������������������������������������������
    bool                  m_flyPrevRmb = false;           // RMB �̑O�t���[�����
    bool                  m_flyJustStarted = false;           // ��������1F�͌����ڌŒ�
    float                 m_flyYaw0 = 0.0f;            // �������̊ Yaw�ideg�j
    float                 m_flyPitch0 = 0.0f;            // �������̊ Pitch�ideg�j
    float                 m_flyAccX = 0.0f;            // �����ȍ~�̗݌v�h���b�O X
    float                 m_flyAccY = 0.0f;            // �����ȍ~�̗݌v�h���b�O Y

    // ���� �������� Transform �o�b�N�A�b�v�i�g�������u�ԂɎp������ԁh�΍�j��������������������
    DirectX::XMFLOAT3     m_pressPos{};                         // �������� Position
    DirectX::XMFLOAT3     m_pressRot{};                         // �������� Rotation�iPitch,Yaw,Roll�j

private:
    // ���� �O���Q�Ɛ�i���L�͂��Ȃ��j��������������������������������������������������������������������������������
    CameraComponent* m_Camera = nullptr; // ����ΏۃJ����
    TransformComponent* m_Transform = nullptr; // �p�����X�V���� Transform

    // ���� �p���p�����[�^�ideg�⋗���j����������������������������������������������������������������������������
    float                 m_Yaw = 0.0f;    // ������]�ideg�j
    float                 m_Pitch = 0.0f;    // ������]�ideg�j���������� �}89�� �ɃN�����v
    float                 m_OrbitDist = 5.0f;    // �I�[�r�b�g�����ipivot ����̋����j

    // Transform �� �p�x�������K�v�Ȃ痘�p�i���g�p�ł��j
    bool                  m_InitSynced = false;

    // ���� ���x�E���x�̃`���[�j���O�l ������������������������������������������������������������������������������
    CameraTuning          m_cfg{};
};
