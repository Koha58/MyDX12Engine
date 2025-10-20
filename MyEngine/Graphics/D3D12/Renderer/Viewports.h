// Renderer/Viewports.h
#pragma once
#include <DirectXMath.h>
#include <utility>             // std::move
#include "Core/RenderTarget.h" // RenderTarget / RenderTargetHandles

// fwd
struct ID3D12Device;
struct ID3D12GraphicsCommandList;
class   SceneRenderer;
class   CameraComponent;
class   Scene;
class   ImGuiLayer;
struct  EditorContext;

/*
    Viewports
    ----------------------------------------------------------------------------
    �����F
      - Scene / Game �p�̃I�t�X�N���[�� RenderTarget ���Ǘ��i�����E�Đ����E����ڏ��j
      - ImGui �֕\�����邽�߂� SRV (ImTextureID) ������
      - UI ���́u��]�T�C�Y�v���󂯎��A�\���Ɉ��肵���Ɣ��f�ł����������č쐬�i�f�o�E���X�j
      - Scene �͓s�x�J�����s����g�p�AGame �́u�ŏ��� Scene �Ɠ��������Œ� View/Proj�v���g�p

    �L�[�݌v�F
      - RequestSceneResize() �Łu��]�T�C�Y�v���󂯎��A��莞�ԁikRequiredStableTime�j
        �ϓ���������� ApplyPendingResizeIfNeeded() �Ŏ��T�C�Y�ɔ��f
      - �č쐬���A�Â� RT �� Detach() �Ńp�b�P�[�W�����ĕԂ��iGPU ������ɒx���j���j
        * ����t���[���� 2 �ȏ�̌Â� RT �����������ꍇ�� m_carryOverDead �Ƀv�[��
      - Game �̃J�����́uView=�Œ� / Proj=�A�X�y�N�g�ɉ����čX�V�v�̕��j
        * ����̂� Scene �̃J�����p���Ɓg�� FOV ��ۂ��e�h�œ���
*/

class Viewports {
public:
    // �������FScene/Game �� RT �� (w,h) �ō쐬
    void Initialize(ID3D12Device* dev, unsigned w, unsigned h);

    // ImGui �� SRV ���������AEditorContext �Ɏ�RT�T�C�Y���𔽉f
    void FeedToUI(EditorContext& ctx, ImGuiLayer* imgui,
        unsigned frameIndex, unsigned sceneSrvBase, unsigned gameSrvBase);

    // UI ����n���ꂽ Scene �E�B���h�E�́u���p�\�T�C�Y�v���󂯎��A�f�o�E���X�����ɉ�
    void RequestSceneResize(unsigned w, unsigned h, float dt);

    // �y���f�B���O����Ă������T�C�Y�𑦎��K�p
    //  - �V���� RT ���쐬
    //  - �Â� RT �� Detach() ���Ė߂��i�ő�1�j
    //  - 2�ڂ͓����� m_carryOverDead �֎����z��
    RenderTargetHandles ApplyPendingResizeIfNeeded(ID3D12Device* dev);

    // �����́g�����z�����[�h���󂯎��A�Ăяo�����iFrameScheduler::EndFrame ���j�Œx���j���o�^����
    RenderTargetHandles TakeCarryOverDead() {
        RenderTargetHandles out = std::move(m_carryOverDead);
        m_carryOverDead = {};
        return out;
    }

    // Scene �p�X�̋L�^�iScene �J�����Ɋ�Â��A��FOV�Œ�ŏcFOV���Čv�Z�j
    void RenderScene(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
        const CameraComponent* cam, const Scene* scene,
        unsigned frameIndex, unsigned maxObjects);

    // Game �p�X�̋L�^�i�ŏ��� Scene �Ɠ��������Œ� View/Proj ���g���j
    void RenderGame(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
        const Scene* scene, unsigned frameIndex, unsigned maxObjects);

    // �������e�F���̓��e P0 �� near/far �Ɓg�� FOV�h��ۂ��AnewAspect �ɍ��킹�ďc FOV ���Čv�Z
    static DirectX::XMMATRIX MakeProjConstHFov(DirectX::XMMATRIX P0, float newAspect);

    // �� RT �T�C�Y�iRT�����쐬�Ȃ� 0�j
    unsigned SceneWidth()  const noexcept;
    unsigned SceneHeight() const noexcept;
    unsigned GameWidth()   const noexcept;
    unsigned GameHeight()  const noexcept;

    // ���ڃA�N�Z�X���K�v�ȏꍇ�̎Q�ƁiImGui SRV �쐬�Ȃǁj
    RenderTarget& SceneRT() { return m_scene; }
    RenderTarget& GameRT() { return m_game; }

private:
    // �I�t�X�N���[��RT�i�J���[/�[�x�ARTV/DSV�A�J�ڃw���p�������j
    RenderTarget m_scene;
    RenderTarget m_game;

    // ---- Scene ���F���e�s��̊���L���v�`���i����̂݁j ----
    DirectX::XMFLOAT4X4 m_sceneProjInit{};  // ����̓��e��ۑ�
    bool                m_sceneProjCaptured = false;

    // ---- Game ���F�Œ�J�����i���񂾂� Scene �Ɠ����j ----
    bool                m_gameFrozen = false;      // ���񓯊����ς񂾂� true
    DirectX::XMFLOAT4X4 m_gameViewInit{};          // �Œ� View
    DirectX::XMFLOAT4X4 m_gameProjInit{};          // �Œ� Proj�i�A�X�y�N�g�ω����͍Čv�Z���čX�V�j

    // ---- �f�o�E���X��ԁi��]�T�C�Y�̈��艻�j ----
    unsigned m_desiredW = 0, m_desiredH = 0; // �ŐV�́u��]�T�C�Y�i�X�i�b�v��j�v��ێ�
    float    m_desiredStableTime = 0.0f;     // �����]���p�������ݐώ���

    // ---- �y���f�B���O ----
    unsigned m_pendingW = 0;                 // ���ۂɓK�p�\��̃T�C�Y�i���蔻���j
    unsigned m_pendingH = 0;

    // 2�ڂ́g���[�h�u����i���t���[���ŗ����Ȃ����������z���j
    RenderTargetHandles m_carryOverDead;

    // �f�o�E���X�E�X�i�b�v�̃p�����[�^
    static constexpr float    kRequiredStableTime = 0.10f; // ��莞�ԃu���Ȃ���Ίm��i�b�j
    static constexpr unsigned kMinDeltaPx = 4;      // �����ω������̂������i���g�p�Ȃ� 0�j
    static constexpr unsigned kSnapStep = 16;     // �T�C�Y�ϓ����̃X�e�b�v�X�i�b�v�ipx�j
};
