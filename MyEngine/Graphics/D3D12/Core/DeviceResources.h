#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <vector>

class DeviceResources
{
public:
    DeviceResources();               // ★ 追加：コンストラクタ宣言
    ~DeviceResources();

    bool Initialize(HWND hwnd, UINT width, UINT height, UINT frameCount);
    void Resize(UINT width, UINT height);
    void Present(UINT syncInterval);

    // Getters
    ID3D12Device* GetDevice() const { return m_device.Get(); }
    ID3D12CommandQueue* GetQueue()  const { return m_queue.Get(); }
    ID3D12Resource* GetBackBuffer(UINT index) const { return m_backBuffers[index].Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(UINT index) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const;
    DXGI_FORMAT          GetRTVFormat() const { return m_rtvFormat; }
    DXGI_FORMAT          GetDSVFormat() const { return m_dsvFormat; }
    UINT                 GetCurrentBackBufferIndex() const { return m_swapChain->GetCurrentBackBufferIndex(); }
    UINT                 GetWidth()  const { return m_width; }
    UINT                 GetHeight() const { return m_height; }

private:
    bool CreateDevice();
    bool CreateCommandQueue();
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height, UINT frameCount);
    bool CreateRTVs(UINT frameCount);
    bool CreateDSV(UINT width, UINT height);
    void ReleaseSizeDependentResources();

private:
    // core
    Microsoft::WRL::ComPtr<ID3D12Device>       m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue;
    Microsoft::WRL::ComPtr<IDXGISwapChain4>    m_swapChain;

    // RTV/DSV
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    UINT        m_rtvStride = 0;
    DXGI_FORMAT m_rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_dsvFormat = DXGI_FORMAT_D32_FLOAT;

    // back buffers
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_backBuffers;

    // depth
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depth;

    // size
    UINT m_width = 0;
    UINT m_height = 0;
};
