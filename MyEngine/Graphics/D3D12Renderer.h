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
#include "Core/DeviceResources.h"        // �f�o�C�X/�X���b�v�`�F�C��/RTV/DSV
#include "Core/FrameResources.h"         // �t���[�������O�iUpload CB���j
#include "Pipeline/PipelineStateBuilder.h"   // �� �������� PipelineSet ���Q�Ƃ���i��`�͂����������j
#include "Editor/EditorContext.h"          // �G�f�B�^ UI �󂯓n��
#include "Editor/ImGuiLayer.h"             // ImGui ������/�`��

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
    // �f�o�C�X/�X���b�v�`�F�C��/RTV/DSV �����Ɏ����ʃ��b�p
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
    PipelineSet                                       m_pipe;  // �� PipelineStateBuilder.h �̒�`���g��

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
};
