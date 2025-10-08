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

// 下位モジュール
#include "Core/DeviceResources.h"           // デバイス/スワップチェイン/RTV/DSV
#include "Core/FrameResources.h"            // フレームリング（Upload CB等）
#include "Pipeline/PipelineStateBuilder.h"  // PipelineSet 定義
#include "Editor/EditorContext.h"           // エディタ UI 受け渡し
#include "Editor/ImGuiLayer.h"              // ImGui 初期化/描画
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

    // エディタ用ヘルパ
    const char* GONameUTF8(const GameObject* go);
    void DrawHierarchyNode(const std::shared_ptr<GameObject>& go);

private:
    // デバイス/スワップチェイン/RTV/DSV
    std::unique_ptr<DeviceResources>                  m_dev;

    // 共有コマンドリスト（各Frameの CommandAllocator で Reset/Close）
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmd;

    // フェンス
    Microsoft::WRL::ComPtr<ID3D12Fence>               m_fence;
    HANDLE                                            m_fenceEvent = nullptr;
    UINT64                                            m_nextFence = 0;

    // フレームリング（Upload CB）
    FrameResources                                    m_frames;

    // PSOセット（Lambert）
    PipelineSet                                       m_pipe;

    // ImGui
    std::unique_ptr<ImGuiLayer>                       m_imgui;

    // 高レベル参照
    std::shared_ptr<Scene>                            m_CurrentScene;
    std::shared_ptr<CameraComponent>                  m_Camera;

    // Editor 状態
    bool                                              m_IsEditor = true;
    std::weak_ptr<GameObject>                         m_Selected;

    // 統計
    UINT                                              m_frameCount = 0;

    // ==== Scene オフスクリーン ====
    Microsoft::WRL::ComPtr<ID3D12Resource>            m_sceneColor;
    Microsoft::WRL::ComPtr<ID3D12Resource>            m_sceneDepth;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      m_offscreenRTVHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      m_offscreenDSVHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE                       m_sceneRTV{};
    D3D12_CPU_DESCRIPTOR_HANDLE                       m_sceneDSV{};
    DXGI_FORMAT                                       m_offscreenFmt = DXGI_FORMAT_R8G8B8A8_UNORM; // 必要なら SRGB
    ImTextureID                                       m_sceneTexId = 0;  // ImGuiに渡すSRV(GPUハンドル)
    UINT                                              m_sceneRTW = 0, m_sceneRTH = 0;             // 現在のサイズ
    UINT                                              m_pendingSceneRTW = 0, m_pendingSceneRTH = 0;// 次フレームで適用する要求
    D3D12_RESOURCE_STATES                             m_sceneState = D3D12_RESOURCE_STATE_COMMON;

    void CreateOffscreen(UINT w, UINT h);
    void ReleaseOffscreen();
    void RequestSceneRTResize(UINT w, UINT h) { m_pendingSceneRTW = w; m_pendingSceneRTH = h; }

    // ==== Game オフスクリーン（固定カメラ、ズームはUIのみ） ====
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

    // 固定カメラ（初期カメラを凍結）
    bool                                              m_gameCamFrozen = false;
    DirectX::XMFLOAT4X4                               m_gameViewInit{};
    DirectX::XMFLOAT4X4                               m_gameProjInit{};

    void CreateGameOffscreen(UINT w, UINT h);
    void ReleaseGameOffscreen();

    float m_gameFrozenAspect = 1.0f;
};
