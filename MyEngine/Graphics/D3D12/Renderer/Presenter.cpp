#include "Renderer/Presenter.h"

void Presenter::Begin(ID3D12GraphicsCommandList* cmd, const PresentTargets& t)
{
    // PRESENT -> RENDER_TARGET
    auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(
        t.backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmd->ResourceBarrier(1, &toRT);

    // RTV 設定＆クリア
    cmd->OMSetRenderTargets(1, &t.rtv, FALSE, nullptr);
    cmd->ClearRenderTargetView(t.rtv, t.clearColor, 0, nullptr);

    // Viewport / Scissor をバックバッファ全面に
    D3D12_VIEWPORT vp{ 0.f, 0.f, static_cast<float>(t.width), static_cast<float>(t.height), 0.f, 1.f };
    D3D12_RECT     sc{ 0, 0, static_cast<LONG>(t.width), static_cast<LONG>(t.height) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);
}

void Presenter::End(ID3D12GraphicsCommandList* cmd, ID3D12Resource* backBuffer)
{
    // RENDER_TARGET -> PRESENT
    auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    cmd->ResourceBarrier(1, &toPresent);
}
