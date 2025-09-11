#pragma once // 同一ヘッダの多重インクルード防止（MSVC/GCC/Clang）

// ============================== 依存ヘッダ ===============================
// D3D12 は COM ベース。Microsoft::WRL::ComPtr で参照カウントを自動管理。
#include <windows.h>        // HWND, HANDLE, Win32 基本型
#include <d3d12.h>          // D3D12 コア API
#include <dxgi1_6.h>        // DXGI（SwapChain/Adapter 列挙）※ IDXGISwapChain4 用
#include <wrl/client.h>     // Microsoft::WRL::ComPtr（COM スマートポインタ）
#include <DirectXMath.h>    // 行列/ベクトル（SIMD ベース）
#include <vector>
#include <memory>
#include <functional>

#include "GameObject.h"            // シーンツリーのノード
#include "Mesh.h"                  // MeshData / Vertex 型 など
#include "Scene.h"                 // ルート GameObject の集合
#include "MeshRendererComponent.h" // メッシュ描画用コンポーネント
#include "CameraComponent.h"       // View/Projection を提供
#include "SceneConstantBuffer.h"   // HLSL cbuffer に対応する CPU 側構造体
#include "d3dx12.h"                // CD3DX12_* ユーティリティ（ヘルパ）
// =======================================================================

// =======================================================================
// D3D12Renderer
//  - D3D12 の初期化/後始末、フレーム毎のコマンド記録、スワップ、同期を担当。
//  - シーン（Scene）とカメラ（CameraComponent）を受け取り描画する。
//  - ポリシー：このクラスは「低レベル描画制御」に集中。ゲームロジックは保持しない。
// =======================================================================
class D3D12Renderer
{
public:
    // ----------------------------- フレーム二重化 -----------------------------
    // バッファ枚数。2=ダブルバッファ。将来は 3 にすると Present 待ちが減る場合あり。
    static const UINT FrameCount = 2;

    // ----------------------------- ライフサイクル -----------------------------
    D3D12Renderer();
    ~D3D12Renderer(); // Cleanup() 経由で安全に破棄（GPU 完了待ち込み）

    // === Public API（処理順）=================================================
    // 1) Initialize:
    //   - デバイス/キュー/スワップチェイン/RTV/DSV/コマンド/PSO を依存順に構築。
    //   - 成功: true / 失敗: false（詳細はデバッグ出力を参照）。
    //   - HWND の寿命は呼び出し側管理（所有しない）。
    bool Initialize(HWND hwnd, UINT width, UINT height);

    // 2) Render:
    //   - コマンドリスト Reset→記録→Close→Execute→Present。
    //   - 終端で WaitForPreviousFrame により CPU/GPU 完全同期（安全重視）。
    //   - シーン/カメラ未設定時は安全にスキップ。
    void Render();

    // 3) Resize:
    //   - スワップチェイン再作成（サイズのみ）→RTV/DSV 作り直し。
    //   - 使用中解放を避けるため、事前に WaitForGPU() で待機。
    void Resize(UINT width, UINT height) noexcept;

    // 4) Cleanup:
    //   - GPU 完了待ち→シーン GPU リソース解放→コマンド/ヒープ/CB/同期/SC/Dev の順で解放。
    //   - 二重呼び出しに対して安全（nullptr/未初期化は無害）。
    void Cleanup();

    // === シーン / カメラ ======================================================
    // 描画対象シーンの設定（共有所有）。切替はフレーム間の安全なタイミングで。
    void SetScene(std::shared_ptr<Scene> scene) { m_CurrentScene = scene; }

    // 使用カメラの設定。アスペクト等の更新は Camera 側で実施して渡す。
    void SetCamera(std::shared_ptr<CameraComponent> camera) { m_Camera = camera; }

    // === メッシュ GPU リソース作成 ==========================================
    // MeshData → Upload ヒープ上の VB/IB を生成し、VBV/IBV/IndexCount を設定。
    // 大量/静的データは将来 Default ヒープ転送版へ置換推奨。
    bool CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> meshRenderer);

    // === 同期・ユーティリティ（必要に応じて外部からも呼べるよう公開） ==========
    // Fence の Signal→到達待ち。frameIndex を Present 後の最新値に更新。
    void WaitForPreviousFrame();

    // 例外を投げない簡易待機。Resize 等「とりあえず待つ」用途。
    void WaitForGPU() noexcept;

    // PSO/ルート/CBV/RT 等が適切に設定済みである前提の即時ドロー。
    void DrawMesh(MeshRendererComponent* meshRenderer);

    // シーン配下の GPU リソース(VB/IB 等)を安全に解放（Resize/Cleanup 内部でも利用）。
    void ReleaseSceneResources();

    // 統計/アニメ制御用：Present 成功回数（経過フレーム数）を返す。
    UINT GetFrameCount() const { return m_frameCount; }

private:
    // === 内部初期化ヘルパ（Initialize から呼ばれる / 失敗時 false を返す） =========
    bool CreateDevice();                         // アダプタ列挙 → D3D12CreateDevice
    bool CreateCommandQueue();                   // Direct コマンドキュー
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height); // Flip Discard スワップチェイン
    bool CreateRenderTargetViews();              // 各バックバッファの RTV 作成
    bool CreateDepthStencilBuffer(UINT width, UINT height);    // D32F テクスチャ + DSV
    bool CreateCommandAllocatorsAndList();       // フレーム別アロケータ + 共有 CL
    bool CreatePipelineState();                  // ルートシグネチャ + PSO 構築

private:
    // ======================== D3D12 主要オブジェクト群 =======================
    // ComPtr はスコープ終了で自動 Release。明示 Release 不要。
    Microsoft::WRL::ComPtr<ID3D12Device>                device;         // 論理デバイス
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>          commandQueue;   // 直接コマンドキュー
    Microsoft::WRL::ComPtr<IDXGISwapChain4>             swapChain;      // バック/フロント切替（IDXGISwapChain4）
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        rtvHeap;        // RTV ヒープ（CPU 可視）
    UINT                                                rtvDescriptorSize = 0; // RTV インクリメント幅
    Microsoft::WRL::ComPtr<ID3D12Resource>              renderTargets[FrameCount]; // バックバッファ群

    // ---- 深度/ステンシル ----
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        dsvHeap;        // DSV ヒープ（CPU 可視）
    Microsoft::WRL::ComPtr<ID3D12Resource>              depthStencilBuffer; // D32F テクスチャ

    // ---- コマンド記録系（フレームごとにアロケータを持つ）----
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      commandAllocators[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>   commandList;    // 1 本運用（Reset/Close で再利用）

    // ---- ルート/パイプライン ----
    Microsoft::WRL::ComPtr<ID3D12RootSignature>         rootSignature;  // リソース可視化レイアウト
    Microsoft::WRL::ComPtr<ID3D12PipelineState>         pipelineState;  // PSO（VS/PS/RS/DS/Blend 等）

    // ---- 同期（CPU-GPU）----
    Microsoft::WRL::ComPtr<ID3D12Fence>                 fence;          // GPU 完了報告
    UINT64                                              fenceValue = 0; // シグナル値（毎フレーム++）
    HANDLE                                              fenceEvent = nullptr; // 待機用 Win32 イベント

    // ---- 定数バッファ（オブジェクト毎）----
    // ・Upload（CPU 可視）に大きめの単一バッファを確保し、各オブジェクトに 256B 単位で割当。
    // ・CBV は ShaderVisible ヒープに事前生成しておき、描画時は GPU ハンドルをオフセット指定。
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        m_cbvHeap;      // CBV/SRV/UAV ヒープ（GPU 可視）
    Microsoft::WRL::ComPtr<ID3D12Resource>              m_constantBuffer; // Upload ヒープ（永続マップ）
    UINT8* m_pCbvDataBegin = nullptr; // Map 先頭アドレス
    UINT                                                m_cbStride = 0;        // 1 オブジェクト分の CB サイズ（256B アライン）
    UINT                                                m_cbvDescriptorSize = 0; // CBV/SRV/UAV インクリメント幅

    // ============================ 状態/サイズ類 ==============================
    UINT                                                m_Width = 0;   // クライアント領域の幅（px）
    UINT                                                m_Height = 0;   // クライアント領域の高さ（px）
    UINT                                                frameIndex = 0; // 現バックバッファ index（Present 後に更新）
    UINT                                                m_frameCount = 0; // Present 回数（統計/アニメ制御など）

    // ============================ 高レベル参照 ===============================
    // Renderer は Scene/Camera の「利用者」。寿命は shared_ptr で共有するだけ。
    std::shared_ptr<Scene>                              m_CurrentScene; // 描画対象シーン
    std::shared_ptr<CameraComponent>                    m_Camera;       // 使用カメラ（View/Proj 供給源）

    // ============================ 将来の拡張（残置） =========================
    // ※ 互換維持のため残置。実使用は m_Camera から取得を推奨。
    DirectX::XMMATRIX                                   m_ViewMatrix;        // m_Camera->GetViewMatrix() を推奨
    DirectX::XMMATRIX                                   m_ProjectionMatrix;  // 同上
};
