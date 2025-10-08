#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgiformat.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "imgui_internal.h"

class ImGuiLayer;

struct RenderTargetDesc
{
	UINT		width = 0;
	UINT		height = 0;
	DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;
	float		clearColor[4] = { 0,0,0,1 };
	float		clearDepth = 1.0f;
};

class RenderTarget
{
public:
	bool Create(ID3D12Device* dev, const RenderTargetDesc& d);
	void Release();
	bool Resize(ID3D12Device* dev, UINT w, UINT h); // サイズ変化時に作り直し

	// パスの頭/尻で呼ぶユーティリティ
	void TransitionToRT(ID3D12GraphicsCommandList* cmd);
	void TransitionToSRV(ID3D12GraphicsCommandList* cmd);
	void Bind(ID3D12GraphicsCommandList* cmd);
	void Clear(ID3D12GraphicsCommandList* cmd);

	// ImGui用(外部のSRVヒープに載せる)
	ImTextureID EnsureImGuiSRV(ImGuiLayer* imgui, UINT slot) const;

	// 参照系
	UINT Width() const { return m_desc.width; }
	UINT Height() const { return m_desc.height; }
	DXGI_FORMAT ColorFormat() const { return m_desc.colorFormat; }
	ID3D12Resource* Color() const { return m_color.Get(); }

private:
	RenderTargetDesc m_desc{};
	Microsoft::WRL::ComPtr<ID3D12Resource>            m_color;
	Microsoft::WRL::ComPtr<ID3D12Resource>            m_depth;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      m_rtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      m_dsvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE                       m_rtv{};
	D3D12_CPU_DESCRIPTOR_HANDLE                       m_dsv{};
	mutable ImTextureID                               m_imguiTexId = 0;

	D3D12_RESOURCE_STATES m_colorState = D3D12_RESOURCE_STATE_COMMON;

};
