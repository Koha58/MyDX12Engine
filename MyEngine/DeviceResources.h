#pragma once
#include <wrl/client.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <vector>

class DeviceResources {
public:
	bool Initialize(HWND hwnd, UINT width, UINT height, UINT frameCount);

	void Resize(UINT width, UINT height);

	void Present(UINT syncInterval = 1);

	// Getters
	ID3D12Device*		 GetDevice() const { return m_device.Get(); }
	ID3D12CommandQueue*	 GetQueue() const { return m_queue.Get(); }
	IDXGISwapChain4*	 GetSwapChain() const { return m_swapChain.Get(); }

	UINT						 GetFrameCount() const { return m_frameCount; }
	UINT						 GetCurrentBackBufferIndex() const { return m_swapChain->GetCurrentBackBufferIndex(); }
	ID3D12Resource*				 GetBackBuffer(UINT i) const { return m_backBuffers[i].Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE  GetRTVHandle(UINT i) const;
	D3D12_CPU_DESCRIPTOR_HANDLE  GetDSVHandle() const;

	DXGI_FORMAT GetRTVFprmat() const { return DXGI_FORMAT_R8G8B8A8_UNORM; }
	DXGI_FORMAT GetDSVFormat() const { return DXGI_FORMAT_D32_FLOAT;      }

	UINT GetWidth()  const { return m_width; }
	UINT GetHeight() const { return m_height; }

private:
	bool CreateDeviceAndQueue();
	bool CreateSwapChain(HWND hwnd);
	bool CreateRTVHeapAndViews();
	bool CreateDSV(UINT width, UINT height);

private:
	Microsoft::WRL::ComPtr<ID3D12Device>                m_device;         // 論理デバイス
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>          m_queue;   // 直接コマンドキュー
	Microsoft::WRL::ComPtr<IDXGISwapChain4>             m_swapChain;      // バック/フロント切替（IDXGISwapChain4）

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        m_rtvHeap;        // RTV ヒープ（CPU 可視）
	UINT m_rtvInc = 0;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_backBuffers;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        m_dsvHeap;        // DSV ヒープ（CPU 可視）
	Microsoft::WRL::ComPtr<ID3D12Resource>				m_depth;        // DSV ヒープ（CPU 可視）

	UINT                                                m_width = 0;   // クライアント領域の幅（px）
	UINT                                                m_height = 0;   // クライアント領域の高さ（px）
	UINT                                                m_frameCount = 0; // Present 回数（統計/アニメ制御など）

};
