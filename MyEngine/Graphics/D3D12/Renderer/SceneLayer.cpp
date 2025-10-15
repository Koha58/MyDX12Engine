// Renderer/SceneLayer.cpp
#include "Renderer/SceneLayer.h"

void SceneLayer::Initialize(ID3D12Device* dev, DXGI_FORMAT /*rtvFmt*/, DXGI_FORMAT /*dsvFmt*/,
    FrameResources* frames, const PipelineSet& pipe,
    unsigned initW, unsigned initH)
{
    // SceneRenderer ������
    m_sceneRenderer.Initialize(dev, pipe, frames);

    // Viewports�iRT �Ǘ��j�������F�����E�B���h�E�T�C�Y�ō쐬�i�ڂ₯�h�~�j
    m_viewports.Initialize(dev, initW, initH);
}

RenderTargetHandles SceneLayer::BeginFrame(ID3D12Device* dev)
{
    // UI ����m�肵���y���f�B���O�E���T�C�Y������΂����œK�p
    return m_viewports.ApplyPendingResizeIfNeeded(dev);
}

void SceneLayer::Record(const SceneLayerBeginArgs& a, unsigned maxObjects)
{
    if (!a.camera || !a.scene || !a.cmd) return;

    // Scene �֕`��iHFOV �Œ�E�A�X�y�N�g�Ǐ]�� Viewports ���j
    m_viewports.RenderScene(a.cmd, m_sceneRenderer, a.camera, a.scene, a.frameIndex, maxObjects);

    // Game �֕`��i�ŏ��� Scene �Ɠ��������Œ�J�������g���j
    m_viewports.RenderGame(a.cmd, m_sceneRenderer, a.scene, a.frameIndex, maxObjects);
}

void SceneLayer::FeedToUI(EditorContext& ctx, ImGuiLayer* imgui,
    unsigned frameIndex, unsigned sceneSrvBase, unsigned gameSrvBase)
{
    m_viewports.FeedToUI(ctx, imgui, frameIndex, sceneSrvBase, gameSrvBase);
}

void SceneLayer::RequestResize(unsigned wantW, unsigned wantH, float dt)
{
    // ��������q�X�e���V�X�� Viewports ���̎����ɈϏ�
    m_viewports.RequestSceneResize(wantW, wantH, dt);
}

void SceneLayer::Shutdown()
{
    // �Â� RT ��؂藣���ĎQ�Ƃ�����i�x���j�����K�v�Ȃ炱���ŉ���L���[�ɍڂ���j
    (void)m_viewports.SceneRT().Detach();
    (void)m_viewports.GameRT().Detach();

    // SceneRenderer �̌�����؂�inullptr �Ŗ������j
    m_sceneRenderer.Initialize(nullptr, PipelineSet{}, nullptr);
}

RenderTargetHandles SceneLayer::TakeCarryOverDead() {
    return m_viewports.TakeCarryOverDead();
}
