#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <windows.h>
#include "imgui.h"

struct EditorContext; // 前方宣言

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

    // ★ 指定スロットに SRV を作って ImTextureID を返す
    ImTextureID CreateOrUpdateTextureSRV(ID3D12Resource* tex, DXGI_FORMAT fmt, UINT slot);

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    bool m_initialized = false;

    ID3D12Device* m_device = nullptr;
    UINT m_srvIncSize = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvCpuStart{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_srvGpuStart{};
};
