#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <vector>

/*
===============================================================================
Mesh / Vertex 周りの最小定義（DX12 向け）
-------------------------------------------------------------------------------
役割：
  - CPU 側メッシュ（MeshData）と GPU 側リソース（MeshGPU）を分けて管理
  - CreateMesh() で Upload ヒープに VB/IB を確保して CPU→GPU にコピー
    ※ 本実装は「描画まで常時マップ不要」「単純・安全」が目的

注意：
  - 本ヘッダは “型定義と作成 API の宣言” だけ。実装は .cpp 側の CreateMesh()。
  - インデックスは 32bit（DXGI_FORMAT_R32_UINT）固定。
    65k 未満しか使わないなら 16bit 化（R16_UINT）でメモリ/帯域削減可。
  - Upload ヒープは CPU から書き換え可能だが、描画時も L0/L1 キャッシュ経由で
    読まれるため、巨大メッシュの常時使用には非推奨（STATIC データは Default ヒープ推奨）。
  - Default ヒープに置きたい場合は、Upload ステージング＋コピーコマンドが必要（別実装）。
===============================================================================
*/

// ------------------------------------------------------------
// 頂点フォーマット（例）
//  - 位置 (px,py,pz)
//  - 法線 (nx,ny,nz)
//  - 色   (r,g,b,a)
//  ※ PSO の InputLayout と合わせること！
// ------------------------------------------------------------
struct Vertex
{
    float px, py, pz;
    float nx, ny, nz;
    float r, g, b, a;
};

// ------------------------------------------------------------
// CPU 側メッシュ
//  - 頂点配列／インデックス配列のみを保持（STL 管理）
//  - インデックスは 32bit（unsigned int）
// ------------------------------------------------------------
struct MeshData
{
    std::vector<Vertex>       Vertices;
    std::vector<unsigned int> Indices;
};

// ------------------------------------------------------------
// GPU 側メッシュ
//  - VB/IB の ID3D12Resource と、IA 用ビューを保持
//  - indexCount は DrawIndexedInstanced の引数にそのまま使用可能
// ------------------------------------------------------------
struct MeshGPU
{
    Microsoft::WRL::ComPtr<ID3D12Resource> vb;  // Upload ヒープの VB
    Microsoft::WRL::ComPtr<ID3D12Resource> ib;  // Upload ヒープの IB
    D3D12_VERTEX_BUFFER_VIEW vbv{};              // IASetVertexBuffers 用
    D3D12_INDEX_BUFFER_VIEW  ibv{};              // IASetIndexBuffer 用
    UINT indexCount = 0;                         // Draw に使う総インデックス数
};

/*
-------------------------------------------------------------------------------
CreateMesh
  概要：
    - 与えられた CPU 側 MeshData を使って、Upload ヒープの VB/IB を確保し、
      中身を Map/コピーして MeshGPU を初期化するユーティリティ。
  引数：
    - dev : ID3D12Device*
    - src : CPU 側メッシュ（Vertices / Indices が空でないこと）
    - out : 作成された GPU リソースとビューが格納される
  戻り値：
    - 成功 true / 失敗 false（デバイス作成失敗、Map 失敗、入力が空など）
  使い方（例）：
    MeshGPU gpu;
    MeshData cpu = LoadMyMesh();
    if (CreateMesh(device, cpu, gpu)) {
        cmd->IASetVertexBuffers(0, 1, &gpu.vbv);
        cmd->IASetIndexBuffer(&gpu.ibv);
        cmd->DrawIndexedInstanced(gpu.indexCount, 1, 0, 0, 0);
    }

  メモ：
    - Upload ヒープなのでそのまま Draw 可能（小規模メッシュ/ツール/デバッグ向け）。
    - 大量 or 高頻度描画は Default ヒープへ移す設計を検討（パフォーマンス向上）。
-------------------------------------------------------------------------------
*/
bool CreateMesh(ID3D12Device* dev, const MeshData& src, MeshGPU& out);
