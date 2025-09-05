#include "MeshRendererComponent.h"
#include "D3D12Renderer.h"
#include "GameObject.h"

MeshRendererComponent::MeshRendererComponent()
    : Component(ComponentType::MeshRenderer), IndexCount(0)
{
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
    auto owner = m_Owner.lock();
    if (owner && owner->IsActive())
    {
        renderer->DrawMesh(this);
    }
}
