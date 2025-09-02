#pragma once // 同じヘッダーファイルが複数回インクルードされるのを防ぐプリプロセッサディレクティブ

#include <windows.h>      // Windows APIの基本機能
#include <d3d12.h>        // Direct3D 12 APIのコアヘッダー
#include <dxgi1_4.h>      // DXGI (DirectX Graphics Infrastructure) APIのヘッダー
#include <wrl/client.h>   // Microsoft::WRL::ComPtr のために必要。COMオブジェクトのスマートポインタ
#include <DirectXMath.h>  // DirectXMathライブラリ。ベクトル、行列などの数学関数
#include <vector>         // std::vectorを使用
#include <memory>         // std::shared_ptrを使用
#include <functional>     // std::function を使用 (今回は直接使われていないが、コールバックなどで有用)

// カスタムクラスのヘッダー
#include "GameObject.h"   // ゲーム内のオブジェクト構造と動作を定義
#include "Mesh.h"         // メッシュデータ構造とMeshRendererComponentを定義
#include "Scene.h"

// Direct3D 12ヘルパーライブラリ (d3dx12.h)
// このファイルは通常、Microsoft.Direct3D.D3D12 NuGetパッケージに含まれている
#include "d3dx12.h" // D3D12オブジェクトの作成を簡素化するユーティリティ関数を提供

// 定数バッファの構造体 (シェーダーとC++コード間でデータを同期)
// シェーダーのCBV（Constant Buffer View）に渡されるデータ構造と一致させる必要がある
struct SceneConstantBuffer
{
    DirectX::XMFLOAT4X4 mvp; // Model-View-Projection 行列
};

// --- D3D12Rendererクラスの定義 ---
// Direct3D 12 APIを用いたレンダリング処理を管理するクラス
class D3D12Renderer
{
public:
    // フレームバッファの数 (ダブルバッファリングの場合は2)
    static const UINT FrameCount = 2;

    // コンストラクタとデストラクタ
    D3D12Renderer();
    ~D3D12Renderer();

    // 初期化関数:
    // D3D12デバイス、スワップチェイン、コマンドリストなどのD3D12基本コンポーネントをセットアップ
    // @param hwnd: レンダリング対象のウィンドウハンドル
    // @param width: クライアント領域の幅
    // @param height: クライアント領域の高さ
    // @return: 初期化が成功した場合はtrue、失敗した場合はfalse
    bool Initialize(HWND hwnd, UINT width, UINT height);

    // レンダリングループの中心:
    // コマンドを記録し、GPUに送信し、結果を表示する
    void Render();

    // クリーンアップ関数:
    // 作成されたD3D12リソースを解放する
    void Cleanup();

    // 同期関数:
    // 前のフレームのGPU処理が完了するのを待機する
    void WaitForPreviousFrame();

    // === 新しいパブリック関数 ===
    // レンダリング対象のシーンを設定する
    // @param scene: レンダリングするSceneオブジェクトのshared_ptr
    void SetScene(std::shared_ptr<Scene> scene) { m_CurrentScene = scene; }

    // GameObjectからMeshRendererComponentを検出し、
    // それに関連するD3D12リソース（頂点バッファ、インデックスバッファなど）を作成・初期化する
    // この関数は`main.cpp`から呼び出され、CPU側のMeshDataをGPUが利用できる形式に変換する
    // @param meshRenderer: リソースを作成するMeshRendererComponentのshared_ptr
    // @return: リソース作成が成功した場合はtrue、失敗した場合はfalse
    bool CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> meshRenderer);

    // アニメーションフレームカウントのゲッター
    // 現在のフレーム数を取得し、アニメーションや時間ベースのロジックに利用できる
    // @return: 現在のフレームカウント
    UINT GetFrameCount() const { return m_frameCount; }

private:
    // === D3D12主要オブジェクトを管理するComPtrメンバー ===
    // ComPtrはCOMインターフェースを自動的に管理し、メモリリークを防ぐスマートポインタ
    Microsoft::WRL::ComPtr<ID3D12Device> device;             // D3D12デバイス
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue; // GPUへのコマンド発行キュー
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;       // フロントバッファとバックバッファを管理し、表示を切り替える
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;    // レンダーターゲットビュー（RTV）のためのディスクリプタヒープ
    UINT rtvDescriptorSize;                                  // RTVディスクリプタのサイズ（ハードウェアによって異なる）
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[FrameCount]; // 各フレームのレンダーターゲットリソース

    // 深度バッファ関連
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;          // 深度ステンシルビュー（DSV）のためのディスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilBuffer;     // 深度ステンシルバッファリソース

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators[FrameCount]; // 各フレームのコマンドアロケータ
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList; // GPUに送信するコマンドを記録するリスト
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;     // シェーダーがアクセスするリソースのレイアウトを定義
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;     // グラフィックスパイプラインの状態（シェーダー、ブレンド設定など）を定義

    // === 同期オブジェクト ===
    Microsoft::WRL::ComPtr<ID3D12Fence> fence; // GPUとCPUの同期オブジェクト
    UINT64 fenceValue;                         // フェンスの現在値
    HANDLE fenceEvent;                         // フェンスイベント発生時に通知されるWin32イベントハンドル

    // === 定数バッファ関連 ===
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvHeap;       // 定数バッファビュー（CBV）のためのディスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;      // 定数バッファのGPUリソース
    SceneConstantBuffer m_constantBufferData;                      // CPU側で保持する定数バッファのデータ
    UINT8* m_pCbvDataBegin;                                        // マップされた定数バッファの先頭ポインタ

    // === 描画関連のメンバー変数 ===
    UINT m_Width;                                                  // クライアント領域の幅
    UINT m_Height;                                                 // クライアント領域の高さ
    UINT frameIndex;                                               // 現在のバックバッファのインデックス
    UINT m_frameCount;                                             // アニメーションなどに使用するフレームカウンター

    // === プライベートヘルパー関数 ===
    // D3D12デバイスを作成する
    bool CreateDevice();
    // コマンドキューを作成する
    bool CreateCommandQueue();
    // スワップチェインを作成する
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height);
    // レンダーターゲットビューを作成する
    bool CreateRenderTargetViews();
    // コマンドアロケータとコマンドリストを作成する
    bool CreateCommandAllocatorsAndList();
    // パイプラインステートオブジェクトを作成する
    bool CreatePipelineState();

    // 深度バッファの作成関数
    // 深度テストとステンシルテストに使用する深度ステンシルバッファを作成する
    // @param width: 深度バッファの幅
    // @param height: 深度バッファの高さ
    // @return: 作成が成功した場合はtrue、失敗した場合はfalse
    bool CreateDepthStencilBuffer(UINT width, UINT height);

    // 現在レンダリングされているシーンへのshared_ptr
    std::shared_ptr<Scene> m_CurrentScene;

    // カメラのビュー行列とプロジェクション行列
    // 現時点ではD3D12Renderer内に直接保持されているが、
    // 将来的にはCameraComponentのような専用のコンポーネントに移動する予定
    DirectX::XMMATRIX m_ViewMatrix;
    DirectX::XMMATRIX m_ProjectionMatrix;
};