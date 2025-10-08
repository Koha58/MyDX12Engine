#include "RenderTarget.h"
#include "d3dx12.h"
#include "Editor/ImGuiLayer.h" 

#ifndef DX_CALL
#define DX_CALL(x) do {                                           \
    HRESULT _hr = (x);                                            \
    if (FAILED(_hr)) {                                            \
        char _buf[256];                                           \
        sprintf_s(_buf, "%s failed (hr=0x%08X)\n", #x, (unsigned)_hr); \
        OutputDebugStringA(_buf);                                  \
        return false;                                             \
    }                                                             \
} while (0)
#endif

bool RenderTarget::Create(ID3D12Device* dev, const RenderTargetDesc& d)
{
    if (!dev) return false;
    if (d.width == 0 || d.height == 0) return false; // ★0サイズ作成禁止

    m_desc = d;

    // RTV heap
    D3D12_DESCRIPTOR_HEAP_DESC r{};
    r.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    r.NumDescriptors = 1;
    r.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    DX_CALL(dev->CreateDescriptorHeap(&r, IID_PPV_ARGS(&m_rtvHeap)));
    m_rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    // Color
    auto rd = CD3DX12_RESOURCE_DESC::Tex2D(d.colorFormat, d.width, d.height);
    rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_CLEAR_VALUE c{};
    c.Format = d.colorFormat;
    memcpy(c.Color, d.clearColor, sizeof(float) * 4);

    DX_CALL(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_RENDER_TARGET, &c, IID_PPV_ARGS(&m_color)));
    dev->CreateRenderTargetView(m_color.Get(), nullptr, m_rtv);
    m_colorState = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // Depth（任意）
    if (d.depthFormat != DXGI_FORMAT_UNKNOWN) {
        D3D12_DESCRIPTOR_HEAP_DESC dh{};
        dh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dh.NumDescriptors = 1;
        DX_CALL(dev->CreateDescriptorHeap(&dh, IID_PPV_ARGS(&m_dsvHeap)));
        m_dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

        auto dd = CD3DX12_RESOURCE_DESC::Tex2D(d.depthFormat, d.width, d.height);
        dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE dc{};
        dc.Format = d.depthFormat;
        dc.DepthStencil.Depth = d.clearDepth;

        DX_CALL(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &dd,
            D3D12_RESOURCE_STATE_DEPTH_WRITE, &dc, IID_PPV_ARGS(&m_depth)));
        dev->CreateDepthStencilView(m_depth.Get(), nullptr, m_dsv);
    }

#ifdef _DEBUG
    if (m_color) m_color->SetName(L"RenderTarget.Color");
    if (m_depth) m_depth->SetName(L"RenderTarget.Depth");
#endif

    m_imguiTexId = 0;
    return true;
}


bool RenderTarget::Resize(ID3D12Device* dev, UINT w, UINT h)
{
    if (w == 0 || h == 0) return false;          // ★追加
    if (w == m_desc.width && h == m_desc.height) return false;

    Release();
    m_desc.width = w; m_desc.height = h;
    return Create(dev, m_desc);
}


void RenderTarget::Release()
{
    // ImGuiのSRVは外部ヒープ管理なので、ここではハンドルだけ忘れる
    m_imguiTexId = 0;

    // in-flight 回避は呼び出し側（必要なら WaitForGPU）で担保すること
    m_depth.Reset();
    m_dsvHeap.Reset();

    m_color.Reset();
    m_rtvHeap.Reset();

    m_rtv = {};
    m_dsv = {};
    m_colorState = D3D12_RESOURCE_STATE_COMMON;
}


void RenderTarget::TransitionToRT(ID3D12GraphicsCommandList* cmd)
{
    if (m_colorState == D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        return;
    }

    auto b = CD3DX12_RESOURCE_BARRIER::Transition(m_color.Get(), m_colorState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmd->ResourceBarrier(1, &b);
    m_colorState = D3D12_RESOURCE_STATE_RENDER_TARGET;
}

void RenderTarget::TransitionToSRV(ID3D12GraphicsCommandList* cmd)
{
    if (m_colorState == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) return;
    auto b = CD3DX12_RESOURCE_BARRIER::Transition(
        m_color.Get(), m_colorState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); // ★fromに実状態
    cmd->ResourceBarrier(1, &b);
    m_colorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}


void RenderTarget::Bind(ID3D12GraphicsCommandList* cmd)
{
    cmd->OMSetRenderTargets(1, &m_rtv, FALSE, m_depth ? &m_dsv : nullptr);
}

void RenderTarget::Clear(ID3D12GraphicsCommandList* cmd)
{
    cmd->ClearRenderTargetView(m_rtv, m_desc.clearColor, 0, nullptr);
    if (m_depth) cmd->ClearDepthStencilView(m_dsv, D3D12_CLEAR_FLAG_DEPTH, m_desc.clearDepth, 0, 0, nullptr);
}

ImTextureID RenderTarget::EnsureImGuiSRV(ImGuiLayer* imgui, UINT slot) const
{
    if (!m_imguiTexId) {
        m_imguiTexId = imgui->CreateOrUpdateTextureSRV(m_color.Get(), m_desc.colorFormat, slot);
    }
    return m_imguiTexId; // ★常に返す（未定義動作の除去）
}

RenderTargetHandles RenderTarget::Detach()
{
    RenderTargetHandles out{};
    out.color = std::move(m_color);
    out.depth = std::move(m_depth);
    out.rtvHeap = std::move(m_rtvHeap);
    out.dsvHeap = std::move(m_dsvHeap);
    out.rtv = m_rtv;
    out.dsv = m_dsv;

    // 内部状態をからっぽに
    m_rtv = {};
    m_dsv = {};
    m_imguiTexId = 0;
    m_colorState = D3D12_RESOURCE_STATE_COMMON;
    m_desc.width = m_desc.height = 0;

    return out; // out の ComPtr が作用域を抜けたタイミングで解放される
}

