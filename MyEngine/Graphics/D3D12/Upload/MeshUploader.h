#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <vector>

/*
===============================================================================
Mesh / Vertex ����̍ŏ���`�iDX12 �����j
-------------------------------------------------------------------------------
�����F
  - CPU �����b�V���iMeshData�j�� GPU �����\�[�X�iMeshGPU�j�𕪂��ĊǗ�
  - CreateMesh() �� Upload �q�[�v�� VB/IB ���m�ۂ��� CPU��GPU �ɃR�s�[
    �� �{�����́u�`��܂ŏ펞�}�b�v�s�v�v�u�P���E���S�v���ړI

���ӁF
  - �{�w�b�_�� �g�^��`�ƍ쐬 API �̐錾�h �����B������ .cpp ���� CreateMesh()�B
  - �C���f�b�N�X�� 32bit�iDXGI_FORMAT_R32_UINT�j�Œ�B
    65k ���������g��Ȃ��Ȃ� 16bit ���iR16_UINT�j�Ń�����/�ш�팸�B
  - Upload �q�[�v�� CPU ���珑�������\�����A�`�掞�� L0/L1 �L���b�V���o�R��
    �ǂ܂�邽�߁A���僁�b�V���̏펞�g�p�ɂ͔񐄏��iSTATIC �f�[�^�� Default �q�[�v�����j�B
  - Default �q�[�v�ɒu�������ꍇ�́AUpload �X�e�[�W���O�{�R�s�[�R�}���h���K�v�i�ʎ����j�B
===============================================================================
*/

// ------------------------------------------------------------
// ���_�t�H�[�}�b�g�i��j
//  - �ʒu (px,py,pz)
//  - �@�� (nx,ny,nz)
//  - �F   (r,g,b,a)
//  �� PSO �� InputLayout �ƍ��킹�邱�ƁI
// ------------------------------------------------------------
struct Vertex
{
    float px, py, pz;
    float nx, ny, nz;
    float r, g, b, a;
};

// ------------------------------------------------------------
// CPU �����b�V��
//  - ���_�z��^�C���f�b�N�X�z��݂̂�ێ��iSTL �Ǘ��j
//  - �C���f�b�N�X�� 32bit�iunsigned int�j
// ------------------------------------------------------------
struct MeshData
{
    std::vector<Vertex>       Vertices;
    std::vector<unsigned int> Indices;
};

// ------------------------------------------------------------
// GPU �����b�V��
//  - VB/IB �� ID3D12Resource �ƁAIA �p�r���[��ێ�
//  - indexCount �� DrawIndexedInstanced �̈����ɂ��̂܂܎g�p�\
// ------------------------------------------------------------
struct MeshGPU
{
    Microsoft::WRL::ComPtr<ID3D12Resource> vb;  // Upload �q�[�v�� VB
    Microsoft::WRL::ComPtr<ID3D12Resource> ib;  // Upload �q�[�v�� IB
    D3D12_VERTEX_BUFFER_VIEW vbv{};              // IASetVertexBuffers �p
    D3D12_INDEX_BUFFER_VIEW  ibv{};              // IASetIndexBuffer �p
    UINT indexCount = 0;                         // Draw �Ɏg�����C���f�b�N�X��
};

/*
-------------------------------------------------------------------------------
CreateMesh
  �T�v�F
    - �^����ꂽ CPU �� MeshData ���g���āAUpload �q�[�v�� VB/IB ���m�ۂ��A
      ���g�� Map/�R�s�[���� MeshGPU �����������郆�[�e�B���e�B�B
  �����F
    - dev : ID3D12Device*
    - src : CPU �����b�V���iVertices / Indices ����łȂ����Ɓj
    - out : �쐬���ꂽ GPU ���\�[�X�ƃr���[���i�[�����
  �߂�l�F
    - ���� true / ���s false�i�f�o�C�X�쐬���s�AMap ���s�A���͂���Ȃǁj
  �g�����i��j�F
    MeshGPU gpu;
    MeshData cpu = LoadMyMesh();
    if (CreateMesh(device, cpu, gpu)) {
        cmd->IASetVertexBuffers(0, 1, &gpu.vbv);
        cmd->IASetIndexBuffer(&gpu.ibv);
        cmd->DrawIndexedInstanced(gpu.indexCount, 1, 0, 0, 0);
    }

  �����F
    - Upload �q�[�v�Ȃ̂ł��̂܂� Draw �\�i���K�̓��b�V��/�c�[��/�f�o�b�O�����j�B
    - ��� or ���p�x�`��� Default �q�[�v�ֈڂ��݌v�������i�p�t�H�[�}���X����j�B
-------------------------------------------------------------------------------
*/
bool CreateMesh(ID3D12Device* dev, const MeshData& src, MeshGPU& out);
