#pragma once
#include "Components/Component.h"
#include "Assets/Mesh.h"
#include <wrl/client.h>
#include <d3d12.h>

/*
================================================================================
 MeshRendererComponent
--------------------------------------------------------------------------------
目的
- 所有 GameObject のメッシュを描画するための最小コンポーネント。
- CPU 側のメッシュデータ（MeshData）を保持し、D3D12 の VB/IB を Renderer 側で生成。
- 実際の DrawIndexedInstanced 呼び出しは D3D12Renderer に委譲（職責分離）。

設計メモ
- GPU リソース（VB/IB）は本クラスでは作らず、D3D12Renderer が生成・更新する設計。
  → そのため D3D12Renderer を friend にして内部バッファへ直接アクセス可能にする。
- SetMesh() は CPU 側コピーのみ（頂点編集ツール等から更新しやすくする）。
  GPU 転送が必要になったら Renderer 側の CreateMeshRendererResources() を呼ぶ。
- Render() は「所有 GameObject が ActiveInHierarchy のときだけ」描画依頼を出す。
  実ドローは D3D12Renderer::DrawMesh() が行う。

使用手順（例）
1) auto mr = go->AddComponent<MeshRendererComponent>();
2) mr->SetMesh(meshData);                           // CPU 側に保持
3) renderer->CreateMeshRendererResources(mr);       // VB/IB を作成（GPU 転送）
4) 毎フレーム：mr->Render(renderer);               // Draw の委譲

注意点
- IndexCount は 32bit（DXGI_FORMAT_R32_UINT）前提で計算している。
  16bit を使いたい場合はビューの Format とデータ型を合わせること。
- CPU Mesh と GPU リソースの同期は明示的（SetMesh だけでは描画されない）。
================================================================================
*/

class D3D12Renderer;

class MeshRendererComponent : public Component
{
    // D3D12Renderer が GPU リソースを直接設定できるようにする
    friend class D3D12Renderer;

public:
    // 型情報（エディタ表示や型分岐に使用）
    static ComponentType StaticType() { return ComponentType::MeshRenderer; }

    MeshRendererComponent();
    ~MeshRendererComponent() override = default;

    //-------------------------------------------------------------------------
    // CPU 側メッシュを設定（GPU 転送は行わない）
    //   - 設定後は Renderer 側の CreateMeshRendererResources() を呼んで VB/IB を更新すること
    //-------------------------------------------------------------------------
    void SetMesh(const MeshData& meshData);

    //-------------------------------------------------------------------------
    // 描画（所有 GameObject が Active のときのみ Renderer に委譲）
    //   - 実際の IA セット & DrawIndexedInstanced は D3D12Renderer が担当
    //-------------------------------------------------------------------------
    void Render(D3D12Renderer* renderer) override;

    // 読み取り用アクセサ（エディタやデバッガ用）
    const MeshData& GetMeshData() const { return m_MeshData; }
    MeshData& GetMeshData() { return m_MeshData; }

    //-------------------------------------------------------------------------
    // GPU リソース（Renderer が生成/更新）
    //   - VertexBuffer / IndexBuffer …… ComPtr で所有
    //   - *_VIEW は IA にバインドするためのビューデータ
    //   - IndexCount は DrawIndexedInstanced のインデックス数
    //-------------------------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;     // 頂点バッファ（GPU）
    D3D12_VERTEX_BUFFER_VIEW               VertexBufferView{}; // VBV（Stride, Size, GPU VA）
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer;      // インデックスバッファ（GPU）
    D3D12_INDEX_BUFFER_VIEW                IndexBufferView{};  // IBV（Format, Size, GPU VA）
    UINT                                   IndexCount = 0;      // インデックス総数

private:
    // CPU 側メッシュ（エディタ編集や再アップロードの元データ）
    MeshData m_MeshData;
};
