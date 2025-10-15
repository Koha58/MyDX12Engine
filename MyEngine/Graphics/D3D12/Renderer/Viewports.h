// Renderer/Viewports.h
#pragma once
#include <DirectXMath.h>
#include <utility> 
#include "Core/RenderTarget.h"

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

    void FeedToUI(EditorContext& ctx, ImGuiLayer* imgui,
        unsigned frameIndex, unsigned sceneSrvBase, unsigned gameSrvBase);

    void RequestSceneResize(unsigned w, unsigned h, float dt);

    // �� ���t���`���ŕۗ����T�C�Y��K�p�B�߂�l�́u�x���j�����ׂ���RT�i�ő�1�j�v�B
    RenderTargetHandles ApplyPendingResizeIfNeeded(ID3D12Device* dev);

    // �� �����z�����[������ EndFrame �ŗ������߂Ɏ擾
    RenderTargetHandles TakeCarryOverDead() {
        RenderTargetHandles out = std::move(m_carryOverDead);
        m_carryOverDead = {};
        return out;
    }

    void RenderScene(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
        const CameraComponent* cam, const Scene* scene,
        unsigned frameIndex, unsigned maxObjects);

    void RenderGame(ID3D12GraphicsCommandList* cmd, SceneRenderer& sr,
        const Scene* scene, unsigned frameIndex, unsigned maxObjects);

    static DirectX::XMMATRIX MakeProjConstHFov(DirectX::XMMATRIX P0, float newAspect);

    unsigned SceneWidth()  const noexcept;
    unsigned SceneHeight() const noexcept;
    unsigned GameWidth()   const noexcept;
    unsigned GameHeight()  const noexcept;

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

    unsigned m_desiredW = 0, m_desiredH = 0;
    float    m_desiredStableTime = 0.0f;

    unsigned m_pendingW = 0;
    unsigned m_pendingH = 0;

    // �� 2�ڂ́g���[�h�������Ɏ����z��
    RenderTargetHandles m_carryOverDead;

    static constexpr float    kRequiredStableTime = 0.10f;
    static constexpr unsigned kMinDeltaPx = 4;
    static constexpr unsigned kSnapStep = 16;
};
