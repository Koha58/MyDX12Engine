#pragma once
#include <vector>
#include <DirectXMath.h>

// --- 頂点構造体 ---
struct Vertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT4 Color;
};

// --- メッシュデータ構造体 ---
struct MeshData
{
    std::vector<Vertex> Vertices;
    std::vector<unsigned int> Indices;
};
