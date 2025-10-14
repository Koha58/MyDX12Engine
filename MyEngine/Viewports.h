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

    // UI �̊�]�T�C�Y�ɉ����� SceneRT �������ւ��i��RT��Ԃ��j
    RenderTargetHandles HandleSceneResizeIfNeeded(ID3D12Device* dev, unsigned wantW, unsigned wantH);

    // �`��i1�J���� �� 1RT�j
    void RenderScene(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
        const CameraComponent* cam, const Scene* scene,
        unsigned frameIndex, unsigned maxObjects);

    // �Œ�J������ Game �`��
    void RenderGame(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
        const Scene* scene, unsigned frameIndex, unsigned maxObjects);

    // HFOV �Œ�̍ē��e�i�e�X�g/�ė��p�p�� public�j
    static DirectX::XMMATRIX MakeProjConstHFov(DirectX::XMMATRIX P0, float newAspect);

    // �K�v�Ȃ疾������������ꍇ�Ɏg��
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
};
