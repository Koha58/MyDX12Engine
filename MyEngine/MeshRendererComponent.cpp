#include "MeshRendererComponent.h"
#include "D3D12Renderer.h"
#include "GameObject.h"

MeshRendererComponent::MeshRendererComponent()
    : Component(ComponentType::MeshRenderer), IndexCount(0)
{
    // 必ずゼロ初期化しておく
    ZeroMemory(&VertexBufferView, sizeof(VertexBufferView));
    ZeroMemory(&IndexBufferView, sizeof(IndexBufferView));
}

void MeshRendererComponent::SetMesh(const MeshData& meshData)
{
    m_MeshData = meshData;
    IndexCount = static_cast<UINT>(meshData.Indices.size());
}

void MeshRendererComponent::Render(D3D12Renderer* renderer)
{
    if (!m_Owner || !m_Owner->IsActive()) return;

    renderer->DrawMesh(this); // ✅ Renderer に描画依頼
}
