#pragma once
#include "Component.h"
#include "Mesh.h"
#include <vector>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <d3d12.h>
#include "d3dx12.h"


class D3D12Renderer;

struct Vertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT4 Color;
};

struct MeshData
{
    std::vector<Vertex> Vertices;
    std::vector<unsigned int> Indices;
};

class D3D12Renderer;

class MeshRendererComponent : public Component
{
    friend class D3D12Renderer;

public:
    static ComponentType StaticType() { return MeshRenderer; }

    MeshRendererComponent();
    virtual ~MeshRendererComponent() = default;

    void SetMesh(const MeshData& meshData);

    // ï`âÊèàóùÇí«â¡
    void Render(D3D12Renderer* renderer);

private:
    MeshData m_MeshData;

public:
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer;
    D3D12_INDEX_BUFFER_VIEW IndexBufferView;
    UINT IndexCount;
};
