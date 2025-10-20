#pragma once
#include <DirectXMath.h>

/*
================================================================================
SceneConstantBuffer.h
--------------------------------------------------------------------------------
目的：
  - シェーダーに渡す“1 ドロー（または 1 オブジェクト）”分の定数データを定義する。
  - HLSL の cbuffer と 1:1 に対応するよう、16 バイト境界（vec4 単位）で整列する。

設計メモ：
  - DirectXMath の行列は行優先(XMFLOAT4x4)だが、HLSL 側の mul 順序に合わせて
    VS 側で扱う（典型的には row-major のまま使用可。必要に応じて transpose する）。
  - “worldIT（World 行列の逆転置）”は法線変換用。非一様スケールを含む場合でも
    正しい法線方向を得るために使う。
  - バッファサイズは 16 の倍数に揃える（ここでは 208B→224B 丸めを想定して
    アップロード時は 256B など 256 アライメントに丸めることが多い）。
    実際のアップロード時はフレームリソース側の GetCBStride() が 256B 単位に
    している前提（D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT = 256）。
使用例（頂点シェーダ）：
    cbuffer SceneCB : register(b0) {
      float4x4 gMVP;
      float4x4 gWorld;
      float4x4 gWorldIT;
      float3   gLightDir;
      float    _pad;
    };
    float4 VSMain(float3 pos : POSITION, float3 nrm : NORMAL) : SV_Position {
      float4 wp = mul(float4(pos,1), gWorld);
      float3 wn = normalize(mul(nrm, (float3x3)gWorldIT));
      return mul(wp, gMVP);
    }
================================================================================
*/
struct SceneConstantBuffer
{
    // 64B: Model-View-Projection（ローカル→ワールド→ビュー→プロジェクション）
    //   頂点をクリップ空間へ変換する最終行列。CPU 側で world*view*proj を前計算して詰める。
    DirectX::XMFLOAT4X4 mvp;      // offset 0, size 64

    // 64B: World 行列（ローカル→ワールド）
    //   デバッグやパスごとの再利用（影用、G-Buffer 用など）で個別にも使えるよう保持。
    DirectX::XMFLOAT4X4 world;    // offset 64, size 64

    // 64B: World 行列の逆転置（Inverse Transpose）
    //   法線ベクトル変換用。非一様スケールを含むモデルでも正しい向きを保つ。
    DirectX::XMFLOAT4X4 worldIT;  // offset 128, size 64

    // 12B: ライト方向（ワールド空間、正規化を推奨）
    //   例：{0,-1,-1} を normalize して使用。ピクセルシェーダで Lambert/Phong に利用。
    DirectX::XMFLOAT3   lightDir; // offset 192, size 12

    // 4B: パディング（cbuffer は 16B 単位で詰められるため穴埋め）
    float pad = 0.0f;             // offset 204, size 4
};
