// D3D12Renderer.h
#pragma once

#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include <functional>

#include "Scene/Scene.h"
#include "Components/CameraComponent.h"
#include "Scene/GameObject.h"
#include "Assets/Mesh.h"
#include "Components/MeshRendererComponent.h"
#include "SceneConstantBuffer.h"

// ���ʃ��W���[��
#include "Core/DeviceResources.h"           // �f�o�C�X/�X���b�v�`�F�C��
#include "Core/FrameResources.h"            // �t���[�������O�iUpload CB ���j
#include "Pipeline/PipelineStateBuilder.h"  // PipelineSet ��`
#include "Editor/EditorContext.h"           // �G�f�B�^ UI �󂯓n��
#include "Editor/ImGuiLayer.h"              // ImGui ������/�`��
#include "Core/RenderTarget.h"              // �I�t�X�N���[��RT�Ǘ�
#include "Core/GpuGarbage.h"                // �x���j���L���[

class MeshRendererComponent;

class D3D12Renderer
{
public:
    static const UINT FrameCount = 3;
    static const UINT MaxObjects = 100;

    D3D12Renderer();
    ~D3D12Renderer();

    bool Initialize(HWND hwnd, UINT width, UINT height);
    void Render();
    void Resize(UINT width, UINT height) noexcept;
    void Cleanup();

    void SetScene(std::shared_ptr<Scene> scene) { m_CurrentScene = std::move(scene); }
    void SetCamera(std::shared_ptr<CameraComponent> cam) { m_Camera = std::move(cam); }

    bool CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> meshRenderer);
    void DrawMesh(MeshRendererComponent* meshRenderer);

    void WaitForGPU() noexcept;
    void ReleaseSceneResources();

    UINT GetFrameCount() const { return m_frameCount; }

private:
    // ========= ��{���\�[�X =========
    std::unique_ptr<DeviceResources>                  m_dev;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmd;
    Microsoft::WRL::ComPtr<ID3D12Fence>               m_fence;
    HANDLE                                            m_fenceEvent = nullptr;
    UINT64                                            m_nextFence = 0;

    FrameResources                                    m_frames;
    PipelineSet                                       m_pipe;
    std::unique_ptr<ImGuiLayer>                       m_imgui;

    // �����x���Q��
    std::shared_ptr<Scene>                            m_CurrentScene;
    std::shared_ptr<CameraComponent>                  m_Camera;

    // Editor ���
    bool                                              m_IsEditor = true;
    std::weak_ptr<GameObject>                         m_Selected;

    // ���v
    UINT                                              m_frameCount = 0;

    // ========= �I�t�X�N���[���iRenderTarget���j =========
    RenderTarget                                      m_sceneRT;   // �G�f�B�^��Scene�r���[�`���
    RenderTarget                                      m_gameRT;    // �Œ�J������Game�r���[�`���
    UINT                                              m_pendingSceneRTW = 0;
    UINT                                              m_pendingSceneRTH = 0;

    // ImGui SRV�̃x�[�X�X���b�g�i�t���[�����ƂɎg�������j
    static constexpr UINT                             kSceneSrvBase = 16;
    static constexpr UINT                             kGameSrvBase = 32;

    // Scene�́g��ˉe�h�i����FOV��j��ێ�
    DirectX::XMFLOAT4X4 m_sceneProjInit{};
    bool                m_sceneProjCaptured = false;


    // ========= Game�p�F�����Scene�J�������Œ� =========
    bool                                              m_gameCamFrozen = false;
    DirectX::XMFLOAT4X4                               m_gameViewInit{};
    DirectX::XMFLOAT4X4                               m_gameProjInit{};
    float                                             m_gameFrozenAspect = 1.0f;

    // ========= �x���j���L���[ =========
    GpuGarbageQueue                                   m_garbage;

    // Scene�r���[�̃f�o�E���X�p
    UINT                                              m_wantSceneW = 0, m_wantSceneH = 0;
    int                                               m_sceneSizeStable = 0;

    // �������[�e�B���e�B
    void RequestSceneRTResize(UINT w, UINT h) { m_pendingSceneRTW = w; m_pendingSceneRTH = h; }

    // ===== ���ʕ`��p�X(1�J������1RT) =====
    struct CameraMatrices
    {
        DirectX::XMMATRIX view;
        DirectX::XMMATRIX proj;
    };

    void DrawSceneToRT(RenderTarget& rt, const CameraMatrices& cam, UINT cbBase);
};
