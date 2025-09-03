#include "MeshRendererComponent.h"

MeshRendererComponent::MeshRendererComponent()
    : Component(ComponentType::MeshRenderer), IndexCount(0)
{
}

void MeshRendererComponent::SetMesh(const MeshData& meshData)
{
    m_MeshData = meshData;
    IndexCount = static_cast<UINT>(meshData.Indices.size());
}
