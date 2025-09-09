#pragma once
#include <DirectXMath.h>

// Lambert用 定数バッファ
struct SceneConstantBuffer
{
    DirectX::XMFLOAT4X4 mvp;   // モデルビュー射影行列
    DirectX::XMFLOAT3 lightDir; // 光源方向
    float pad;                  // 16バイト境界合わせ用（必須）
};



