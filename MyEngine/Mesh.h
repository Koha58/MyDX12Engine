#pragma once
#include <vector>
#include <DirectXMath.h>

// --- ���_�\���� ---
struct Vertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT4 Color;
};

// --- ���b�V���f�[�^�\���� ---
struct MeshData
{
    std::vector<Vertex> Vertices;
    std::vector<unsigned int> Indices;
};
