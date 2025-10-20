#include "DeviceResources.h"
#include "d3dx12.h"
#include <cassert>
#include "Debug/DebugHr.h"
// （さらに深掘りデバッグしたい）
//   #include <dxgidebug.h> + dxguid.lib
//   → IDXGIInfoQueue で DXGI/D3D の runtime warning をダンプ可能

using Microsoft::WRL::ComPtr;

#include <sstream>
static void DR_Log(const char* s) { OutputDebugStringA(s); OutputDebugStringA("\n"); }
static void DR_LogHR(HRESULT hr, const char* where) {
    if (FAILED(hr)) {
        std::ostringstream oss; oss << "[DR][HR] 0x" << std::hex << hr << " @ " << where;
        OutputDebugStringA(oss.str().c_str()); OutputDebugStringA("\n");
    }
    else {
        std::ostringstream oss; oss << "[DR] OK @ " << where;
        OutputDebugStringA(oss.str().c_str()); OutputDebugStringA("\n");
    }
}


// ============================================================================
// DeviceResources
//   役割：D3D12 Device/Queue/SwapChain/RTV/DSV の生成とリサイズを一元管理。
//   注意：描画コマンドや同期（Fence 等）は呼び出し側の責務。
//   DPI/論理座標：ここはピクセル解像度（物理）だけ扱う。DPIは上位で吸収。
// ============================================================================
DeviceResources::DeviceResources()
{
    // フォーマットは先に既定値を入れて事故を防ぐ（未初期化利用の保険）
    m_rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    m_dsvFormat = DXGI_FORMAT_D32_FLOAT;
    m_rtvStride = 0;

    // GetWidth/Height の返値源。Resize() 時に更新しないと古い値のままになる点に注意。
    m_width = m_height = 0;
}

DeviceResources::~DeviceResources()
{
    // サイズ依存リソース → デバイス非依存の順に解放（参照を切ってから Finalize）
    ReleaseSizeDependentResources();
    m_queue.Reset();
    m_swapChain.Reset();
    m_device.Reset();
}

bool DeviceResources::Initialize(HWND hwnd, UINT width, UINT height, UINT frameCount)
{
    DR_Log("[DR] Initialize begin");

    // ここでの width/height は“初期ウィンドウ解像度”。GetWidth/Height の初期値にもする。
    m_width = width;
    m_height = height;

    // 1) デバイス生成（ハードウェアアダプタを選抜して D3D12CreateDevice）
    if (!CreateDevice()) { DR_Log("[DR] CreateDevice FAILED"); return false; }
    DR_Log("[DR] CreateDevice OK");

    // 2) DIRECT キュー 1 本（グラフィックス用）。Copy/Compute は必要時に別途。
    if (!CreateCommandQueue()) { DR_Log("[DR] CreateCommandQueue FAILED"); return false; }
    DR_Log("[DR] CreateCommandQueue OK");

    // 3) スワップチェインを HWND に関連付けて作成（Flip-Discard）
    //    幅/高さは初期値。実運用では WM_SIZE で Resize() によって更新される。
    if (!CreateSwapChain(hwnd, width, height, frameCount)) { DR_Log("[DR] CreateSwapChain FAILED"); return false; }
    DR_Log("[DR] CreateSwapChain OK");

    // 4) バックバッファから RTV を作成（frameCount 分）
    if (!CreateRTVs(frameCount)) { DR_Log("[DR] CreateRTVs FAILED"); return false; }
    DR_Log("[DR] CreateRTVs OK");

    // 5) 深度テクスチャ＋DSV を“ウィンドウサイズ”で用意（SceneRT と分けたい場合は別管理）
    if (!CreateDSV(width, height)) { DR_Log("[DR] CreateDSV FAILED"); return false; }
    DR_Log("[DR] CreateDSV OK");

    DR_Log("[DR] Initialize end OK");
    return true;
}


// ============================================================================
// Resize
//   目的：SwapChain バッファ群 + DSV を新しい幅/高さで作り直す。
//   前提：呼び出し側で GPU 完了待ち済み（使用中のバックバッファを握ったまま ResizeBuffers はNG）。
//   典型ルート：WM_SIZE → Renderer::Resize() → DeviceResources::Resize()。
//   重要：GetWidth/Height を最新化したいなら ResizeBuffers 成功後に m_width/m_height を更新する。
// ============================================================================
void DeviceResources::Resize(UINT width, UINT height)
{
    // ログ（ウィンドウ最小化や未初期化を識別する助け）
    {
        std::ostringstream oss; oss << "[DR] Resize(" << width << "," << height << ")";
        DR_Log(oss.str().c_str());
    }

    // 最小化(0x0)・未初期化・SwapChain 未生成のいずれかなら抜ける
    if (width == 0 || height == 0 || !m_device || !m_swapChain) {
        DR_Log("[DR] Resize early return");
        return;
    }

    // 現在の SC 設定を取得。BufferCount/Format/Flags を使い回す（呼び出し元が変えていない前提）
    DXGI_SWAP_CHAIN_DESC1 desc{};
    HRESULT hr = m_swapChain->GetDesc1(&desc);
    DR_LogHR(hr, "GetDesc1");
    if (FAILED(hr)) return;

    // バッファ枚数：GetDesc1 が返した値を優先。0 の場合は念のため現在の配列サイズを参照。
    const UINT bufferCount = desc.BufferCount ? desc.BufferCount : (UINT)m_backBuffers.size();
    if (bufferCount == 0) {
        // このケースは通常発生しない（CreateRTVs 未実行など）。安全に中断。
        DR_Log("[DR] Resize bufferCount==0");
        return;
    }

    // 1) サイズ依存リソースの参照を切る（これを怠ると ResizeBuffers が E_INVALIDARG などで失敗しやすい）
    ReleaseSizeDependentResources();

    // 2) バックバッファを新しい幅/高さで再確保
    //    DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING を使う運用なら desc.Flags に含める必要がある。
    hr = m_swapChain->ResizeBuffers(bufferCount, width, height, desc.Format, desc.Flags);
    DR_LogHR(hr, "ResizeBuffers");
    if (FAILED(hr)) return;

    // 3) （オプション）GetWidth/Height を最新化したい場合はここで更新
    //    実際に UI 側で m_dev->GetWidth()/GetHeight() を表示したいなら必須。
    // m_width  = width;
    // m_height = height;

    // 4) 新しいバックバッファに対して RTV を作成し直す
    if (!CreateRTVs(bufferCount)) {
        DR_Log("[DR] CreateRTVs FAILED after Resize");
        return;
    }

    // 5) DSV も同様に再作成（ここではウィンドウサイズに追随させる方針）
    if (!CreateDSV(width, height)) {
        DR_Log("[DR] CreateDSV FAILED after Resize");
        return;
    }

    DR_Log("[DR] Resize end OK");
}



// ============================================================================
// Present
//   Flip-Discard 前提。Present 失敗は TDR やデバイスロストの兆候なので HR を残す。
//   可変リフレッシュ/テアリングは syncInterval/Flags で制御（別運用）。
// ============================================================================
void DeviceResources::Present(UINT syncInterval)
{
    if (!m_swapChain) return;

    // syncInterval: 0=テアリング可（Flags 側の設定も必要） / 1=VSync
    // Flags: DXGI_PRESENT_ALLOW_TEARING を付けるとテアリング許可（SwapChain 側の flag と対）
    HRESULT hr = m_swapChain->Present(syncInterval, 0);
    DR_LogHR(hr, "Present");
}



// ============================================================================
// GetRTVHandle
//   RTV ヒープの index 番目の CPU ハンドルを返す。
//   注意：ここは“範囲チェック”をしない。呼び出し側で [0..FrameCount-1] を保証する。
// ============================================================================
D3D12_CPU_DESCRIPTOR_HANDLE DeviceResources::GetRTVHandle(UINT index) const
{
    // 生成前に呼ばれていないかの保険。assert は開発時チェック用。
    assert(m_rtvHeap && m_rtvStride != 0 && "RTV heap is not ready");
    D3D12_CPU_DESCRIPTOR_HANDLE h{};
    if (!m_rtvHeap || m_rtvStride == 0) return h; // リリースビルドでは null を返す

    // ヒープ先頭 + index * インクリメントサイズ
    h = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += SIZE_T(index) * SIZE_T(m_rtvStride);
    return h;
}

// ============================================================================
// GetDSVHandle
//   単一の DSV を想定。未生成時は null ハンドル。
// ============================================================================
D3D12_CPU_DESCRIPTOR_HANDLE DeviceResources::GetDSVHandle() const
{
    D3D12_CPU_DESCRIPTOR_HANDLE h{};
    if (!m_dsvHeap) return h;
    return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}



// ============================================================================
// CreateDevice
//   1) DXGI Factory 作成（デバッグ時は DebugLayer + FactoryDebug）
//   2) ソフトウェアアダプタを除外してハードウェアアダプタを列挙 → 最初に通るもので Device 生成。
//      ※ WARP fallback を入れる場合は失敗時に D3D12CreateDevice(nullptr, ...) を試す。
// ============================================================================
bool DeviceResources::CreateDevice()
{
    UINT flags = 0;

#ifdef _DEBUG
    // デバッグレイヤーオン（GPU Validation などは外部設定推奨）
    if (Microsoft::WRL::ComPtr<ID3D12Debug> dbg; SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)))) {
        dbg->EnableDebugLayer();
        flags = DXGI_CREATE_FACTORY_DEBUG; // DXGI のデバッグ情報も出す
    }
#endif

    // Factory は本関数内ローカル（保持不要）。デバイス作成にのみ使用。
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    HRB(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory))); // 失敗時は HRB がログ/例外。

    // ハードウェアアダプタの列挙。要求レベルは D3D_FEATURE_LEVEL_11_0（必要なら上げる）。
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);

        // ソフトウェア（WARP）を除外
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

        // 通れば m_device にセットされる。失敗なら次のアダプタへ。
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device))))
            break;
    }

    // すべて失敗した場合は false（呼び出し側でログ済み）
    if (!m_device) return false;
    return true;
}



// ============================================================================
// CreateCommandQueue
//   DIRECT（グラフィックス）キューを 1 本。タイムライン/Fence は外部で管理。
// ============================================================================
bool DeviceResources::CreateCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC q{};
    q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;   // 描画コマンド流す本命
    q.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;    // 必要に応じてタイムスタンプ等を付与
    return SUCCEEDED(m_device->CreateCommandQueue(&q, IID_PPV_ARGS(&m_queue)));
}



// ============================================================================
// CreateSwapChain
//   HWND ターゲットに SwapChain を作成（Flip-Discard, R8G8B8A8_UNORM）。
//   Alt+Enter は抑止（全画面切替はアプリ側で管理）。
//   注意：テアリング許可・HDR（カラー空間）は別途設定が必要。
// ============================================================================
bool DeviceResources::CreateSwapChain(HWND hwnd, UINT width, UINT height, UINT frameCount)
{
    DR_Log("[DR] CreateSwapChain begin");

    // Factory：ここではローカル。SwapChain 作成にのみ使用。
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    UINT flags = 0;
#ifdef _DEBUG
    flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    HRESULT hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory));
    DR_LogHR(hr, "CreateDXGIFactory2");
    if (FAILED(hr)) return false;

    // 最小限の SC 設定。Tearing を許可したいなら Flags/Present 側も合わせて要設定。
    DXGI_SWAP_CHAIN_DESC1 sc{};
    sc.BufferCount = frameCount;          // ダブル/トリプルバッファ
    sc.Width = width;                    // 初期バックバッファの幅（後で Resize() で更新）
    sc.Height = height;                   // 初期高さ
    sc.Format = m_rtvFormat;              // 例: R8G8B8A8_UNORM
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // 推奨
    sc.SampleDesc = { 1, 0 };             // MSAA 無し

    // IDXGISwapChain1 をまず作って、IDXGISwapChain4 に昇格
    Microsoft::WRL::ComPtr<IDXGISwapChain1> sc1;
    hr = factory->CreateSwapChainForHwnd(m_queue.Get(), hwnd, &sc, nullptr, nullptr, &sc1);
    DR_LogHR(hr, "CreateSwapChainForHwnd");
    if (FAILED(hr)) return false;

    hr = sc1.As(&m_swapChain);
    DR_LogHR(hr, "As IDXGISwapChain4");
    if (FAILED(hr)) return false;

    // Alt+Enter を OS 既定に任せない（アプリ制御にする）
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    DR_Log("[DR] CreateSwapChain end OK");
    return true;
}



// ============================================================================
// CreateRTVs
//   現在の SwapChain から BackBuffer を取得し、RTV を連番で作成。
//   リサイズ時にも呼ばれるため、冒頭で旧リソースの参照を必ず断つ。
// ============================================================================
bool DeviceResources::CreateRTVs(UINT frameCount)
{
    // 旧バックバッファ/RTV ヒープの参照をクリア（ResizeBuffers の“ため込み”を避ける）
    m_backBuffers.clear();                 // vector のサイズも変えるので clear→resize の順
    m_backBuffers.resize(frameCount);
    m_rtvHeap.Reset();
    m_rtvStride = 0;

    // RTV ヒープ（CPU 専用）
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.NumDescriptors = frameCount;      // バックバッファ枚数と一致
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    if (FAILED(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvHeap)))) {
        m_backBuffers.clear();
        return false;
    }

    // ハンドル計算用のインクリメント
    m_rtvStride = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // 先頭から連番で RTV を作っていく
    CD3DX12_CPU_DESCRIPTOR_HANDLE h(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < frameCount; ++i) {
        // SC の i 番目のバックバッファ（ID3D12Resource）を取得
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])))) {
            // 途中失敗時：中途半端な状態を残さない
            m_backBuffers.clear();
            m_rtvHeap.Reset();
            m_rtvStride = 0;
            return false;
        }

        // デフォルト記述子で RTV 作成（バックバッファは sRGB RTV をサポートしない点に注意）
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, h);

        // 次のスロットへ進める
        h.Offset(1, m_rtvStride);
    }
    return true;
}



// ============================================================================
// CreateDSV
//   ウィンドウサイズで Depth テクスチャを作り、単一の DSV を切る。
//   表示用の深度として運用。SceneRT 専用の深度が欲しければ別に管理する。
// ============================================================================
bool DeviceResources::CreateDSV(UINT width, UINT height)
{
    // ヒープ/リソースを作り直すため、まずは参照を切る
    m_dsvHeap.Reset();
    m_depth.Reset();

    // DSV ヒープ（CPU 専用・1個）
    D3D12_DESCRIPTOR_HEAP_DESC dsv{};
    dsv.NumDescriptors = 1;
    dsv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    if (FAILED(m_device->CreateDescriptorHeap(&dsv, IID_PPV_ARGS(&m_dsvHeap))))
        return false;

    // Depth テクスチャ本体を既定フォーマット/サイズで作成
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        m_dsvFormat, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    // クリア値（DSV 作成時に最適化される）
    D3D12_CLEAR_VALUE clear{};
    clear.Format = m_dsvFormat;
    clear.DepthStencil.Depth = 1.0f;
    clear.DepthStencil.Stencil = 0;

    // 深度リソース作成（初期状態：DEPTH_WRITE）
    if (FAILED(m_device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear, IID_PPV_ARGS(&m_depth))))
        return false;

    // DSV 記述子（デフォルト）
    D3D12_DEPTH_STENCIL_VIEW_DESC v{};
    v.Format = m_dsvFormat;
    v.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    // DSV 作成（単一）
    m_device->CreateDepthStencilView(m_depth.Get(), &v, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
}



// ============================================================================
// ReleaseSizeDependentResources
//   リサイズ前/デバイスロスト前などに必ず呼ぶ。
//   バックバッファやヒープの“参照”を切ってから ResizeBuffers を呼ばないと失敗しがち。
// ============================================================================
void DeviceResources::ReleaseSizeDependentResources()
{
    // BackBuffer 参照をすべて解放
    for (auto& bb : m_backBuffers) bb.Reset();
    m_backBuffers.clear();

    // 深度/ヒープも解放（ハンドル増分もクリア）
    m_depth.Reset();
    m_rtvHeap.Reset();
    m_dsvHeap.Reset();
    m_rtvStride = 0;
}
