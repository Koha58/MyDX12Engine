#pragma once
#include <d3d12.h>

// d3dx12.h の相対パスはプロジェクト構成に合わせて
#include "d3dx12.h"

// バックバッファに描くときに必要なターゲット情報
struct PresentTargets {
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{}; // BackBuffer の RTV
    ID3D12Resource* backBuffer = nullptr;
    UINT                        width = 0;
    UINT                        height = 0;
    float                       clearColor[4] = { 0.2f, 0.2f, 0.4f, 1.0f };
};

class Presenter {
public:
    void Begin(ID3D12GraphicsCommandList* cmd, const PresentTargets& t);
    void End(ID3D12GraphicsCommandList* cmd, ID3D12Resource* backBuffer);
};
