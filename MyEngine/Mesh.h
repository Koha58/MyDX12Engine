#pragma once
#include <vector>
#include <DirectXMath.h>
#include <string>
#include <wrl/client.h> // ComPtr

// �� Direct3D 12 �̌^��`�̂��߂ɒǉ�
#include <d3d12.h>
#include "d3dx12.h" // ����d3dx12.h���g�p���Ă���Ȃ�

// Component �N���X����`����Ă��� GameObject.h ���C���N���[�h
#include "GameObject.h" 

// �O���錾: D3D12Renderer �� MeshRendererComponent �̃t�����h�ɂ��邽�߂ɕK�v
class D3D12Renderer;

// ���_�\����
struct Vertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT4 Color;
    // DirectX::XMFLOAT2 TexCoord; // �e�N�X�`���Ή����ɒǉ�
    // DirectX::XMFLOAT3 Normal;   // ���C�e�B���O�Ή����ɒǉ�
};

// ���b�V���f�[�^
struct MeshData
{
    std::vector<Vertex> Vertices;
    std::vector<unsigned int> Indices; // unsigned short ���� unsigned int �ɕύX�i�C���f�b�N�X���̏���𑝂₷���߁j
};

// =====================================================================================
// MeshRendererComponent (GameObject�Ƀ��b�V����`�悳���邽�߂̃R���|�[�l���g)
// =====================================================================================
class MeshRendererComponent : public Component
{
    // D3D12Renderer �� MeshRendererComponent �� private �����o�[�ɃA�N�Z�X�ł���悤�ɂ���
    friend class D3D12Renderer;

public:
    // �R���|�[�l���g�̌^���ʎq
    static ComponentType StaticType() { return MeshRenderer; }

    MeshRendererComponent(); // �R���X�g���N�^�� Component �̃R���X�g���N�^���Ăяo��
    virtual ~MeshRendererComponent() = default;

    void SetMesh(const MeshData& meshData);

    // GPU���\�[�X�i���_�o�b�t�@�A�C���f�b�N�X�o�b�t�@�j�ւ̃|�C���^
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer;
    D3D12_INDEX_BUFFER_VIEW IndexBufferView;

    UINT IndexCount; // �`�悷��C���f�b�N�X��

    // ��������D3D12Renderer::CreateMeshRendererResources �ōs��
    virtual void Initialize() override {}

private:
    // ���̃����o�[�� private �̂܂܂�OK�B�t�����h�錾�� D3D12Renderer ����A�N�Z�X�\�ɂȂ�
    MeshData m_MeshData; // ���b�V���f�[�^���̂�CPU���ŕێ����邱�Ƃ��ł���
};