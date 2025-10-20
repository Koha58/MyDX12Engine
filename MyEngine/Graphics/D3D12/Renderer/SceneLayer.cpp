// Renderer/SceneLayer.cpp
#include "Renderer/SceneLayer.h"

/*
    SceneLayer
    ----------------------------------------------------------------------------
    �����F
      - �u�V�[���`��̍����x���������v�F���ʂ� Viewports�iRT/�T�C�Y�Ǘ��j��
        SceneRenderer�i���`��R�}���h�����j�̋��n�����s���B
      - UI�iImGui�j���֋������� SRV �̑������S���B

    �Ăяo�����f���̖ڈ��i1�t���[���j�F
      1) BeginFrame()              ... UI�N���̃��T�C�Y�m���K�p�iRT�Đ���������΋�RT��Ԃ��j
      2) Record(args, maxObjects)  ... Scene��Game �̏��ŃI�t�X�N���[���`����L�^
      3) FeedToUI(ctx, imgui, ...) ... ImGui �֕`�挋�ʂ� SRV ������
      4) �i�Ăяo������ Presenter.Begin �� ImGui �� Presenter.End�j
      5) BeginFrame �ŕԂ�����RT�� FrameScheduler.EndFrame(sig) ���Œx���j���o�^

    ���ӓ_�F
      - ���T�C�Y����/�q�X�e���V�X/�������� Viewports ���ɏW��i�Ӗ������j
      - Scene �� Game �͓��𑜓x�œ������Ă���O��i����� Scene �ɓ����j
      - UI �֓n�� SRV �X���b�g�͊O���iRenderer�j���t���[���C���f�b�N�X�Ń��j�[�N�Ɋ�����
*/

void SceneLayer::Initialize(ID3D12Device* dev, DXGI_FORMAT /*rtvFmt*/, DXGI_FORMAT /*dsvFmt*/,
    FrameResources* frames, const PipelineSet& pipe,
    unsigned initW, unsigned initH)
{
    // SceneRenderer �������i�p�C�v���C��/�t���[��UPL �Ȃǂ̊֘A�t���j
    m_sceneRenderer.Initialize(dev, pipe, frames);

    // Viewports�iRT/�T�C�Y�Ǘ��j�������F�E�B���h�E�����T�C�Y�ō쐬���邱�Ƃ�
    // ����̊g�厞�ɂڂ₯�Ȃ��i���T�C�Y���g��ɂ��ꎞ�I�ȏk���`�����j
    m_viewports.Initialize(dev, initW, initH);
}

RenderTargetHandles SceneLayer::BeginFrame(ID3D12Device* dev)
{
    // UI ���őO�t���[�����Ɉ��艻������]�T�C�Y������΁A������ RT ����蒼���B
    // ��RT �́uDetach �����n���h���iComPtr���j�v�ŕԂ��̂ŁA�Ăяo������
    // GpuGarbageQueue �Ɂi���t���� fence �l�Łj�o�^���Ēx���j�����邱�ƁB
    return m_viewports.ApplyPendingResizeIfNeeded(dev);
}

void SceneLayer::Record(const SceneLayerBeginArgs& a, unsigned maxObjects)
{
    // �h��F�K�v�p�����[�^�������Ă����牽�����Ȃ��i���S���j
    if (!a.camera || !a.scene || !a.cmd) return;

    // 1) Scene �֕`��
    //    - HFOV�i��FOV�j����ɁA�A�X�y�N�g�ω��ɒǏ]���铊�e�� Viewports ���Œ���
    //    - View/Proj �̑I��� CB �ݒ�� SceneRenderer.Record ���Ŏ��s
    m_viewports.RenderScene(a.cmd, m_sceneRenderer, a.camera, a.scene, a.frameIndex, maxObjects);

    // 2) Game �֕`��
    //    - ����� Scene �Ɠ��������Œ�J�����iView/Proj�j�ŐÓI�\��
    //    - Scene ���̓��e/�A�X�y�N�g�ɍ��킹�� Game �������i���񓯊��j
    m_viewports.RenderGame(a.cmd, m_sceneRenderer, a.scene, a.frameIndex, maxObjects);
}

void SceneLayer::FeedToUI(EditorContext& ctx, ImGuiLayer* imgui,
    unsigned frameIndex, unsigned sceneSrvBase, unsigned gameSrvBase)
{
    // ImGui �֕`�挋�ʁiRT �� Color�j�� SRV �������B
    // - DX12 �� ImTextureID �́uGPU �� SRV �f�B�X�N���v�^�n���h���iUINT64�j�v�݊�
    // - �t�H���g�� slot=0 ���g���O��Ȃ̂ŁAsceneSrvBase/gameSrvBase �� 1 �ȏ��
    //   �e�t���[���iframeIndex�j�ɉ��������j�[�N�ȃX���b�g���������邱�ƁB
    m_viewports.FeedToUI(ctx, imgui, frameIndex, sceneSrvBase, gameSrvBase);
}

void SceneLayer::RequestResize(unsigned wantW, unsigned wantH, float dt)
{
    // ImGui ���ő����� �gScene �E�B���h�E���̗��p�\�T�C�Y�h �� Viewports �֓`����B
    // Viewports ���ŁF
    //   - �X�e�b�v�X�i�b�v/������
    //   - ��莞�Ԃ̈���i�q�X�e���V�X�j
    //   - pending �ւ̊m��
    // ���܂Ƃ߂Ėʓ|������B�����ł͂����n�������B
    m_viewports.RequestSceneResize(wantW, wantH, dt);
}

// �ǉ������FUI ���� Stats �\���Ȃǁu�����i�^��RT�T�C�Y�j�v�̓����Ɏg���B
void SceneLayer::SyncStatsTo(EditorContext& ctx) const
{
    // �^�̃\�[�X�FViewports ���ێ����錻�݂� RT ���T�C�Y
    ctx.sceneRTWidth = m_viewports.SceneWidth();
    ctx.sceneRTHeight = m_viewports.SceneHeight();
    ctx.gameRTWidth = m_viewports.GameWidth();
    ctx.gameRTHeight = m_viewports.GameHeight();

    // ���� EditorContext::rtWidth/rtHeight ���uScene RT �̃~���[�v�Ƃ��Ĉ��������Ȃ�
    // ���L��L�����i����̓X���b�v�`�F�C���T�C�Y�\���p�r�Ȃ̂ŃR�����g�A�E�g�j
    // ctx.rtWidth  = ctx.sceneRTWidth;
    // ctx.rtHeight = ctx.sceneRTHeight;
}

void SceneLayer::Shutdown()
{
    // ���S���F���̏�� RT ���uDetach�v���ĎQ�Ƃ�������Ă���
    // �i�����ł͕ԋp�悪�Ȃ����ߑ����j���ɂ͂Ȃ�Ȃ��B�V���b�g�_�E������
    //  GPU �����҂����x���j���L���[�S flush ���ς܂��Ă������Ɓj
    (void)m_viewports.SceneRT().Detach();
    (void)m_viewports.GameRT().Detach();

    // SceneRenderer �̌�����؂�inullptr �Ŗ������j
    m_sceneRenderer.Initialize(nullptr, PipelineSet{}, nullptr);
}

RenderTargetHandles SceneLayer::TakeCarryOverDead() {
    // Viewports �����ێ����Ă���u�����z�����[�i2�ڈȍ~�̋�RT�j�v������B
    // �Ăяo�����iRenderer�j�Łu���t���[���Ŏ̂Ă�g�v����Ȃ炱����g���B
    return m_viewports.TakeCarryOverDead();
}
