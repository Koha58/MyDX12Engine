#include "MeshRendererComponent.h"
#include "D3D12Renderer.h"
#include "GameObject.h"

// ===============================================================
// コンストラクタ
// ---------------------------------------------------------------
// ・ComponentType を MeshRenderer に設定
// ・IndexCount を 0 で初期化
// ・VertexBufferView / IndexBufferView をゼロクリアして無効状態に
// ===============================================================
MeshRendererComponent::MeshRendererComponent()
    : Component(ComponentType::MeshRenderer), IndexCount(0)
{
    ZeroMemory(&VertexBufferView, sizeof(VertexBufferView)); // 未使用状態に初期化
    ZeroMemory(&IndexBufferView, sizeof(IndexBufferView));   // 未使用状態に初期化
}

// ===============================================================
// SetMesh
// ---------------------------------------------------------------
// ・描画対象の MeshData を CPU 側に保持する
// ・Indices の数を数えて IndexCount に保存
//   → 後で DrawIndexedInstanced の呼び出しに使う
// ===============================================================
void MeshRendererComponent::SetMesh(const MeshData& meshData)
{
    m_MeshData = meshData; // CPU 側コピーを保持
    IndexCount = static_cast<UINT>(meshData.Indices.size());
}

// ===============================================================
// Render
// ---------------------------------------------------------------
// ・呼び出し元: Scene → GameObject → 各コンポーネント
// ・GameObject がアクティブ状態のときのみ描画処理を行う
// ・描画自体は D3D12Renderer::DrawMesh() に委譲
//   → GPU コマンド発行はレンダラー側の責務
// ===============================================================
void MeshRendererComponent::Render(D3D12Renderer* renderer)
{
    // 所属する GameObject を weak_ptr 経由で取得
    auto owner = m_Owner.lock();

    // オーナーが存在し、かつアクティブなら描画
    if (owner && owner->IsActive())
    {
        // 実際の描画コマンド発行は D3D12Renderer に任せる
        renderer->DrawMesh(this);
    }
}
