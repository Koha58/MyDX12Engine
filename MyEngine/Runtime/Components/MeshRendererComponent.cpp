#include "Components/MeshRendererComponent.h"
#include "../D3D12Renderer.h"
#include "Scene/GameObject.h"
#include <Windows.h> // OutputDebugStringA

/*
    MeshRendererComponent
    ----------------------------------------------------------------------------
    役割：
      - GameObject に紐づくメッシュ（頂点/インデックス）を GPU に渡して描画するための“橋渡し”。
      - 実際のドローコール（パイプライン設定やコマンド発行）は D3D12Renderer 側に委譲する。
      - 本コンポーネントは「どのメッシュを描くか」と「IB/VB のビュー情報」を保持する。

    データの流れ（典型）：
      1) SetMesh() で CPU 側の MeshData を受け取る（頂点/インデックス配列）。
      2) D3D12Renderer::CreateMeshRendererResources() などで
         - Upload ヒープへ頂点/インデックスをコピー
         - ID3D12Resource と D3D12_*_BUFFER_VIEW を本コンポーネントへ設定
      3) Render() が呼ばれたら、レンダラへ自身(this)を渡し、DrawMesh(this) を実行。
         DrawMesh は IA セット＆DrawIndexedInstanced を発行する。

    注意：
      - IndexCount は Draw のインデックス数。SetMesh() 時点では CPU メッシュから初期化。
        最終的には GPU リソース生成後も一致している必要がある。
      - VertexBuffer/IndexBuffer リソース自体の寿命は、外部（レンダラ側の破棄ルーチン）で管理。
*/

MeshRendererComponent::MeshRendererComponent()
    : Component(ComponentType::MeshRenderer), IndexCount(0)
{
    // 安全のためビュー構造体をゼロ初期化（未設定アクセス防止）
    ZeroMemory(&VertexBufferView, sizeof(VertexBufferView));
    ZeroMemory(&IndexBufferView, sizeof(IndexBufferView));
}

void MeshRendererComponent::SetMesh(const MeshData& meshData)
{
    // 1) CPU 側コピー（オリジナルがスコープアウトしても参照を維持）
    //    → この後、レンダラが CreateMeshRendererResources() で GPU 転送する想定。
    m_MeshData = meshData;

    // 2) インデックス数を更新（描画時の DrawIndexedInstanced で使用）
    IndexCount = static_cast<UINT>(meshData.Indices.size());
}

void MeshRendererComponent::Render(D3D12Renderer* renderer)
{
    // 所有 GameObject が存在し、かつアクティブでなければ描画しない
    auto owner = m_Owner.lock();
    if (!owner || !owner->IsActive()) return;

    // 実際の描画はレンダラへ委譲（PSO/ルートシグネチャ/CBV 設定等はレンダラ側）
    // ここでは自分自身（VBV/IBV/IndexCount を持つ）を引数に渡すだけ。
    if (renderer) {
        renderer->DrawMesh(this);
    }
}
