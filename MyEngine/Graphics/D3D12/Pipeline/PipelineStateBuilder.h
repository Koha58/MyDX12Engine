#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>

struct PipelineSet {
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
};

// Lambert ‚Ì RootSig/PSO ‚ğ‘g‚İ—§‚Ä‚Ä out ‚ÉŠi”[
// –ß‚è’l: ¬Œ÷ true / ¸”s false
bool BuildLambertPipeline(ID3D12Device* dev,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat,
    PipelineSet& out);
