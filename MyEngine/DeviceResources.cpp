#include "DeviceResources.h"
#include "DxDebug.h"
#include <Windows.h>
#include <d3dx12.h>

using Microsoft::WRL::ComPtr;

bool DeviceResources::Initialize(HWND hwnd, UINT width, UINT height, UINT frameCount)
{
    if (!CreateDeviceAndQueue())   return false; 
    if (!CreateSwapChain(hwnd))    return false;
    if (!CreateRTVHeapAndViews())  return false;
    if (!CreateDSV(width, height)) return false;

    return true;
}

void DeviceResources::Resize(UINT width, UINT height)
{
    if (width == 0 || height == 0) return; // 最小化など

    m_width = width;
    m_height = height;

    // 旧サイズ依存を破棄
    for (auto& bb : m_backBuffers) bb.Reset();
    m_depth.Reset();
    m_dsvHeap.Reset();

    // スワップチェインだけサイズ変更
    DXGI_SWAP_CHAIN_DESC1 desc{};
    m_swapChain->GetDesc1(&desc);
    HRESULT hr = m_swapChain->ResizeBuffers(m_frameCount, width, height, desc.Format, desc.Flags);
    if (FAILED(hr)) return;

    // バックバッファ再取得 ＆ RTV作成
    CreateRTVHeapAndViews();

    // DSV 再作成
    CreateDSV(width, height);
}

void DeviceResources::Present(UINT syncInterval)
{
    HRESULT hr = m_swapChain->Present(syncInterval, 0);
    dxdbg::LogHRESULTError(hr, "Present");
}

D3D12_CPU_DESCRIPTOR_HANDLE DeviceResources::GetRTVHandle(UINT i) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE h = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += static_cast<SIZE_T>(i) * m_rtvInc;
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DeviceResources::GetDSVHandle() const
{
    return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

bool DeviceResources::CreateDeviceAndQueue()
{
    UINT dxgiFlags = 0;

#ifdef _DEBUG

    dxgiFlags = DXGI_CREATE_FACTORY_DEBUG;

#endif

    ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&factory));
    dxdbg::LogHRESULTError(hr, "CreateDXGIFactory2");
    if (FAILED(hr)) return false;

#ifdef _DEBUG

    if (ComPtr<ID3D12Debug> dbg; SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg))))
    {
        dbg->EnableDebugLayer();
    }

#endif

    // ハードウェアアダプタを列挙し、最初にデバイス作成に成功したものを採用
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue; // WARP は除外
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device))))
            break;
    }
    if (!m_device) {
        OutputDebugStringA("[DeviceResources] Failed to create D3D12 device\n");
        return false;
    }

    dxdbg::SetupImfoQueue(m_device);

    D3D12_COMMAND_QUEUE_DESC q{};
    q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = m_device->CreateCommandQueue(&q, IID_PPV_ARGS(&m_queue));
    dxdbg::LogHRESULTError(hr, "CreateCommandQueue");
    return SUCCEEDED(hr);
}

bool DeviceResources::CreateSwapChain(HWND hwnd)
{
    UINT dxgiFlags = 0;
#ifdef _DEBUG
    dxgiFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&factory));
    dxdbg::LogHRESULTError(hr, "CreateDXGIFactoru2(swapchain)");
    if (FAILED(hr)) return false;

    DXGI_SWAP_CHAIN_DESC1 sc{};
    sc.BufferCount = m_frameCount;                        // バックバッファ枚数はヘッダ側定義に一致
    sc.Width = m_width;
    sc.Height = m_height;
    sc.Format = GetRTVFprmat();
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;     // 近年の推奨
    sc.SampleDesc = { 1, 0 };                            // MSAA 無し

    ComPtr<IDXGISwapChain1> sc1;
    hr = factory->CreateSwapChainForHwnd(m_queue.Get(), hwnd, &sc, nullptr, nullptr, &sc1);
    dxdbg::LogHRESULTError(hr, "CreateSwapChainForHwnd");
    if (FAILED(hr)) return false;

    hr = sc1.As(&m_swapChain);                             // IDXGISwapChain4 へ昇格
    dxdbg::LogHRESULTError(hr, "IDXGISwapChain1::As");
    if (FAILED(hr)) return false;

    // Alt+Enter を禁止（フルスクリーン切替はアプリ側の明示的操作に統一）
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    return true;
}

bool DeviceResources::CreateRTVHeapAndViews()
{
    m_backBuffers.resize(m_frameCount);
    D3D12_DESCRIPTOR_HEAP_DESC h{};

    h.NumDescriptors = m_frameCount;
    h.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    HRESULT hr = m_device->CreateDescriptorHeap(&h, IID_PPV_ARGS(&m_rtvHeap));
    dxdbg::LogHRESULTError(hr, "CreateDescriptorHeap(RTV)");
    if (FAILED(hr)) return false;

    // ヒープ内インクリメント幅（この値だけ Offset して次のハンドルへ進む）
    m_rtvInc = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    auto handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < m_frameCount; ++i)
    {
        hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
        dxdbg::LogHRESULTError(hr, "GetBuffer");
        if (FAILED(hr)) return false;
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, handle);
        handle.ptr += m_rtvInc;
    }
    return true;
}

bool DeviceResources::CreateDSV(UINT width, UINT height)
{
    D3D12_DESCRIPTOR_HEAP_DESC d{};
    d.NumDescriptors = 1;
    d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

    HRESULT hr = m_device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&m_dsvHeap));

    dxdbg::LogHRESULTError(hr, "CreateDescriptorHeap(DSV)");

    if (FAILED(hr)) return false;

    D3D12_CLEAR_VALUE clear{};
    clear.Format = GetDSVFormat();
    clear.DepthStencil.Depth = 1.0f;
    D3D12_HEAP_PROPERTIES hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC   td = CD3DX12_RESOURCE_DESC::Tex2D(
        GetDSVFormat(), width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    hr = m_device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &td,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear, IID_PPV_ARGS(&m_depth));
    dxdbg::LogHRESULTError(hr, "CreateCommittedResource(Depth)");
    if (FAILED(hr)) return false;

    D3D12_DEPTH_STENCIL_VIEW_DESC v{};
    v.Format = GetDSVFormat();
    v.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    m_device->CreateDepthStencilView(m_depth.Get(), &v, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
}