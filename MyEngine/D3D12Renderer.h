#pragma once

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h> // Microsoft::WRL::ComPtr
#include <DirectXMath.h> // DirectX::XMFLOAT4X4 など
#include <vector>
#include <memory> // std::shared_ptr
#include <functional> // std::function のために追加

// 追加
#include "GameObject.h" // GameObjectクラスをインクルード
#include "Mesh.h"       // MeshRendererComponentなどをインクルード

// Direct3D 12ヘルパーライブラリ (d3dx12.h) はプロジェクトに含めて使用
#include "d3dx12.h" // これがプロジェクトに含まれていることを確認してください

// 定数バッファの構造体 (シェーダーと同期)
struct SceneConstantBuffer
{
    DirectX::XMFLOAT4X4 mvp; // Model-View-Projection 行列
};

class D3D12Renderer
{
public:
    static const UINT FrameCount = 2; // ダブルバッファリング

    D3D12Renderer();
    ~D3D12Renderer();

    // 初期化: D3D12デバイス、スワップチェイン、コマンドリストなどをセットアップ
    bool Initialize(HWND hwnd, UINT width, UINT height);
    // レンダリングループ: コマンドを記録し、GPUに送信し、表示
    void Render();
    // クリーンアップ: リソースの解放
    void Cleanup();
    // 前のフレームが完了するのを待機
    void WaitForPreviousFrame();

    // === 新しいパブリック関数 ===
    // シーンを設定
    void SetScene(std::shared_ptr<Scene> scene) { m_CurrentScene = scene; }

    // GameObjectからメッシュレンダラーコンポーネントを検出し、そのD3D12リソースを作成・初期化する
    // (Public にすることで main.cpp から呼び出せるようにする)
    bool CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> meshRenderer);

    // アニメーションフレームカウントのゲッター
    UINT GetFrameCount() const { return m_frameCount; }

private:
    // === D3D12オブジェクト ===
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    UINT rtvDescriptorSize;
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[FrameCount];

    // ★追加: 深度バッファ関連
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap; // 深度ステンシルビューヒープ
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilBuffer; // 深度ステンシルバッファリソース

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

    // === 同期オブジェクト ===
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    UINT64 fenceValue;
    HANDLE fenceEvent;

    // === 定数バッファ関連 ===
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
    SceneConstantBuffer m_constantBufferData;
    UINT8* m_pCbvDataBegin; // マップされた定数バッファの先頭ポインタ

    // === 描画関連 ===
    UINT m_Width;
    UINT m_Height;
    UINT frameIndex;
    UINT m_frameCount; // アニメーション用

    // === プライベートヘルパー関数 ===
    bool CreateDevice();
    bool CreateCommandQueue();
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height);
    bool CreateRenderTargetViews();
    bool CreateCommandAllocatorsAndList();
    bool CreatePipelineState();

    // ★追加: 深度バッファの作成関数
    bool CreateDepthStencilBuffer(UINT width, UINT height);

    // 現在のシーンを保持
    std::shared_ptr<Scene> m_CurrentScene;

    // カメラのビュー行列とプロジェクション行列（後でCameraComponentに移す）
    DirectX::XMMATRIX m_ViewMatrix;
    DirectX::XMMATRIX m_ProjectionMatrix;
};