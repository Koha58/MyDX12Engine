#pragma once
// ============================================================================
// PipelineStateBuilder.h
// 目的：
//   - “Lambert シェーディング用”の最小限の RootSignature / PSO をひとまとめにし、
//     呼び出し側が 1 関数でセットアップできるようにするヘッダ。
//   - 実装（.cpp）側で HLSL の組み込み（D3DCompile）と PSO 構築を行う。
// 運用メモ：
//   - 入力レイアウトは POSITION(float3) / NORMAL(float3) / COLOR(float4) を想定。
//   - ルートシグネチャは CBV(b0) を 1 本だけ（VS/PS 共有）に限定。
//   - 深度は既定で ON（LESS、書き込みあり）。必要なら .cpp 側で調整。
//   - RTV は 1 枚のみ、フォーマットは呼び出し時に指定。
// ============================================================================

#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>

// ----------------------------------------------------------------------------
// PipelineSet
// 役割：BuildLambertPipeline で組み上げた RootSignature と PSO の受け皿。
// 注意：ComPtr なので参照カウントは自動管理。Destroy は不要。
// ----------------------------------------------------------------------------
struct PipelineSet {
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root;  // CBV(b0) 1 本の簡易 RootSig
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;   // Lambert 用 Graphics PSO
};

// ----------------------------------------------------------------------------
// BuildLambertPipeline
// 役割：Lambert 用 HLSL（VS/PS）を内部でコンパイル → RootSig と PSO を作成。
// 戻り値：true=成功 / false=失敗（D3DCompile/PSO 作成のどれかが失敗）
//
// 引数：
//  - dev       : D3D12 デバイス
//  - rtvFormat : 出力カラーの DXGI フォーマット（例：DXGI_FORMAT_R8G8B8A8_UNORM）
//  - dsvFormat : 深度ステンシルのフォーマット（例：DXGI_FORMAT_D32_FLOAT）
//  - out       : 作成した RootSignature / PSO を格納（成功時のみ有効）
//
// 期待するバインド：
//  - VS/PS とも CBV(b0) に以下を期待：
//      row_major float4x4 g_mvp;
//      row_major float4x4 g_world;
//      row_major float4x4 g_worldIT;
//      float3 g_lightDir; float pad;
//
// 期待する入力レイアウト：
//  - POSITION : float3 (offset 0)
//  - NORMAL   : float3 (offset 12)
//  - COLOR    : float4 (offset 24)
//
// 利用例：
//   PipelineSet pipe{};
//   if (!BuildLambertPipeline(dev, rtvFmt, dsvFmt, pipe)) { /* エラー処理 */ }
//   cmd->SetGraphicsRootSignature(pipe.root.Get());
//   cmd->SetPipelineState(pipe.pso.Get());
// ----------------------------------------------------------------------------
bool BuildLambertPipeline(ID3D12Device* dev,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat,
    PipelineSet& out);
