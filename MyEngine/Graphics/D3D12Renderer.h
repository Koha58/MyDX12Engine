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
#include "Core/DeviceResources.h"           // �f�o�C�X/�X���b�v�`�F�C��/RTV/DSV
#include "Core/FrameResources.h"            // �t���[�������O�iUpload CB���j
#include "Pipeline/PipelineStateBuilder.h"  // PipelineSet ��`
#include "Editor/EditorContext.h"           // �G�f�B�^ UI �󂯓n��
#include "Editor/ImGuiLayer.h"              // ImGui ������/�`��
#include "Core/RendererTarget.h"

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

    // �G�f�B�^�p�w���p
    const char* GONameUTF8(const GameObject* go);
    void DrawHierarchyNode(const std::shared_ptr<GameObject>& go);

private:
    // �f�o�C�X/�X���b�v�`�F�C��/RTV/DSV
    std::unique_ptr<DeviceResources>                  m_dev;

    // ���L�R�}���h���X�g�i�eFrame�� CommandAllocator �� Reset/Close�j
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmd;

    // �t�F���X
    Microsoft::WRL::ComPtr<ID3D12Fence>               m_fence;
    HANDLE                                            m_fenceEvent = nullptr;
    UINT64                                            m_nextFence = 0;

    // �t���[�������O�iUpload CB�j
    FrameResources                                    m_frames;

    // PSO�Z�b�g�iLambert�j
    PipelineSet                                       m_pipe;

    // ImGui
    std::unique_ptr<ImGuiLayer>                       m_imgui;

    // �����x���Q��
    std::shared_ptr<Scene>                            m_CurrentScene;
    std::shared_ptr<CameraComponent>                  m_Camera;

    // Editor ���
    bool                                              m_IsEditor = true;
    std::weak_ptr<GameObject>                         m_Selected;

    // ���v
    UINT                                              m_frameCount = 0;

    // ==== Scene �I�t�X�N���[�� ====
    Microsoft::WRL::ComPtr<ID3D12Resource>            m_sceneColor;
    Microsoft::WRL::ComPtr<ID3D12Resource>            m_sceneDepth;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      m_offscreenRTVHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      m_offscreenDSVHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE                       m_sceneRTV{};
    D3D12_CPU_DESCRIPTOR_HANDLE                       m_sceneDSV{};
    DXGI_FORMAT                                       m_offscreenFmt = DXGI_FORMAT_R8G8B8A8_UNORM; // �K�v�Ȃ� SRGB
    ImTextureID                                       m_sceneTexId = 0;  // ImGui�ɓn��SRV(GPU�n���h��)
    UINT                                              m_sceneRTW = 0, m_sceneRTH = 0;             // ���݂̃T�C�Y
    UINT                                              m_pendingSceneRTW = 0, m_pendingSceneRTH = 0;// ���t���[���œK�p����v��
    D3D12_RESOURCE_STATES                             m_sceneState = D3D12_RESOURCE_STATE_COMMON;

    void CreateOffscreen(UINT w, UINT h);
    void ReleaseOffscreen();
    void RequestSceneRTResize(UINT w, UINT h) { m_pendingSceneRTW = w; m_pendingSceneRTH = h; }

    // ==== Game �I�t�X�N���[���i�Œ�J�����A�Y�[����UI�̂݁j ====
    Microsoft::WRL::ComPtr<ID3D12Resource>            m_gameColor;
    Microsoft::WRL::ComPtr<ID3D12Resource>            m_gameDepth;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      m_gameRTVHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      m_gameDSVHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE                       m_gameRTV{};
    D3D12_CPU_DESCRIPTOR_HANDLE                       m_gameDSV{};
    DXGI_FORMAT                                       m_gameColorFmt = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT                                       m_gameDepthFmt = DXGI_FORMAT_D32_FLOAT;
    ImTextureID                                       m_gameTexId = 0;
    UINT                                              m_gameRTW = 0, m_gameRTH = 0;
    D3D12_RESOURCE_STATES                             m_gameState = D3D12_RESOURCE_STATE_COMMON;

    // �Œ�J�����i�����J�����𓀌��j
    bool                                              m_gameCamFrozen = false;
    DirectX::XMFLOAT4X4                               m_gameViewInit{};
    DirectX::XMFLOAT4X4                               m_gameProjInit{};

    void CreateGameOffscreen(UINT w, UINT h);
    void ReleaseGameOffscreen();

    float m_gameFrozenAspect = 1.0f;
};
