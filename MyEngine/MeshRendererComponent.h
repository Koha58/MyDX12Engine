#pragma once
#include "Component.h"
#include "Mesh.h"
#include <wrl/client.h>  // Microsoft::WRL::ComPtr (�X�}�[�g�|�C���^ for COM)
#include <d3d12.h>       // DirectX 12 API

class D3D12Renderer;

// ===============================================================
// MeshRendererComponent
// ---------------------------------------------------------------
// �EGameObject �ɃA�^�b�`���āu���b�V����`�悷��v������S���R���|�[�l���g
// �E�����ɕێ����� MeshData�iCPU ���̃f�[�^�j�� GPU �o�b�t�@�ɕϊ����A�`�悷��
// �ED3D12Renderer �� GPU ���\�[�X������S�����邽�߁Afriend �Ƃ��ăA�N�Z�X�\
// ===============================================================
class MeshRendererComponent : public Component
{
    // D3D12Renderer �� VertexBuffer/IndexBuffer �𒼐ڑ���ł���悤�ɂ���
    friend class D3D12Renderer;

public:
    // �^���ʗp�BComponentType::MeshRenderer ��Ԃ�
    static ComponentType StaticType() { return ComponentType::MeshRenderer; }

    MeshRendererComponent();
    virtual ~MeshRendererComponent() = default;

    // -----------------------------------------------------------
    // SetMesh
    // �E�`��ΏۂƂȂ郁�b�V���f�[�^��ݒ肷��
    // �E������ MeshData �� CPU ���ŕێ�����A��� GPU �o�b�t�@�ɕϊ������
    // -----------------------------------------------------------
    void SetMesh(const MeshData& meshData);

    // -----------------------------------------------------------
    // Render
    // �ED3D12Renderer �ɑ΂��ĕ`����˗�����
    // �E�����I�ɂ� VertexBuffer / IndexBuffer ��p���� DrawCall �𔭍s
    // -----------------------------------------------------------
    void Render(D3D12Renderer* renderer) override;

private:
    // CPU ���̃��b�V���f�[�^
    MeshData m_MeshData;

public:
    // ---------------- GPU �����\�[�X ----------------
    // �EComPtr: COM �I�u�W�F�N�g�̎�������X�}�[�g�|�C���^
    // �ED3D12Renderer::CreateMeshRendererResources() �ɂĎ��ۂɐ��������
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;  // ���_�o�b�t�@ (GPU ��)
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView{};          // ���_�o�b�t�@�r���[�i�`��R�}���h�p�j
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer;   // �C���f�b�N�X�o�b�t�@ (GPU ��)
    D3D12_INDEX_BUFFER_VIEW IndexBufferView{};            // �C���f�b�N�X�o�b�t�@�r���[
    UINT IndexCount = 0;                                  // �C���f�b�N�X���i�O�p�`�`��ɕK�v�j
};
