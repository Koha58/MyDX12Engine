#pragma once
#include <DirectXMath.h>
#include "Core/RenderTarget.h" // �l�����o�ŕێ����邽�ߕK�v

// ---- forward declarations ----
struct ID3D12Device;
struct ID3D12GraphicsCommandList;
class SceneRenderer;
class CameraComponent;
class Scene;
class ImGuiLayer;
struct EditorContext;

class Viewports {
public:
    void Initialize(ID3D12Device* dev, unsigned w, unsigned h);

    // ImGui �p SRV ���m�ۂ� EditorContext �ɗ�������
    void FeedToUI(EditorContext& ctx, ImGuiLayer* imgui,
        unsigned frameIndex, unsigned sceneSrvBase, unsigned gameSrvBase);

    // �� UI ����̊�]�T�C�Y���L�^�i���̃t���[���ł͍�蒼���Ȃ��j
    //    dt ���󂯎�莞�Ԉ��蔻��i�q�X�e���V�X�j���s��
    void RequestSceneResize(unsigned w, unsigned h, float dt);

    // �� ���t���[���`���ŕۗ����T�C�Y��K�p�i��RT��Ԃ��j
    RenderTargetHandles ApplyPendingResizeIfNeeded(ID3D12Device* dev);

    // �`��i1�J���� �� 1RT�j
    void RenderScene(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
        const CameraComponent* cam, const Scene* scene,
        unsigned frameIndex, unsigned maxObjects);

    // �Œ�J������ Game �`��
    void RenderGame(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
        const Scene* scene, unsigned frameIndex, unsigned maxObjects);

    // HFOV �Œ�̍ē��e�i�e�X�g/�ė��p�p�� public�j
    static DirectX::XMMATRIX MakeProjConstHFov(DirectX::XMMATRIX P0, float newAspect);

    // ���݂� Scene / Game RT �̃T�C�Y�iUI ���̔�r�p�j
    unsigned SceneWidth()  const noexcept;
    unsigned SceneHeight() const noexcept;
    unsigned GameWidth()   const noexcept;
    unsigned GameHeight()  const noexcept;

    // �K�v�Ȃ疾������������ꍇ�Ɏg���i���ڑ��삵�����Ƃ��j
    RenderTarget& SceneRT() { return m_scene; }
    RenderTarget& GameRT() { return m_game; }

private:
    RenderTarget m_scene;
    RenderTarget m_game;

    DirectX::XMFLOAT4X4 m_sceneProjInit{};
    bool m_sceneProjCaptured = false;

    bool m_gameFrozen = false;
    DirectX::XMFLOAT4X4 m_gameViewInit{};
    DirectX::XMFLOAT4X4 m_gameProjInit{};

    // �� �q�X�e���V�X�p�o�b�t�@
    unsigned m_desiredW = 0, m_desiredH = 0; // UI ���痈�����߂̊�]�T�C�Y
    float    m_desiredStableTime = 0.0f;     // ��]���A���ňێ����ꂽ����

    // �� ���t���[���ɓK�p����y���f�B���O�T�C�Y�i�m��ς݁j
    unsigned m_pendingW = 0;
    unsigned m_pendingH = 0;

    // �� �����p�����[�^
    static constexpr float   kRequiredStableTime = 0.10f; // 100ms�ȏ����Ŋm��
    static constexpr unsigned kMinDeltaPx = 4;     // 4px �����͖���
    static constexpr unsigned kSnapStep = 16;    // 16px �X�i�b�v
};
