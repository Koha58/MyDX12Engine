#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <vector>

/*
    DeviceResources
    ----------------------------------------------------------------------------
    役割：
      - DXGI / D3D12 の「土台」になるリソースをひとまとめに管理するユーティリティ。
        * デバイス（ID3D12Device）
        * コマンドキュー（ID3D12CommandQueue）
        * スワップチェイン（IDXGISwapChain4）
        * バックバッファ（ID3D12Resource[]）＋ RTV ヒープ
        * 深度バッファ（ID3D12Resource）＋ DSV ヒープ
      - ウィンドウのリサイズ時に必要な「サイズ依存リソース（バックバッファ/深度）」の
        作り直しもここで完結させる。

    ポリシー：
      - Initialize() → Resize() → Present() の直列使用を想定（マルチスレッド非対応）。
      - サイズに依存するものは ReleaseSizeDependentResources() で一旦解放してから再作成。
      - RTV/DSV のフォーマットは固定（必要ならセッタを追加してプロジェクト方針に合わせて変更）。
      - エラーハンドリングは bool 戻り値。詳細なログは上位層または dxdbg などで行う。
*/
class DeviceResources
{
public:
    DeviceResources();               // ★ 追加：コンストラクタ宣言（初期値はメンバ初期化子にて）
    ~DeviceResources();

    /*
        Initialize
        ------------------------------------------------------------------------
        - D3D12 デバイス、コマンドキュー、スワップチェイン、RTV/DSV を生成。
        - 最初のウィンドウサイズ（width/height）とフレーム数（frameCount）を受け取る。
        戻り値：成功で true
    */
    bool Initialize(HWND hwnd, UINT width, UINT height, UINT frameCount);

    /*
        Resize
        ------------------------------------------------------------------------
        - ウィンドウクライアントサイズの変更に伴い、サイズ依存リソースを作り直す。
        - 事前に GPU 完了待ち（外側）を行ってから呼ぶのが安全。
    */
    void Resize(UINT width, UINT height);

    /*
        Present
        ------------------------------------------------------------------------
        - スワップチェイン Present。syncInterval=1 で垂直同期待ちの一般的な設定。
    */
    void Present(UINT syncInterval);

    // ------------------------- Getters（軽量アクセサ群） -------------------------
    ID3D12Device* GetDevice() const { return m_device.Get(); }
    ID3D12CommandQueue* GetQueue()  const { return m_queue.Get(); }
    ID3D12Resource* GetBackBuffer(UINT index) const { return m_backBuffers[index].Get(); }

    // バックバッファ i 番の RTV ハンドル
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(UINT index) const;

    // 深度ステンシルの DSV ハンドル
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const;

    // パイプライン互換用に RTV/DSV のフォーマットを公開
    DXGI_FORMAT GetRTVFormat() const { return m_rtvFormat; }
    DXGI_FORMAT GetDSVFormat() const { return m_dsvFormat; }

    // 現在のバックバッファインデックス（スワップチェインから直取得）
    UINT GetCurrentBackBufferIndex() const { return m_swapChain->GetCurrentBackBufferIndex(); }

    // 現行のクライアント幅/高さ（バックバッファと一致）
    UINT GetWidth()  const { return m_width; }
    UINT GetHeight() const { return m_height; }

private:
    // ------------------------- 内部ヘルパ（生成手順を分割） -------------------------
    bool CreateDevice();                                                  // アダプタ選定 → ID3D12Device 生成
    bool CreateCommandQueue();                                            // DIRECT タイプのキュー
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height, UINT frameCount); // スワップチェイン生成
    bool CreateRTVs(UINT frameCount);                                     // RTV ヒープ＋各バックバッファ RTV
    bool CreateDSV(UINT width, UINT height);                              // DSV ヒープ＋深度バッファ
    void ReleaseSizeDependentResources();                                 // バックバッファ/深度を解放

private:
    // ------------------------- Core -------------------------
    Microsoft::WRL::ComPtr<ID3D12Device>       m_device;     // 諸々のファクトリ
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue;      // 提出先（DIRECT）
    Microsoft::WRL::ComPtr<IDXGISwapChain4>    m_swapChain;  // 表示面の二重/三重バッファ

    // ------------------------- RTV / DSV -------------------------
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;  // バックバッファ用 RTV ヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;  // 深度用 DSV ヒープ
    UINT        m_rtvStride = 0;                             // RTV ハンドル間インクリメント
    DXGI_FORMAT m_rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;    // 既定のバックバッファフォーマット
    DXGI_FORMAT m_dsvFormat = DXGI_FORMAT_D32_FLOAT;         // 既定の深度フォーマット

    // ------------------------- Back Buffers -------------------------
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_backBuffers; // SwapChain の各画像

    // ------------------------- Depth Buffer -------------------------
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depth;          // 深度ステンシルテクスチャ

    // ------------------------- Size -------------------------
    UINT m_width = 0;   // 現在のクライアント幅（バックバッファ幅）
    UINT m_height = 0;   // 現在のクライアント高（バックバッファ高）
};
