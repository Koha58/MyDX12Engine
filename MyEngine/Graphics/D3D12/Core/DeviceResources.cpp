#include "DeviceResources.h"
#include "d3dx12.h"
#include <cassert>
#include "Debug/DebugHr.h"
// （デバッグ強化したいなら）#include <dxgidebug.h> と dxguid.lib をリンク

using Microsoft::WRL::ComPtr;

// ---- mini logger -------------------------------------------------
#include <sstream>
static void DR_Log(const char* s) { OutputDebugStringA(s); OutputDebugStringA("\n"); }
static void DR_LogHR(HRESULT hr, const char* where) {
    if (FAILED(hr)) {
        std::ostringstream oss;
        oss << "[DR][HR] 0x" << std::hex << hr << " @ " << where;
        OutputDebugStringA(oss.str().c_str()); OutputDebugStringA("\n");
    }
    else {
        std::ostringstream oss;
        oss << "[DR] OK @ " << where;
        OutputDebugStringA(oss.str().c_str()); OutputDebugStringA("\n");
    }
}


// 重要：既定フォーマットを初期化（未初期化由来の失敗を防止）
DeviceResources::DeviceResources()
{
    m_rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    m_dsvFormat = DXGI_FORMAT_D32_FLOAT;
    m_rtvStride = 0;
    m_width = m_height = 0;
}

DeviceResources::~DeviceResources()
{
    ReleaseSizeDependentResources();
    m_queue.Reset();
    m_swapChain.Reset();
    m_device.Reset();
}

bool DeviceResources::Initialize(HWND hwnd, UINT width, UINT height, UINT frameCount)
{
    DR_Log("[DR] Initialize begin");
    m_width = width; m_height = height;

    if (!CreateDevice()) { DR_Log("[DR] CreateDevice FAILED"); return false; }
    DR_Log("[DR] CreateDevice OK");
    if (!CreateCommandQueue()) { DR_Log("[DR] CreateCommandQueue FAILED"); return false; }
    DR_Log("[DR] CreateCommandQueue OK");
    if (!CreateSwapChain(hwnd, width, height, frameCount)) { DR_Log("[DR] CreateSwapChain FAILED"); return false; }
    DR_Log("[DR] CreateSwapChain OK");
    if (!CreateRTVs(frameCount)) { DR_Log("[DR] CreateRTVs FAILED"); return false; }
    DR_Log("[DR] CreateRTVs OK");
    if (!CreateDSV(width, height)) { DR_Log("[DR] CreateDSV FAILED"); return false; }
    DR_Log("[DR] CreateDSV OK");

    DR_Log("[DR] Initialize end OK");
    return true;
}


void DeviceResources::Resize(UINT width, UINT height)
{
    {
        std::ostringstream oss;
        oss << "[DR] Resize(" << width << "," << height << ")";
        DR_Log(oss.str().c_str());
    }
    if (width == 0 || height == 0 || !m_device || !m_swapChain) { DR_Log("[DR] Resize early return"); return; }

    DXGI_SWAP_CHAIN_DESC1 desc{};
    HRESULT hr = m_swapChain->GetDesc1(&desc);
    DR_LogHR(hr, "GetDesc1"); if (FAILED(hr)) return;

    const UINT bufferCount = desc.BufferCount ? desc.BufferCount : (UINT)m_backBuffers.size();
    if (bufferCount == 0) { DR_Log("[DR] Resize bufferCount==0"); return; }

    ReleaseSizeDependentResources();
    hr = m_swapChain->ResizeBuffers(bufferCount, width, height, desc.Format, desc.Flags);
    DR_LogHR(hr, "ResizeBuffers"); if (FAILED(hr)) return;

    if (!CreateRTVs(bufferCount)) { DR_Log("[DR] CreateRTVs FAILED after Resize"); return; }
    if (!CreateDSV(width, height)) { DR_Log("[DR] CreateDSV FAILED after Resize"); return; }
    DR_Log("[DR] Resize end OK");
}



void DeviceResources::Present(UINT syncInterval)
{
    if (!m_swapChain) return;
    HRESULT hr = m_swapChain->Present(syncInterval, 0);
    DR_LogHR(hr, "Present");
}


D3D12_CPU_DESCRIPTOR_HANDLE DeviceResources::GetRTVHandle(UINT index) const
{
    assert(m_rtvHeap && m_rtvStride != 0 && "RTV heap is not ready");
    D3D12_CPU_DESCRIPTOR_HANDLE h{};
    if (!m_rtvHeap || m_rtvStride == 0) return h;

    h = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += SIZE_T(index) * SIZE_T(m_rtvStride);
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DeviceResources::GetDSVHandle() const
{
    D3D12_CPU_DESCRIPTOR_HANDLE h{};
    if (!m_dsvHeap) return h;
    return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

bool DeviceResources::CreateDevice()
{
    UINT flags = 0;
#ifdef _DEBUG
    if (Microsoft::WRL::ComPtr<ID3D12Debug> dbg; SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg))))
        dbg->EnableDebugLayer(), flags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    HRB(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory))); // ★ここだけまず置き換え

    // ハードウェアアダプタを選択
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
    DR_Log("[DR] CreateSwapChain begin");
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    UINT flags = 0;
#ifdef _DEBUG
    flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    HRESULT hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory));
    DR_LogHR(hr, "CreateDXGIFactory2"); if (FAILED(hr)) return false;

    DXGI_SWAP_CHAIN_DESC1 sc{};
    sc.BufferCount = frameCount;
    sc.Width = width; sc.Height = height;
    sc.Format = m_rtvFormat;
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sc.SampleDesc = { 1,0 };

    Microsoft::WRL::ComPtr<IDXGISwapChain1> sc1;
    hr = factory->CreateSwapChainForHwnd(m_queue.Get(), hwnd, &sc, nullptr, nullptr, &sc1);
    DR_LogHR(hr, "CreateSwapChainForHwnd"); if (FAILED(hr)) return false;

    hr = sc1.As(&m_swapChain);
    DR_LogHR(hr, "As IDXGISwapChain4"); if (FAILED(hr)) return false;

    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    DR_Log("[DR] CreateSwapChain end OK");
    return true;
}


bool DeviceResources::CreateRTVs(UINT frameCount)
{
    // 残骸をクリア
    m_backBuffers.clear();
    m_backBuffers.resize(frameCount);
    m_rtvHeap.Reset();
    m_rtvStride = 0;

    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.NumDescriptors = frameCount;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    if (FAILED(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvHeap)))) {
        m_backBuffers.clear();
        return false;
    }

    m_rtvStride = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE h(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < frameCount; ++i) {
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])))) {
            m_backBuffers.clear();
            m_rtvHeap.Reset();
            m_rtvStride = 0;
            return false;
        }
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, h);
        h.Offset(1, m_rtvStride);
    }
    return true;
}

bool DeviceResources::CreateDSV(UINT width, UINT height)
{
    m_dsvHeap.Reset();
    m_depth.Reset();

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
    m_backBuffers.clear();
    m_depth.Reset();
    m_rtvHeap.Reset();
    m_dsvHeap.Reset();
    m_rtvStride = 0;
}
