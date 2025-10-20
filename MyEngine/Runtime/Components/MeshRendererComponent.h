#pragma once
#include "Components/Component.h"
#include "Assets/Mesh.h"
#include <wrl/client.h>
#include <d3d12.h>

/*
================================================================================
 MeshRendererComponent
--------------------------------------------------------------------------------
�ړI
- ���L GameObject �̃��b�V����`�悷�邽�߂̍ŏ��R���|�[�l���g�B
- CPU ���̃��b�V���f�[�^�iMeshData�j��ێ����AD3D12 �� VB/IB �� Renderer ���Ő����B
- ���ۂ� DrawIndexedInstanced �Ăяo���� D3D12Renderer �ɈϏ��i�E�ӕ����j�B

�݌v����
- GPU ���\�[�X�iVB/IB�j�͖{�N���X�ł͍�炸�AD3D12Renderer �������E�X�V����݌v�B
  �� ���̂��� D3D12Renderer �� friend �ɂ��ē����o�b�t�@�֒��ڃA�N�Z�X�\�ɂ���B
- SetMesh() �� CPU ���R�s�[�̂݁i���_�ҏW�c�[��������X�V���₷������j�B
  GPU �]�����K�v�ɂȂ����� Renderer ���� CreateMeshRendererResources() ���ĂԁB
- Render() �́u���L GameObject �� ActiveInHierarchy �̂Ƃ������v�`��˗����o���B
  ���h���[�� D3D12Renderer::DrawMesh() ���s���B

�g�p�菇�i��j
1) auto mr = go->AddComponent<MeshRendererComponent>();
2) mr->SetMesh(meshData);                           // CPU ���ɕێ�
3) renderer->CreateMeshRendererResources(mr);       // VB/IB ���쐬�iGPU �]���j
4) ���t���[���Fmr->Render(renderer);               // Draw �̈Ϗ�

���ӓ_
- IndexCount �� 32bit�iDXGI_FORMAT_R32_UINT�j�O��Ōv�Z���Ă���B
  16bit ���g�������ꍇ�̓r���[�� Format �ƃf�[�^�^�����킹�邱�ƁB
- CPU Mesh �� GPU ���\�[�X�̓����͖����I�iSetMesh �����ł͕`�悳��Ȃ��j�B
================================================================================
*/

class D3D12Renderer;

class MeshRendererComponent : public Component
{
    // D3D12Renderer �� GPU ���\�[�X�𒼐ڐݒ�ł���悤�ɂ���
    friend class D3D12Renderer;

public:
    // �^���i�G�f�B�^�\����^����Ɏg�p�j
    static ComponentType StaticType() { return ComponentType::MeshRenderer; }

    MeshRendererComponent();
    ~MeshRendererComponent() override = default;

    //-------------------------------------------------------------------------
    // CPU �����b�V����ݒ�iGPU �]���͍s��Ȃ��j
    //   - �ݒ��� Renderer ���� CreateMeshRendererResources() ���Ă�� VB/IB ���X�V���邱��
    //-------------------------------------------------------------------------
    void SetMesh(const MeshData& meshData);

    //-------------------------------------------------------------------------
    // �`��i���L GameObject �� Active �̂Ƃ��̂� Renderer �ɈϏ��j
    //   - ���ۂ� IA �Z�b�g & DrawIndexedInstanced �� D3D12Renderer ���S��
    //-------------------------------------------------------------------------
    void Render(D3D12Renderer* renderer) override;

    // �ǂݎ��p�A�N�Z�T�i�G�f�B�^��f�o�b�K�p�j
    const MeshData& GetMeshData() const { return m_MeshData; }
    MeshData& GetMeshData() { return m_MeshData; }

    //-------------------------------------------------------------------------
    // GPU ���\�[�X�iRenderer ������/�X�V�j
    //   - VertexBuffer / IndexBuffer �c�c ComPtr �ŏ��L
    //   - *_VIEW �� IA �Ƀo�C���h���邽�߂̃r���[�f�[�^
    //   - IndexCount �� DrawIndexedInstanced �̃C���f�b�N�X��
    //-------------------------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;     // ���_�o�b�t�@�iGPU�j
    D3D12_VERTEX_BUFFER_VIEW               VertexBufferView{}; // VBV�iStride, Size, GPU VA�j
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer;      // �C���f�b�N�X�o�b�t�@�iGPU�j
    D3D12_INDEX_BUFFER_VIEW                IndexBufferView{};  // IBV�iFormat, Size, GPU VA�j
    UINT                                   IndexCount = 0;      // �C���f�b�N�X����

private:
    // CPU �����b�V���i�G�f�B�^�ҏW��ăA�b�v���[�h�̌��f�[�^�j
    MeshData m_MeshData;
};
