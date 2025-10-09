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

// 下位モジュール
#include "Core/DeviceResources.h"           // デバイス/スワップチェイン
#include "Core/FrameResources.h"            // フレームリング（Upload CB 等）
#include "Pipeline/PipelineStateBuilder.h"  // PipelineSet 定義
#include "Editor/EditorContext.h"           // エディタ UI 受け渡し
#include "Editor/ImGuiLayer.h"              // ImGui 初期化/描画
#include "Core/RenderTarget.h"              // オフスクリーンRT管理
#include "Core/GpuGarbage.h"                // 遅延破棄キュー

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
    // ========= 基本リソース =========
    std::unique_ptr<DeviceResources>                  m_dev;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmd;
    Microsoft::WRL::ComPtr<ID3D12Fence>               m_fence;
    HANDLE                                            m_fenceEvent = nullptr;
    UINT64                                            m_nextFence = 0;

    FrameResources                                    m_frames;
    PipelineSet                                       m_pipe;
    std::unique_ptr<ImGuiLayer>                       m_imgui;

    // 高レベル参照
    std::shared_ptr<Scene>                            m_CurrentScene;
    std::shared_ptr<CameraComponent>                  m_Camera;

    // Editor 状態
    bool                                              m_IsEditor = true;
    std::weak_ptr<GameObject>                         m_Selected;

    // 統計
    UINT                                              m_frameCount = 0;

    // ========= オフスクリーン（RenderTarget化） =========
    RenderTarget                                      m_sceneRT;   // エディタのSceneビュー描画先
    RenderTarget                                      m_gameRT;    // 固定カメラのGameビュー描画先
    UINT                                              m_pendingSceneRTW = 0;
    UINT                                              m_pendingSceneRTH = 0;

    // ImGui SRVのベーススロット（フレームごとに使い分け）
    static constexpr UINT                             kSceneSrvBase = 16;
    static constexpr UINT                             kGameSrvBase = 32;

    // Sceneの“基準射影”（水平FOV基準）を保持
    DirectX::XMFLOAT4X4 m_sceneProjInit{};
    bool                m_sceneProjCaptured = false;


    // ========= Game用：初回にSceneカメラを固定 =========
    bool                                              m_gameCamFrozen = false;
    DirectX::XMFLOAT4X4                               m_gameViewInit{};
    DirectX::XMFLOAT4X4                               m_gameProjInit{};
    float                                             m_gameFrozenAspect = 1.0f;

    // ========= 遅延破棄キュー =========
    GpuGarbageQueue                                   m_garbage;

    // Sceneビューのデバウンス用
    UINT                                              m_wantSceneW = 0, m_wantSceneH = 0;
    int                                               m_sceneSizeStable = 0;

    // 内部ユーティリティ
    void RequestSceneRTResize(UINT w, UINT h) { m_pendingSceneRTW = w; m_pendingSceneRTH = h; }

    // ===== 共通描画パス(1カメラ→1RT) =====
    struct CameraMatrices
    {
        DirectX::XMMATRIX view;
        DirectX::XMMATRIX proj;
    };

    void DrawSceneToRT(RenderTarget& rt, const CameraMatrices& cam, UINT cbBase);
};
