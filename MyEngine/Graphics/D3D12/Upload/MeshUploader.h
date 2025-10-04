#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <vector>

struct Vertex { float px, py, pz; float nx, ny, nz; float r, g, b, a; }; // —á
struct MeshData {
    std::vector<Vertex>       Vertices;
    std::vector<unsigned int> Indices;
};

struct MeshGPU {
    Microsoft::WRL::ComPtr<ID3D12Resource> vb, ib;
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    D3D12_INDEX_BUFFER_VIEW  ibv{};
    UINT indexCount = 0;
};

bool CreateMesh(ID3D12Device* dev, const MeshData& src, MeshGPU& out);

