// D3D12Renderer.h
#pragma once

/*
================================================================================
D3D12Renderer
--------------------------------------------------------------------------------
�����i�����x���j�F
  - �A�v�������G��u�����_���[�v�{�̂̌��J API ���
  - ������ / �t���[���`�� / ���T�C�Y / �I������ �̃��C�t�T�C�N�����i��
  - �჌�x���iDeviceResources, FrameResources, Fence�c�j��
    �����x���iScene, Camera, ImGui, Offscreen RT�c�j�̋��n��

�݌v�̒��F
  - �჌�x���� DX12 ���\�[�X�Ǘ��� Core::* �ɈϏ��iSRP�j
  - ���`��̑唼�� SceneLayer�iSceneRenderer + Viewports�j�ֈϏ�
  - �R�}���h��o�EPresent�E�t�F���X������ FrameScheduler �ɈϏ�
  - Editor(UI) �� ImGuiLayer, EditorContext ����đa������

�悭���闎�Ƃ����F
  - Resize: �X���b�v�`�F�C�������ł͂Ȃ��A�J�����̃A�X�y�N�g���X�V���邱��
  - �t�F���X�F�O���� Signal �����\�����l�����A�P������������Ȃ�
  - ImGui: NewFrame �� Build �� Render �̏����A���`��� BB �ɑ΂��čs��
================================================================================
*/

#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include <functional>

// ---- �G���W����ʁi�V�[���܂��j ----
#include "Scene/Scene.h"
#include "Components/CameraComponent.h"
#include "Scene/GameObject.h"

// ---- �f�[�^�i���b�V���j ----
#include "Assets/Mesh.h"
#include "Components/MeshRendererComponent.h"

// ---- �萔�o�b�t�@�iGPU ���ƈ�v������j----
#include "SceneConstantBuffer.h"

// ---- ���ʃ��W���[���iDX12 ���[�e�B���e�B�Q�j----
#include "Core/DeviceResources.h"           // �f�o�C�X/�X���b�v�`�F�C��/RTV/DSV
#include "Core/FrameResources.h"            // �t���[�������O�iUpload CB, CmdAlloc�j
#include "Core/GpuGarbage.h"                // �x���j���L���[�i�t�F���X���B��ɉ���j
#include "Pipeline/PipelineStateBuilder.h"  // RootSig/PSO �\�z�iLambert�j
#include "Editor/EditorContext.h"           // �G�f�B�^ UI �Ƃ̃f�[�^�󂯓n��
#include "Editor/ImGuiLayer.h"              // ImGui ������/�`��
#include "Renderer/Presenter.h"             // BB �J�ځE�N���A�E�ݒ�
#include "Renderer/SceneLayer.h"            // �I�t�X�N���[���`��iScene/Game�j�ꎮ

// ---- �X�P�W���[���i��o/Present/�t�F���X�Ǘ��j----
#include "Renderer/FrameScheduler.h"

class MeshRendererComponent;

/*
--------------------------------------------------------------------------------
D3D12Renderer
--------------------------------------------------------------------------------
���J�C���^�t�F�[�X�̈Ӑ}�F
  - Initialize(hwnd, w, h)  �c DX12/ImGui/�p�C�v���C���Ȃǈꎮ�\�z
  - Render()                �c 1�t���[�����̕`��iUI �܂ށj
  - Resize(w, h)            �c �X���b�v�`�F�C���̃��T�C�Y�i�E�B���h�E�T�C�Y�ύX���j
  - Cleanup()               �c GPU �ҋ@�����\�[�X���
  - SetScene/SetCamera      �c ���t���[���`��Ώۂ̃V�[��/�J�����������ւ�
  - CreateMeshRendererResources �c MeshRenderer �p VB/IB ���쐬�i�ȈՃ��[�e�B���e�B�j

�����\���̊T�v�F
  - DeviceResources : Device / SwapChain / RTV / DSV / Queue ��ێ�
  - FrameResources  : CmdAllocator / Upload CB / �e�t���[���� fence �l
  - FrameScheduler  : BeginFrame() / EndFrame() �� CmdList �Ǘ���Present
  - SceneLayer      : Viewports + SceneRenderer�iScene/Game �� 2 RT �ɕ`��j
  - Presenter       : BB �� RT �ɑJ�ڂ��� ImGui ��`�恨Present �J��
--------------------------------------------------------------------------------
*/
class D3D12Renderer
{
public:
    // �t���[�����iSwapChain �o�b�t�@���Ɛ���������j
    static const UINT FrameCount = 3;
    // 1�t���[���ɃA�b�v���[�h����ő�I�u�W�F�N�g���iCBV �X���b�g���j
    static const UINT MaxObjects = 100;

    D3D12Renderer();
    ~D3D12Renderer();

    // ---- ���C�t�T�C�N�� ----
    bool Initialize(HWND hwnd, UINT width, UINT height);
    void Render();
    void Resize(UINT width, UINT height) noexcept;
    void Cleanup();

    // ---- �V�[���^�J�����̍����ւ� ----
    void SetScene(std::shared_ptr<Scene> scene) { m_CurrentScene = std::move(scene); }
    void SetCamera(std::shared_ptr<CameraComponent> cam) { m_Camera = std::move(cam); }

    // ---- ���b�V���i�ȈՃA�b�v���[�_�j----
    //  �EMeshRendererComponent �Ɋ܂܂�� CPU ���b�V�������� VB/IB ���쐬
    bool CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> meshRenderer);

    //  �E���ۂ̃h���[�� Render() ���� SceneRenderer ���s�����A
    //    �P���`����s�������ꍇ�ȂǂɎg�p�iVB/IB/�g�|���W�ݒ�{ DrawIndexed�j
    void DrawMesh(MeshRendererComponent* meshRenderer);

    // ---- GPU ���S�ҋ@�i�f�o�b�O/�I�����Ȃǁj----
    void WaitForGPU() noexcept;

    // ---- �V�[���j������ GPU ���\�[�X����iVB/IB �Ȃǁj----
    void ReleaseSceneResources();

    // ---- ���v�F�o�߃t���[���� ----
    UINT GetFrameCount() const { return m_frameCount; }

private:
    // ========= ��{ DX12 ���\�[�X =========
    std::unique_ptr<DeviceResources>        m_dev;          // Device / Queue / SwapChain / RTV / DSV
    Microsoft::WRL::ComPtr<ID3D12Fence>     m_fence;        // WaitForGPU �p�̃t�F���X�i�X�P�W���[���Ƃ͕ʊǗ��j
    HANDLE                                  m_fenceEvent = nullptr;
    UINT64                                  m_nextFence = 0; // WaitForGPU �p�i�P���ȃV�O�i���҂��Ɏg�p�j

    // �t���[�������O�i�e�t���[���� CmdAllocator / Upload CB / Fence �l�Ȃǁj
    FrameResources                          m_frames;

    // Lambert �p�� RootSig / PSO �ꎮ
    PipelineSet                             m_pipe;

    // ImGui �̏������E�`��w���p
    std::unique_ptr<ImGuiLayer>             m_imgui;

    // ========= �����x���Q�� =========
    std::shared_ptr<Scene>                  m_CurrentScene; // ����`�悷��V�[��
    std::shared_ptr<CameraComponent>        m_Camera;       // ����g�p����J����

    // ========= Editor ��� =========
    bool                                    m_IsEditor = true;
    std::weak_ptr<GameObject>               m_Selected;     // Inspector/Hiearchy �̑I��Ώ�

    // ========= ���v =========
    UINT                                    m_frameCount = 0;

    // ========= ��o��Present �X�P�W���[�� =========
    FrameScheduler                          m_scheduler;

    // ========= ��ʏo�� / �V�[���`�� =========
    Presenter                               m_presenter;    // BB �ւ̑J�ځE�N���A�E�߂�
    SceneLayer                              m_sceneLayer;   // Scene/Game �̃I�t�X�N���[���`��Ǘ�

    // ImGui SRV �̃x�[�X�X���b�g�i�t���[������ +frameIndex �Ŏg�������j
    static constexpr UINT kSceneSrvBase = 16;               // Scene �r���[�|�[�g�\���p
    static constexpr UINT kGameSrvBase = 32;               // Game  �r���[�|�[�g�\���p

    // ========= �x���j���i�t�F���X���B��Ɉ��S�ɉ���j=========
    GpuGarbageQueue                         m_garbage;
};
