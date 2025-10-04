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
#include "Core/DeviceResources.h"        // デバイス/スワップチェイン/RTV/DSV
#include "Core/FrameResources.h"         // フレームリング（Upload CB等）
#include "Pipeline/PipelineStateBuilder.h"   // ★ ここから PipelineSet を参照する（定義はこっちだけ）
#include "Editor/EditorContext.h"          // エディタ UI 受け渡し
#include "Editor/ImGuiLayer.h"             // ImGui 初期化/描画

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
    // デバイス/スワップチェイン/RTV/DSV を一手に持つ下位ラッパ
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
    PipelineSet                                       m_pipe;  // ← PipelineStateBuilder.h の定義を使う

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
};
