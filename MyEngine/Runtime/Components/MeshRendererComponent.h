#pragma once
#include "Components/Component.h"
#include "Assets/Mesh.h"
#include <wrl/client.h>
#include <d3d12.h>

class D3D12Renderer;

class MeshRendererComponent : public Component
{
    // D3D12Renderer �� GPU ���\�[�X�𒼐ږ��߂邽�߂̋���
    friend class D3D12Renderer;

public:
    static ComponentType StaticType() { return ComponentType::MeshRenderer; }

    MeshRendererComponent();
    ~MeshRendererComponent() override = default;

    // CPU �����b�V����ݒ�
    void SetMesh(const MeshData& meshData);

    // �����_���ɕ`��˗��iGameObject �� Active �̂Ƃ��̂݁j
    void Render(D3D12Renderer* renderer) override;

    // �ǂݎ��p�A�N�Z�T
    const MeshData& GetMeshData() const { return m_MeshData; }
    MeshData& GetMeshData() { return m_MeshData; }

    // ---- GPU ���\�[�X�iRenderer ������/�X�V�j----
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;  // VB
    D3D12_VERTEX_BUFFER_VIEW               VertexBufferView{};
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer;   // IB
    D3D12_INDEX_BUFFER_VIEW                IndexBufferView{};
    UINT                                   IndexCount = 0; // �Q�Ɨp

private:
    // �������ɂ���1�񂾂���`�i�d���������j
    MeshData m_MeshData;
};
