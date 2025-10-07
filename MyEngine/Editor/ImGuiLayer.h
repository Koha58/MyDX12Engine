#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <windows.h>

struct EditorContext; // ëOï˚êÈåæ

class ImGuiLayer
{
public:
    bool Initialize(HWND hwnd,
        ID3D12Device* device,
        ID3D12CommandQueue* queue,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat,
        UINT numFramesInFlight);

    void NewFrame();
    void BuildDockAndWindows(EditorContext& ctx);
    void Render(ID3D12GraphicsCommandList* cmd);
    void Shutdown();

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    bool m_initialized = false;
};
