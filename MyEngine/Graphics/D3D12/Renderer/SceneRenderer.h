#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include "Core/RenderTarget.h"              // �I�t�X�N���[��RT�Ǘ��i�J���[/�[�x�ARTV/DSV�A�J�ڃ��[�e�B���e�B�j
#include "Core/FrameResources.h"            // �t���[�������O�iUpload CB ���j
#include "Pipeline/PipelineStateBuilder.h"  // PipelineSet ��`�iRootSignature / PSO�j
#include "Scene/Scene.h"
#include "Scene/GameObject.h"
#include "Graphics/SceneConstantBuffer.h"   // HLSL �ɍ��킹���萔�o�b�t�@�\��
#include "Components/MeshRendererComponent.h"

/*
    SceneRenderer.h
    ----------------------------------------------------------------------------
    �ړI�F
      - �u1�̃J�����s��Z�b�g �� 1���� RenderTarget�v�֕`��R�}���h���L�^����ŏ��̕`����B
      - RootSignature/PSO�iPipelineSet�j�ƁA�e�t���[���̃A�b�v���[�hCB�iFrameResources�j
        ���󂯎���ĕێ����A�^����ꂽ Scene �������`���B

    �z��t���[�i�Ăяo����=Viewports/SceneLayer �Ȃǁj�F
      1) Initialize(dev, pipe, frames)
         - �g�p���� PSO �ƃt���[�������O�iCB�j�ւ̃|�C���^��ێ�
      2) Record(cmd, rt, cam, scene, cbBase, frameIndex, maxObjects)
         - rt �� RT ��Ԃ֑J�� �� �o�C���h/�N���A
         - cam(view/proj) �� Scene ����AGameObject/Component ��H���ă��b�V����`��
         - �萔�o�b�t�@�� FrameResources ��� [cbBase .. cbBase+maxObjects-1] ���g�p

    ���ӓ_�F
      - �ucbBase�v�͕����p�X�œ��� FrameResources �𕪊����p���邽�߂̃I�t�Z�b�g�B
        ��jScene �p�X�� 0..N-1�AGame �p�X�� N..2N-1 �����蓖�Ă�݌v�B
      - �uframeIndex�v�̓t���[�������O�̃C���f�b�N�X�iBackBufferIndex �ƈ�v������ƕ�����₷���j�B
      - Record ���ł� PSO/RootSignature ���Z�b�g����z��B���ۂ� PSO �͌Ăяo�����ŏ㏑���\�B
      - RenderTarget �́ARecord ���̖`���� RT �ւ̑J��/�ݒ�A������ SRV �ւ̑J�ڂ��s�����[�e�B���e�B��
        �Ăяo���i�I�t�X�N���[����ImGui �\���ɔ�����j�B
*/

// ===== ���ʕ`��p�X(1�J������1RT) =====
/** �J�����p�s�񑩁iLH: ����n�z��j */
struct CameraMatrices
{
    DirectX::XMMATRIX view;  ///< View �s��i���[���h���r���[�j
    DirectX::XMMATRIX proj;  ///< Projection �s��i�r���[���N���b�v�j
};

/**
 * @brief �V�[���`��̔����t�@�T�[�h�B
 *        �^����ꂽ RenderTarget �ɑ΂��AScene ���� MeshRenderer �����ɕ`���B
 */
class SceneRenderer
{
public:

    /**
     * @brief �g�p���� PipelineSet �� FrameResources ���֘A�t����B
     * @param dev     �i���g�p�B�����̊g���� RootSig �����Ȃǂ��K�v�ɂȂ����ꍇ�ɔ����ێ��j
     * @param pipe    ���[�g�V�O�l�`��/PSO �̃Z�b�g
     * @param frames  �t���[�������O�i�A�b�v���[�h�萔�o�b�t�@��R�}���h�A���P�[�^�Q�j
     *
     * @note ���� dev �͎g�p���Ă��Ȃ����AInitialize �̓���C���^�[�t�F�[�X�Ƃ��Ď󂯂Ă����B
     */
    void Initialize(ID3D12Device* /*dev*/, const PipelineSet& pipe, FrameResources* frames)
    {
        m_pipe = pipe;
        m_frames = frames;
    }

    /**
     * @brief 1 �J���� �� 1 RenderTarget �֕`��R�}���h���L�^����B
     *
     * @param cmd         �L�^��R�}���h���X�g�iDIRECT�j
     * @param rt          �`��Ώۂ� RenderTarget�i�I�t�X�N���[���j
     * @param cam         �J�����s��iview/proj�j
     * @param scene       �`��Ώ� Scene�i���[�g����ċA����j
     * @param cbBase      FrameResources ��̒萔�o�b�t�@�X���b�g�̊J�n�I�t�Z�b�g
     * @param frameIndex  �t���[�������O�̃C���f�b�N�X�iBackBufferIndex �ɑΉ��j
     * @param maxObjects  ���̃p�X�Ŋm�ۂ��Ă悢 CB �X���b�g���i�K�[�h�p�j
     *
     * @details
     *   - �{���\�b�h�̒��ŁF
     *       1) rt.TransitionToRT(cmd) / Bind(cmd) / Clear(cmd) ���Ă�
     *       2) VP/SC/IA/RS/RootSignature ���Z�b�g���AScene ��H���� MeshRenderer ��`��
     *       3) �萔�o�b�t�@�iSceneConstantBuffer�j�� FrameResources �� Upload �̈�ɏ�������
     *       4) �Ō�� rt.TransitionToSRV(cmd) �� SRV readable �ɖ߂�
     *
     *   - cbBase �� maxObjects �ɂ��A1�t���[�����ŕ����p�X�iScene/Game ���j��
     *     ���� FrameResources �����Ȃ��g����B
     */
    void Record(ID3D12GraphicsCommandList* cmd,
        RenderTarget& rt,
        const CameraMatrices& cam,
        const Scene* scene,
        UINT cbBase,
        UINT frameIndex,
        UINT maxObjects);

private:
    PipelineSet     m_pipe{};        ///< ���[�g�V�O�l�`��/PSO�iLambert ���j
    FrameResources* m_frames = nullptr; ///< �t���[�������O�iUpload CB/�R�}���h�A���P�[�^���j
};
