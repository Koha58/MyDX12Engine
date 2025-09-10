#pragma once // 同一ヘッダの多重インクルード防止（MSVC/GCC/Clang対応の簡易版）

// ============================== 依存ヘッダ ===============================
// ※ D3D12 は COM ベース。Microsoft::WRL::ComPtr で参照カウントを自動管理する。
#include <windows.h>      // HWND, HANDLE, Win32 基本型
#include <d3d12.h>        // D3D12 コア API
#include <dxgi1_4.h>      // スワップチェイン / アダプタ列挙（DXGI 1.4）
#include <wrl/client.h>   // Microsoft::WRL::ComPtr（COM スマートポインタ）
#include <DirectXMath.h>  // 行列/ベクトル（SIMD ベース）
#include <vector>
#include <memory>
#include <functional>

#include "GameObject.h"            // シーンツリーのノード
#include "Mesh.h"                  // MeshData / Vertex 型 など
#include "Scene.h"                 // ルート GameObject の集合と更新/描画の起点
#include "MeshRendererComponent.h" // メッシュ描画用コンポーネント
#include "CameraComponent.h"       // View/Projection を提供
#include "SceneConstantBuffer.h"   // HLSL cbuffer に対応する CPU 側構造体
#include "d3dx12.h"                // D3D12 ヘルパー（CD3DX12_* ユーティリティ）
// =======================================================================

// =======================================================================
// D3D12Renderer
//  - D3D12 の初期化/後始末、フレーム毎のコマンド記録、スワップ、同期を担当。
//  - シーン（Scene）とカメラ（CameraComponent）を受け取って描画を行う。
//  - ポリシー：このクラスは「低レベルの描画制御」に集中し、ゲームロジックは保持しない。
//    * GameObject の巡回やコンポーネント単位の描画呼び出しは Scene / GameObject / Component 側。
//    * Renderer は GPU リソースの作成/束縛/ドローの最小限 API を提供。
// =======================================================================
class D3D12Renderer
{
public:
    // ----------------------------- フレーム二重化 -----------------------------
    // バッファの個数。2=ダブルバッファ。ティアリング軽減と GPU/CPU の並行を確保。
    // ※ 将来的に 3 (トリプル) にすると Present の待ちが減る場合がある。
    static const UINT FrameCount = 2;

    // ----------------------------- ライフサイクル -----------------------------
    D3D12Renderer();
    ~D3D12Renderer(); // Cleanup() を呼んでリソース破棄（ComPtr により安全）

    // 初期化：
    //  - デバイス/キュー/スワップチェイン/RTV/DSV/コマンド関連/パイプライン を構築。
    //  - 成功時 true。失敗時 false（詳細はデバッグ出力）。
    //  - HWND は所有しない（Win32 ウィンドウ寿命は呼び出し側で管理）。
    bool Initialize(HWND hwnd, UINT width, UINT height);

    // フレーム描画：
    //  - コマンドリストを Reset → 記録 → Close → Execute。
    //  - Present → 前フレームの完了待ち（フェンス同期）。
    //  - シーンやカメラが未設定なら早期 return。
    void Render();

    void WaitForGPU() noexcept;

    void Resize(UINT width, UINT height) noexcept;

    // メッシュ単体ドローのヘルパー：
    //  - 事前に IA（VB/IB/トポロジ）をセットし、DrawIndexedInstanced を発行。
    //  - PSO/RS/DS/CBV などは Render() 側の流れで設定されている前提。
    void DrawMesh(MeshRendererComponent* meshRenderer);

    // 終了処理（明示的破棄）：
    //  - GPU 完了待ち → マップ解除 → ハンドル解放（ComPtr は自動 Release）。
    //  - Initialize を繰り返す場合のためにも呼び出し可能にしてある。
    void Cleanup();

    // GPU/CPU 同期：
    //  - 現在のフェンス値を Signal → その値に到達するまで待機。
    //  - フレームリソース（コマンドアロケータ等）のリセット安全化に必須。
    void WaitForPreviousFrame();

    // ----------------------------- シーン/カメラ ------------------------------
    // 描画対象のシーンを設定（共有所有）。
    // ・Renderer は Scene の寿命を保持しない設計でもよいが、ここでは shared_ptr で参照。
    // ・Scene の切替はフレーム間で安全なタイミングで行うこと。
    void SetScene(std::shared_ptr<Scene> scene) { m_CurrentScene = scene; }

    // 使用するカメラを設定：
    // ・View/Projection は CameraComponent から取得。
    // ・アスペクト比などの変更は Camera 側で SetAspect などを呼んで反映させる。
    void SetCamera(std::shared_ptr<CameraComponent> camera) { m_Camera = camera; }

    // メッシュの GPU リソース生成：
    //  - MeshRendererComponent が持つ MeshData から Upload ヒープに VB/IB を作成し、コピー。
    //  - VBV/IBV を埋め、IndexCount を設定。
    //  - 本関数は「CPU メッシュ→GPU バッファ」変換の入り口。テクスチャ等は未対応。
    bool CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> meshRenderer);

    // 経過フレーム数（Present 成功回数）を返す。アニメ/時間ベースの簡易トリガに利用可。
    UINT GetFrameCount() const { return m_frameCount; }

private:
    // ======================== D3D12 主要オブジェクト群 =======================
    // ※ ComPtr は例外安全。スコープ終了で Release。明示 Release は不要。
    Microsoft::WRL::ComPtr<ID3D12Device> device;                // 論理デバイス
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;    // 直接コマンドキュー
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;          // バック/フロントバッファ切替
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;       // RTV 用ヒープ（CPU 可視）
    UINT rtvDescriptorSize = 0;                                  // RTV インクリメント幅
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[FrameCount]; // バックバッファ群

    // ---- 深度/ステンシル ----
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;       // DSV 用ヒープ
    Microsoft::WRL::ComPtr<ID3D12Resource>      depthStencilBuffer; // D32F テクスチャ

    // ---- コマンド記録系（フレームごとにアロケータ）----
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList; // 1 本運用

    // ---- ルート&パイプライン ----
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;  // リソース可視化レイアウト
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;  // PSO（VS/PS/RS/DS/Blend 等）

    // ---- 同期（CPU-GPU）----
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;  // GPU 完了報告
    UINT64 fenceValue = 0;                      // シグナル値（毎フレームインクリメント）
    HANDLE fenceEvent = nullptr;                // 待機用 Win32 イベント（手動リセット不要）

    // ---- 定数バッファ（オブジェクト毎）----
    // ・アップロード（CPU 可視）に大きめの単一バッファを確保し、各オブジェクトにオフセット割当。
    // ・CBV は ShaderVisible ヒープ上に動的作成（描画時に参照/コピー費用がかかる点に注意）。
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvHeap;  // CBV/SRV/UAV ヒープ（GPU 可視）
    Microsoft::WRL::ComPtr<ID3D12Resource>      m_constantBuffer; // アップロードバッファ
    UINT8* m_pCbvDataBegin = nullptr;           // Map した先頭アドレス（Unmap 忘れ注意）

    // ============================ 状態/サイズ類 ==============================
    UINT m_Width = 0;                          // クライアント領域の幅（ピクセル）
    UINT m_Height = 0;                          // クライアント領域の高さ（ピクセル）
    UINT frameIndex = 0;                        // 現在のバックバッファ Index（swapChain 由来）
    UINT m_frameCount = 0;                      // Present 回数（統計/アニメ制御など）

    // 予備（未使用/拡張用）。別コマンドリストを追加したい時などに転用可。
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList;

    // ============================ 内部ヘルパー ==============================
    // 低レベル初期化の分割：失敗時は false を返す（Log は内部で出力）
    bool CreateDevice();                         // アダプタ列挙 → D3D12CreateDevice
    bool CreateCommandQueue();                   // 直接キュー作成（Copy/Compute は未作成）
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height); // フリップ系スワップ
    bool CreateRenderTargetViews();              // バックバッファ → RTV 作成
    bool CreateCommandAllocatorsAndList();       // フレーム別アロケータ + 共有 CL
    bool CreatePipelineState();                  // ルートシグネチャ + PSO 構築

    // 深度ステンシル（D32F）テクスチャ作成：
    //  - DSV ヒープに 1 つ登録。Render() の OMSetRenderTargets で使用。
    bool CreateDepthStencilBuffer(UINT width, UINT height);

    // ============================ 高レベル参照 ===============================
    // Renderer は Scene/Camera の「利用者」。寿命は shared_ptr で共有するだけ。
    std::shared_ptr<Scene>           m_CurrentScene;  // 描画対象シーン
    std::shared_ptr<CameraComponent> m_Camera;        // 使用カメラ（View/Proj の供給源）

    // ============================ 将来の拡張痕跡 ============================
    // ※ 一時的な保持（旧実装の名残）：将来は CameraComponent に集約して参照のみの予定。
    DirectX::XMMATRIX m_ViewMatrix;        // 使用側は m_Camera->GetViewMatrix() を推奨
    DirectX::XMMATRIX m_ProjectionMatrix;  // 同上（SetAspect/FOV は Camera 側で管理）
};

// ============================ 運用/設計メモ ================================
// ・スレッドセーフではない：Initialize/Render/Cleanup はメインスレッド想定。
// ・解像度変更/ウィンドウリサイズ：Resize 対応を追加する場合、DSV/RTV/SwapChain を作り直し。
// ・Descriptor の寿命：Create*View 後は元リソースが有効であることが必要（ComPtr 管理）。
// ・CBV の配置：D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT(256) に合わせてアライン必須。
// ・Present の引数：ティアリング/垂直同期を制御したい場合は適切なフラグに変更。
// ・デバッグ時は D3D12 デバッグレイヤを有効化（CreateDevice 内の _DEBUG ブロック）。
// ============================================================================
