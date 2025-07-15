#include "MeshRendererComponent.h"
// #include "GameObject.h" // Only include if GameObject's full definition is needed here

MeshRendererComponent::MeshRendererComponent(std::shared_ptr<GameObject> owner)
    : m_Owner(owner),
    IndexCount(0) // Initialize IndexCount
{
    // Constructor logic
}

MeshRendererComponent::~MeshRendererComponent()
{
    // Destructor logic (ComPtrs handle D3D12 resource release automatically)
}

void MeshRendererComponent::SetMesh(const Mesh& mesh)
{
    m_MeshData = mesh; // Copy the mesh data
    IndexCount = (UINT)m_MeshData.Indices.size(); // Set the index count
}