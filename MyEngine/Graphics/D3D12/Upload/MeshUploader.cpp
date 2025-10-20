#include "MeshUploader.h"
#include "Debug/DxDebug.h"
#include <cstring>
#include <d3dx12.h>

using Microsoft::WRL::ComPtr;

/*
    CreateMesh
    ----------------------------------------------------------------------------
    目的：
      - CPU 側のメッシュデータ (MeshData: 頂点配列・インデックス配列) を
        DX12 の Upload ヒープ上のバッファにコピーして、描画に必要な
        GPU リソースビュー (VBV/IBV) をセットアップする。

    特徴：
      - ヒープは UPLOAD（CPU 書き込み可・GPU 読み取り可）を使用。
        頂点数や更新頻度が少ない開発ツール用途/小規模メッシュに向く。
        頻繁に描く/大きいメッシュは DEFAULT ヒープ + アップロード一時バッファ推奨。
      - 失敗時は dxdbg::LogHRESULTError で HRESULT をデバッグ出力し、false を返す。
      - out（MeshGPU）に以下を詰める：
          * vb / ib           : ComPtr<ID3D12Resource>（Upload バッファ本体）
          * vbv / ibv         : D3D12_VERTEX_BUFFER_VIEW / D3D12_INDEX_BUFFER_VIEW
          * indexCount        : 描画に使うインデックス数

    前提：
      - dev != nullptr
      - src.Vertices / src.Indices が空でない
      - Vertex のレイアウトは PSO の InputLayout と一致していること

    注意：
      - UPLOAD ヒープは CPU アクセス可能なため、GPU からの読み出しは比較的遅い。
        パフォーマンス重視の最終アプリでは、DEFAULT ヒープへコピーして使うこと。
      - Map/Unmap の範囲 (CD3DX12_RANGE) を 0,0 にすると CPU 書き込み専用の意図になり、
        読み戻しはしないことをドライバへ伝えられる（最適化）。
*/
bool CreateMesh(ID3D12Device* dev, const MeshData& src, MeshGPU& out)
{
    // 入力チェック：空メッシュは生成不要
    if (src.Indices.empty() || src.Vertices.empty())
        return false;

    // すべて Upload ヒープに作る（開発ツール/軽量メッシュ向けの簡易パス）
    // 将来的に DEFAULT ヒープへ移行するなら、ここを二段階転送に置き換え。
    HRESULT hr;
    D3D12_HEAP_PROPERTIES heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // ==============================
    // 頂点バッファ (VB) の生成
    // ==============================
    const UINT vbSize = static_cast<UINT>(src.Vertices.size() * sizeof(Vertex));

    // 1) リソース作成（バッファ）
    {
        D3D12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);
        hr = dev->CreateCommittedResource(
            &heap,                             // UPLOAD ヒープ
            D3D12_HEAP_FLAG_NONE,
            &vbDesc,                           // バッファ
            D3D12_RESOURCE_STATE_GENERIC_READ, // CPU 書き込み & GPU 読み出し
            nullptr,
            IID_PPV_ARGS(&out.vb));
        dxdbg::LogHRESULTError(hr, "Create VB");
        if (FAILED(hr)) return false;
    }

    // 2) CPU からデータを書き込む（Map → memcpy → Unmap）
    {
        UINT8* dst = nullptr;
        CD3DX12_RANGE rr(0, 0); // 読み戻し無し（CPU 書き込み専用の意図）
        hr = out.vb->Map(0, &rr, reinterpret_cast<void**>(&dst));
        dxdbg::LogHRESULTError(hr, "VB Map");
        if (FAILED(hr)) return false;

        // 頂点データ全体を Upload ヒープへコピー
        std::memcpy(dst, src.Vertices.data(), vbSize);

        // 書き込み完了（読み戻さないため第二引数は nullptr）
        out.vb->Unmap(0, nullptr);
    }

    // 3) VBV のセットアップ（IA に渡すためのビュー情報）
    out.vbv.BufferLocation = out.vb->GetGPUVirtualAddress(); // GPU 仮想アドレス
    out.vbv.StrideInBytes = sizeof(Vertex);                 // 1 頂点のバイト数
    out.vbv.SizeInBytes = vbSize;                         // バッファ全体のサイズ

    // ==============================
    // インデックスバッファ (IB) の生成
    // ==============================
    const UINT ibSize = static_cast<UINT>(src.Indices.size() * sizeof(unsigned int));

    // 1) リソース作成（バッファ）
    {
        D3D12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);
        hr = dev->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &ibDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&out.ib));
        dxdbg::LogHRESULTError(hr, "Create IB");
        if (FAILED(hr)) return false;
    }

    // 2) CPU からデータを書き込む（Map → memcpy → Unmap）
    {
        UINT8* dst = nullptr;
        CD3DX12_RANGE rr(0, 0);
        hr = out.ib->Map(0, &rr, reinterpret_cast<void**>(&dst));
        dxdbg::LogHRESULTError(hr, "IB Map");
        if (FAILED(hr)) return false;

        std::memcpy(dst, src.Indices.data(), ibSize);
        out.ib->Unmap(0, nullptr);
    }

    // 3) IBV のセットアップ（IA に渡すためのビュー情報）
    out.ibv.BufferLocation = out.ib->GetGPUVirtualAddress();
    out.ibv.Format = DXGI_FORMAT_R32_UINT;  // 32bit インデックス（src が uint32 前提）
    out.ibv.SizeInBytes = ibSize;

    // ==============================
    // メッシュ基本情報
    // ==============================
    out.indexCount = static_cast<UINT>(src.Indices.size()); // DrawIndexedInstanced に渡す数

    /*
        使い方メモ（呼び出し側）：
          cmd->IASetVertexBuffers(0, 1, &out.vbv);
          cmd->IASetIndexBuffer(&out.ibv);
          cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
          cmd->DrawIndexedInstanced(out.indexCount, 1, 0, 0, 0);
    */

    return true;
}
