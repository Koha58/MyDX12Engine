// Renderer/SceneLayer.h
#pragma once
#include <d3d12.h>
#include "Renderer/Viewports.h"
#include "Renderer/SceneRenderer.h"
#include "Editor/EditorContext.h"
#include "Editor/ImGuiLayer.h"

struct SceneLayerBeginArgs {
    ID3D12Device* device = nullptr;
    ID3D12GraphicsCommandList* cmd = nullptr;
    unsigned frameIndex = 0;
    class Scene* scene = nullptr;
    class CameraComponent* camera = nullptr;
};

class SceneLayer {
public:
    // �����T�C�Y��n����Łi�ڂ₯�h�~�j
    void Initialize(ID3D12Device* dev, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
        FrameResources* frames, const PipelineSet& pipe,
        unsigned initW, unsigned initH);

    // ��5�����ł��c���i�݊��j
    void Initialize(ID3D12Device* dev, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
        FrameResources* frames, const PipelineSet& pipe)
    {
        Initialize(dev, rtvFmt, dsvFmt, frames, pipe, 64, 64);
    }

    // �t���[���擪�ŁF�y���f�B���O�E���T�C�Y��K�p
    RenderTargetHandles BeginFrame(ID3D12Device* dev);

    // Scene/Game ���I�t�X�N���[���ɕ`��
    void Record(const SceneLayerBeginArgs& args, unsigned maxObjects);

    // ImGui �� SRV �������i�����̌Ăяo�������̂܂ܒu���ł���j
    void FeedToUI(EditorContext& ctx, ImGuiLayer* imgui,
        unsigned frameIndex, unsigned sceneSrvBase, unsigned gameSrvBase);

    // UI ����̊�]�T�C�Y���L�^�i�f�o�E���X�Ή��j
    void RequestResize(unsigned wantW, unsigned wantH, float dt);

    // ���ǉ��F�����j���iRenderer::Cleanup ����Ă΂��j
    void Shutdown();

    RenderTargetHandles TakeCarryOverDead();

    // �A�N�Z�X�n�i�K�v�Ȃ�j
    unsigned SceneWidth()  const { return m_viewports.SceneWidth(); }
    unsigned SceneHeight() const { return m_viewports.SceneHeight(); }
    unsigned GameWidth()   const { return m_viewports.GameWidth(); }
    unsigned GameHeight()  const { return m_viewports.GameHeight(); }

private:
    Viewports     m_viewports;
    SceneRenderer m_sceneRenderer;
};
