// D3D12Renderer.h
#pragma once

/*
================================================================================
D3D12Renderer
--------------------------------------------------------------------------------
役割（高レベル）：
  - アプリ側が触る「レンダラー」本体の公開 API を提供
  - 初期化 / フレーム描画 / リサイズ / 終了処理 のライフサイクルを司る
  - 低レベル（DeviceResources, FrameResources, Fence…）と
    高レベル（Scene, Camera, ImGui, Offscreen RT…）の橋渡し

設計の柱：
  - 低レベルの DX12 リソース管理は Core::* に委譲（SRP）
  - 実描画の大半は SceneLayer（SceneRenderer + Viewports）へ委譲
  - コマンド提出・Present・フェンス同期は FrameScheduler に委譲
  - Editor(UI) は ImGuiLayer, EditorContext を介して疎結合に

よく見る落とし穴：
  - Resize: スワップチェインだけではなく、カメラのアスペクトも更新すること
  - フェンス：外部で Signal される可能性を考慮し、単調増加を崩さない
  - ImGui: NewFrame → Build → Render の順序、かつ描画は BB に対して行う
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

// ---- エンジン上位（シーンまわり） ----
#include "Scene/Scene.h"
#include "Components/CameraComponent.h"
#include "Scene/GameObject.h"

// ---- データ（メッシュ） ----
#include "Assets/Mesh.h"
#include "Components/MeshRendererComponent.h"

// ---- 定数バッファ（GPU 側と一致させる）----
#include "SceneConstantBuffer.h"

// ---- 下位モジュール（DX12 ユーティリティ群）----
#include "Core/DeviceResources.h"           // デバイス/スワップチェイン/RTV/DSV
#include "Core/FrameResources.h"            // フレームリング（Upload CB, CmdAlloc）
#include "Core/GpuGarbage.h"                // 遅延破棄キュー（フェンス到達後に解放）
#include "Pipeline/PipelineStateBuilder.h"  // RootSig/PSO 構築（Lambert）
#include "Editor/EditorContext.h"           // エディタ UI とのデータ受け渡し
#include "Editor/ImGuiLayer.h"              // ImGui 初期化/描画
#include "Renderer/Presenter.h"             // BB 遷移・クリア・設定
#include "Renderer/SceneLayer.h"            // オフスクリーン描画（Scene/Game）一式

// ---- スケジューラ（提出/Present/フェンス管理）----
#include "Renderer/FrameScheduler.h"

class MeshRendererComponent;

/*
--------------------------------------------------------------------------------
D3D12Renderer
--------------------------------------------------------------------------------
公開インタフェースの意図：
  - Initialize(hwnd, w, h)  … DX12/ImGui/パイプラインなど一式構築
  - Render()                … 1フレーム分の描画（UI 含む）
  - Resize(w, h)            … スワップチェインのリサイズ（ウィンドウサイズ変更時）
  - Cleanup()               … GPU 待機→リソース解放
  - SetScene/SetCamera      … 今フレーム描画対象のシーン/カメラを差し替え
  - CreateMeshRendererResources … MeshRenderer 用 VB/IB を作成（簡易ユーティリティ）

内部構造の概要：
  - DeviceResources : Device / SwapChain / RTV / DSV / Queue を保持
  - FrameResources  : CmdAllocator / Upload CB / 各フレームの fence 値
  - FrameScheduler  : BeginFrame() / EndFrame() で CmdList 管理＆Present
  - SceneLayer      : Viewports + SceneRenderer（Scene/Game の 2 RT に描画）
  - Presenter       : BB を RT に遷移して ImGui を描画→Present 遷移
--------------------------------------------------------------------------------
*/
class D3D12Renderer
{
public:
    // フレーム数（SwapChain バッファ数と整合させる）
    static const UINT FrameCount = 3;
    // 1フレームにアップロードする最大オブジェクト数（CBV スロット分）
    static const UINT MaxObjects = 100;

    D3D12Renderer();
    ~D3D12Renderer();

    // ---- ライフサイクル ----
    bool Initialize(HWND hwnd, UINT width, UINT height);
    void Render();
    void Resize(UINT width, UINT height) noexcept;
    void Cleanup();

    // ---- シーン／カメラの差し替え ----
    void SetScene(std::shared_ptr<Scene> scene) { m_CurrentScene = std::move(scene); }
    void SetCamera(std::shared_ptr<CameraComponent> cam) { m_Camera = std::move(cam); }

    // ---- メッシュ（簡易アップローダ）----
    //  ・MeshRendererComponent に含まれる CPU メッシュを元に VB/IB を作成
    bool CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> meshRenderer);

    //  ・実際のドローは Render() 内で SceneRenderer が行うが、
    //    単発描画を行いたい場合などに使用（VB/IB/トポロジ設定＋ DrawIndexed）
    void DrawMesh(MeshRendererComponent* meshRenderer);

    // ---- GPU 完全待機（デバッグ/終了時など）----
    void WaitForGPU() noexcept;

    // ---- シーン破棄時の GPU リソース解放（VB/IB など）----
    void ReleaseSceneResources();

    // ---- 統計：経過フレーム数 ----
    UINT GetFrameCount() const { return m_frameCount; }

private:
    // ========= 基本 DX12 リソース =========
    std::unique_ptr<DeviceResources>        m_dev;          // Device / Queue / SwapChain / RTV / DSV
    Microsoft::WRL::ComPtr<ID3D12Fence>     m_fence;        // WaitForGPU 用のフェンス（スケジューラとは別管理）
    HANDLE                                  m_fenceEvent = nullptr;
    UINT64                                  m_nextFence = 0; // WaitForGPU 用（単純なシグナル待ちに使用）

    // フレームリング（各フレームの CmdAllocator / Upload CB / Fence 値など）
    FrameResources                          m_frames;

    // Lambert 用の RootSig / PSO 一式
    PipelineSet                             m_pipe;

    // ImGui の初期化・描画ヘルパ
    std::unique_ptr<ImGuiLayer>             m_imgui;

    // ========= 高レベル参照 =========
    std::shared_ptr<Scene>                  m_CurrentScene; // 今回描画するシーン
    std::shared_ptr<CameraComponent>        m_Camera;       // 今回使用するカメラ

    // ========= Editor 状態 =========
    bool                                    m_IsEditor = true;
    std::weak_ptr<GameObject>               m_Selected;     // Inspector/Hiearchy の選択対象

    // ========= 統計 =========
    UINT                                    m_frameCount = 0;

    // ========= 提出＆Present スケジューラ =========
    FrameScheduler                          m_scheduler;

    // ========= 画面出力 / シーン描画 =========
    Presenter                               m_presenter;    // BB への遷移・クリア・戻し
    SceneLayer                              m_sceneLayer;   // Scene/Game のオフスクリーン描画管理

    // ImGui SRV のベーススロット（フレーム毎に +frameIndex で使い分け）
    static constexpr UINT kSceneSrvBase = 16;               // Scene ビューポート表示用
    static constexpr UINT kGameSrvBase = 32;               // Game  ビューポート表示用

    // ========= 遅延破棄（フェンス到達後に安全に解放）=========
    GpuGarbageQueue                         m_garbage;
};
