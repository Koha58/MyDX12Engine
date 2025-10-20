// Renderer/SceneLayer.h
#pragma once
#include <d3d12.h>
#include "Renderer/Viewports.h"
#include "Renderer/SceneRenderer.h"
#include "Editor/EditorContext.h"
#include "Editor/ImGuiLayer.h"

/*
    SceneLayer
    ----------------------------------------------------------------------------
    �����F
      - Scene(RenderTarget) �� Game(RenderTarget) �̓�ʃr���[���܂Ƃ߂ĊǗ��B
      - Viewports�iRT����/���T�C�Y/ImGui�\���pSRV�̋����j��
        SceneRenderer�i���b�V���`��/CB�X�V�j�̋��n�����s�������t�@�T�[�h�B

    �t���[���̊�{�t���[�iRenderer ������̌Ăяo�����j�F
      1) BeginFrame(dev)
         - �O�t���[���� UI ���v���������T�C�Y�i�y���f�B���O�j���m�肵�� RT ����蒼��
         - �Â� RT�i�ő�1�Z�b�g�j�� RenderTargetHandles �Ƃ��ĕԂ�
           �� Renderer::EndFrame �� GpuGarbageQueue �ɓo�^���Ēx���j������
      2) Record(args, maxObjects)
         - Scene RT / Game RT �̗����ɕ`��R�}���h���L�^
         - Scene �͖���J�����ɒǏ]�AGame �́u�ŏ���1�񂾂��vScene �Ɠ������A���̌�͌Œ�
      3) FeedToUI(ctx, imgui, ...)
         - �� RT �� SRV�iImTextureID�j���m�ۂ��� EditorContext �ɗ�������
      4) RequestResize(w, h, dt)
         - UI ������]����\���s�N�Z���T�C�Y���f�o�E���X�t���Ŏ�t�i�����������ł͊m�肵�Ȃ��j
         - ���ۂ� RT �Đ����͎��t���[���� BeginFrame �ōs��

    �|�C���g�F
      - ���T�C�Y�������f���ƃh���b�O���� RT �č쐬���A�����ăJ�N�����߁A
        Viewports ���� StepSnap + ��莞�Ԃ̈����҂��Ă���m�肷��B
      - �Â� RT �� GPU ���܂��g���Ă���\��������̂ŁADetach��GpuGarbageQueue �ň��S�ɔj���B
      - Game �r���[�́u�p��(View)�͌Œ�A���e(Proj)�����A�X�y�N�g�ɍ��킹�čX�V�v����d�l�ɑΉ��ς݁B
*/

/// �R�}���h�L�^���ɕK�v�ȓ��͑��i���t������ēn���j
struct SceneLayerBeginArgs {
    ID3D12Device* device = nullptr;  // �f�o�C�X�i�K�v�ɉ����� Viewports �Ŏg�p�j
    ID3D12GraphicsCommandList* cmd = nullptr; // �L�^��̃R�}���h���X�g�i�K�{�j
    unsigned                  frameIndex = 0;    // �t���[�������O�o�b�t�@�̃C���f�b�N�X
    class Scene* scene = nullptr;   // �`��ΏۃV�[���i���[�g����ċA�j
    class CameraComponent* camera = nullptr;  // Scene �r���[�p�̎Q�ƃJ����
};

class SceneLayer {
public:
    // ------------------------------------------------------------------------
    // Initialize�i�����F�����E�B���h�E�T�C�Y��n���Łj
    //  - ����� RT ���E�B���h�E���T�C�Y�ō쐬���āu�g�厞�̈ꎞ�{�P�v��}����B
    //  - frames : �t���[���ʂ̃A�b�v���[�h CB �ȂǁiSceneRenderer ���g�p�j
    //  - pipe   : ���[�g�V�O�l�`��/PSO�iSceneRenderer ���g�p�j
    // ------------------------------------------------------------------------
    void Initialize(ID3D12Device* dev, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
        FrameResources* frames, const PipelineSet& pipe,
        unsigned initW, unsigned initH);

    // �݊��F�� 5 �����Łi�T�C�Y�w��Ȃ� �� �����߂̉��T�C�Y�ō쐬�j
    void Initialize(ID3D12Device* dev, DXGI_FORMAT rtvFmt, DXGI_FORMAT dsvFmt,
        FrameResources* frames, const PipelineSet& pipe)
    {
        Initialize(dev, rtvFmt, dsvFmt, frames, pipe, 64, 64);
    }

    // ------------------------------------------------------------------------
    // BeginFrame
    //  - Viewports ���ێ����Ă���u�ۗ����̃��T�C�Y�v���v�������Ŋm��E�K�p�B
    //  - �V���� RT ����蒼���A�Â� RT�i�ő�1�Ԃ�j���Ăяo�����֕Ԃ��B
    //    �� �Ăяo������ FrameScheduler::EndFrame �Œx���j���o�^���邱�ƁB
    // ------------------------------------------------------------------------
    RenderTargetHandles BeginFrame(ID3D12Device* dev);

    // ------------------------------------------------------------------------
    // Record
    //  - Scene / Game �̗������_�[�^�[�Q�b�g�֕`��B
    //  - maxObjects�F1�t���[���ɐς߂�萔�o�b�t�@�X���b�g���iScene �� Game �ŕ����g�p�j�B
    // ------------------------------------------------------------------------
    void Record(const SceneLayerBeginArgs& args, unsigned maxObjects);

    // ------------------------------------------------------------------------
    // FeedToUI
    //  - ImGui �\���p�� SRV�iImTextureID�j���m�ۂ��� EditorContext �ɏ����o���B
    //  - sceneSrvBase / gameSrvBase�F�t���[���ʂ� SRV �X���b�g���Փ˂��Ȃ��悤�x�[�X�����炷�B
    // ------------------------------------------------------------------------
    void FeedToUI(EditorContext& ctx, ImGuiLayer* imgui,
        unsigned frameIndex, unsigned sceneSrvBase, unsigned gameSrvBase);

    // ------------------------------------------------------------------------
    // RequestResize
    //  - UI ����n���ꂽ�uScene �r���[�|�[�g�̊�]�T�C�Y�v���L�^���邾���i�m��͎��t���j�B
    //  - dt ��ώZ���Ĉ�莞�Ԉ��肵����y���f�B���O���m��iViewports ���̎����j�B
    // ------------------------------------------------------------------------
    void RequestResize(unsigned wantW, unsigned wantH, float dt);

    // ------------------------------------------------------------------------
    // SyncStatsTo
    //  - Viewports �����u���ۂ� RT �̌��݃T�C�Y�v�� EditorContext �ɔ��f�B
    //  - UI ���̕\����f�o�b�O�Ɏg�p�i�E�B���h�E/�X���b�v�`�F�C���̃T�C�Y�Ƃ͋�ʁj�B
    // ------------------------------------------------------------------------
    void SyncStatsTo(EditorContext& ctx) const;

    // ------------------------------------------------------------------------
    // Shutdown
    //  - ������ RenderTarget �Q�Ƃ�؂�A�K�v�ɉ����Ēx���j���L���[�֍ڂ���O��̃f�^�b�`�B
    // ------------------------------------------------------------------------
    void Shutdown();

    // ------------------------------------------------------------------------
    // TakeCarryOverDead
    //  - BeginFrame �ŕԂ�����Ȃ������u����1�g�̌Â� RT�i�����z���j�v���������B
    //    Renderer ���� �g���t���Ŏ̂ĕ����Ȃ������ꍇ�̂݁h �������p�r��z��B
    // ------------------------------------------------------------------------
    RenderTargetHandles TakeCarryOverDead();

    // �i�I�v�V�����j���݂� RT ���T�C�Y
    unsigned SceneWidth()  const { return m_viewports.SceneWidth(); }
    unsigned SceneHeight() const { return m_viewports.SceneHeight(); }
    unsigned GameWidth()   const { return m_viewports.GameWidth(); }
    unsigned GameHeight()  const { return m_viewports.GameHeight(); }

private:
    Viewports     m_viewports;     // RT �̐���/���T�C�Y/ImGui SRV �����EGame�J�����ێ��Ȃ�
    SceneRenderer m_sceneRenderer; // ���b�V���`��iCB ��������/PSO/RS �ݒ�j
};
