#include "DeviceResources.h"
#include "d3dx12.h"

using Microsoft::WRL::ComPtr;

DeviceResources::~DeviceResources()
{
    ReleaseSizeDependentResources();
    m_queue.Reset();
    m_swapChain.Reset();
    m_device.Reset();
}

bool DeviceResources::Initialize(HWND hwnd, UINT width, UINT height, UINT frameCount)
{
    m_width = width;
    m_height = height;

    if (!CreateDevice()) return false;
    if (!CreateCommandQueue()) return false;
    if (!CreateSwapChain(hwnd, width, height, frameCount)) return false;
    if (!CreateRTVs(frameCount)) return false;
    if (!CreateDSV(width, height)) return false;

    return true;
}

void DeviceResources::Resize(UINT width, UINT height)
{
    if (width == 0 || height == 0) return;
    if (!m_device || !m_swapChain) return;

    m_width = width;
    m_height = height;

    ReleaseSizeDependentResources();

    DXGI_SWAP_CHAIN_DESC1 desc{};
    m_swapChain->GetDesc1(&desc);
    m_swapChain->ResizeBuffers((UINT)m_backBuffers.size(), width, height, desc.Format, desc.Flags);

    CreateRTVs((UINT)m_backBuffers.size());
    CreateDSV(width, height);
}

void DeviceResources::Present(UINT syncInterval)
{
    if (!m_swapChain) return;
    m_swapChain->Present(syncInterval, 0);
}

D3D12_CPU_DESCRIPTOR_HANDLE DeviceResources::GetRTVHandle(UINT index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE h = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += SIZE_T(index) * SIZE_T(m_rtvStride);
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DeviceResources::GetDSVHandle() const
{
    return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

bool DeviceResources::CreateDevice()
{
    UINT flags = 0;
#ifdef _DEBUG
    if (ComPtr<ID3D12Debug> dbg; SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg))))
        dbg->EnableDebugLayer(), flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory)))) return false;

    // pick hardware adapter
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device))))
            break;
    }
    if (!m_device) return false;
    return true;
}

bool DeviceResources::CreateCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC q{};
    q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    q.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    return SUCCEEDED(m_device->CreateCommandQueue(&q, IID_PPV_ARGS(&m_queue)));
}

bool DeviceResources::CreateSwapChain(HWND hwnd, UINT width, UINT height, UINT frameCount)
{
    ComPtr<IDXGIFactory4> factory;
    UINT flags = 0;
#ifdef _DEBUG
    flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    if (FAILED(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory)))) return false;

    DXGI_SWAP_CHAIN_DESC1 sc{};
    sc.BufferCount = frameCount;
    sc.Width = width;
    sc.Height = height;
    sc.Format = m_rtvFormat;
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sc.SampleDesc = { 1, 0 };

    ComPtr<IDXGISwapChain1> sc1;
    if (FAILED(factory->CreateSwapChainForHwnd(m_queue.Get(), hwnd, &sc, nullptr, nullptr, &sc1)))
        return false;
    if (FAILED(sc1.As(&m_swapChain))) return false;

    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    return true;
}

bool DeviceResources::CreateRTVs(UINT frameCount)
{
    m_backBuffers.clear();
    m_backBuffers.resize(frameCount);

    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.NumDescriptors = frameCount;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    if (FAILED(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvHeap))))
        return false;

    m_rtvStride = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE h(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < frameCount; ++i) {
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]))))
            return false;
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, h);
        h.Offset(1, m_rtvStride);
    }
    return true;
}

bool DeviceResources::CreateDSV(UINT width, UINT height)
{
    D3D12_DESCRIPTOR_HEAP_DESC dsv{};
    dsv.NumDescriptors = 1;
    dsv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    if (FAILED(m_device->CreateDescriptorHeap(&dsv, IID_PPV_ARGS(&m_dsvHeap))))
        return false;

    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        m_dsvFormat, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    D3D12_CLEAR_VALUE clear{};
    clear.Format = m_dsvFormat;
    clear.DepthStencil.Depth = 1.0f;
    clear.DepthStencil.Stencil = 0;

    if (FAILED(m_device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear, IID_PPV_ARGS(&m_depth))))
        return false;

    D3D12_DEPTH_STENCIL_VIEW_DESC v{};
    v.Format = m_dsvFormat;
    v.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    m_device->CreateDepthStencilView(m_depth.Get(), &v, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
}

void DeviceResources::ReleaseSizeDependentResources()
{
    for (auto& bb : m_backBuffers) bb.Reset();
    m_depth.Reset();
    m_rtvHeap.Reset();
    m_dsvHeap.Reset();
}
