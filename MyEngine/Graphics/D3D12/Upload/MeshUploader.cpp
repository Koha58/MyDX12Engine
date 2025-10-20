#include "MeshUploader.h"
#include "Debug/DxDebug.h"
#include <cstring>
#include <d3dx12.h>

using Microsoft::WRL::ComPtr;

/*
    CreateMesh
    ----------------------------------------------------------------------------
    �ړI�F
      - CPU ���̃��b�V���f�[�^ (MeshData: ���_�z��E�C���f�b�N�X�z��) ��
        DX12 �� Upload �q�[�v��̃o�b�t�@�ɃR�s�[���āA�`��ɕK�v��
        GPU ���\�[�X�r���[ (VBV/IBV) ���Z�b�g�A�b�v����B

    �����F
      - �q�[�v�� UPLOAD�iCPU �������݉EGPU �ǂݎ��j���g�p�B
        ���_����X�V�p�x�����Ȃ��J���c�[���p�r/���K�̓��b�V���Ɍ����B
        �p�ɂɕ`��/�傫�����b�V���� DEFAULT �q�[�v + �A�b�v���[�h�ꎞ�o�b�t�@�����B
      - ���s���� dxdbg::LogHRESULTError �� HRESULT ���f�o�b�O�o�͂��Afalse ��Ԃ��B
      - out�iMeshGPU�j�Ɉȉ����l�߂�F
          * vb / ib           : ComPtr<ID3D12Resource>�iUpload �o�b�t�@�{�́j
          * vbv / ibv         : D3D12_VERTEX_BUFFER_VIEW / D3D12_INDEX_BUFFER_VIEW
          * indexCount        : �`��Ɏg���C���f�b�N�X��

    �O��F
      - dev != nullptr
      - src.Vertices / src.Indices ����łȂ�
      - Vertex �̃��C�A�E�g�� PSO �� InputLayout �ƈ�v���Ă��邱��

    ���ӁF
      - UPLOAD �q�[�v�� CPU �A�N�Z�X�\�Ȃ��߁AGPU ����̓ǂݏo���͔�r�I�x���B
        �p�t�H�[�}���X�d���̍ŏI�A�v���ł́ADEFAULT �q�[�v�փR�s�[���Ďg�����ƁB
      - Map/Unmap �͈̔� (CD3DX12_RANGE) �� 0,0 �ɂ���� CPU �������ݐ�p�̈Ӑ}�ɂȂ�A
        �ǂݖ߂��͂��Ȃ����Ƃ��h���C�o�֓`������i�œK���j�B
*/
bool CreateMesh(ID3D12Device* dev, const MeshData& src, MeshGPU& out)
{
    // ���̓`�F�b�N�F�󃁃b�V���͐����s�v
    if (src.Indices.empty() || src.Vertices.empty())
        return false;

    // ���ׂ� Upload �q�[�v�ɍ��i�J���c�[��/�y�ʃ��b�V�������̊ȈՃp�X�j
    // �����I�� DEFAULT �q�[�v�ֈڍs����Ȃ�A�������i�K�]���ɒu�������B
    HRESULT hr;
    D3D12_HEAP_PROPERTIES heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // ==============================
    // ���_�o�b�t�@ (VB) �̐���
    // ==============================
    const UINT vbSize = static_cast<UINT>(src.Vertices.size() * sizeof(Vertex));

    // 1) ���\�[�X�쐬�i�o�b�t�@�j
    {
        D3D12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);
        hr = dev->CreateCommittedResource(
            &heap,                             // UPLOAD �q�[�v
            D3D12_HEAP_FLAG_NONE,
            &vbDesc,                           // �o�b�t�@
            D3D12_RESOURCE_STATE_GENERIC_READ, // CPU �������� & GPU �ǂݏo��
            nullptr,
            IID_PPV_ARGS(&out.vb));
        dxdbg::LogHRESULTError(hr, "Create VB");
        if (FAILED(hr)) return false;
    }

    // 2) CPU ����f�[�^���������ށiMap �� memcpy �� Unmap�j
    {
        UINT8* dst = nullptr;
        CD3DX12_RANGE rr(0, 0); // �ǂݖ߂������iCPU �������ݐ�p�̈Ӑ}�j
        hr = out.vb->Map(0, &rr, reinterpret_cast<void**>(&dst));
        dxdbg::LogHRESULTError(hr, "VB Map");
        if (FAILED(hr)) return false;

        // ���_�f�[�^�S�̂� Upload �q�[�v�փR�s�[
        std::memcpy(dst, src.Vertices.data(), vbSize);

        // �������݊����i�ǂݖ߂��Ȃ����ߑ������� nullptr�j
        out.vb->Unmap(0, nullptr);
    }

    // 3) VBV �̃Z�b�g�A�b�v�iIA �ɓn�����߂̃r���[���j
    out.vbv.BufferLocation = out.vb->GetGPUVirtualAddress(); // GPU ���z�A�h���X
    out.vbv.StrideInBytes = sizeof(Vertex);                 // 1 ���_�̃o�C�g��
    out.vbv.SizeInBytes = vbSize;                         // �o�b�t�@�S�̂̃T�C�Y

    // ==============================
    // �C���f�b�N�X�o�b�t�@ (IB) �̐���
    // ==============================
    const UINT ibSize = static_cast<UINT>(src.Indices.size() * sizeof(unsigned int));

    // 1) ���\�[�X�쐬�i�o�b�t�@�j
    {
        D3D12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);
        hr = dev->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &ibDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&out.ib));
        dxdbg::LogHRESULTError(hr, "Create IB");
        if (FAILED(hr)) return false;
    }

    // 2) CPU ����f�[�^���������ށiMap �� memcpy �� Unmap�j
    {
        UINT8* dst = nullptr;
        CD3DX12_RANGE rr(0, 0);
        hr = out.ib->Map(0, &rr, reinterpret_cast<void**>(&dst));
        dxdbg::LogHRESULTError(hr, "IB Map");
        if (FAILED(hr)) return false;

        std::memcpy(dst, src.Indices.data(), ibSize);
        out.ib->Unmap(0, nullptr);
    }

    // 3) IBV �̃Z�b�g�A�b�v�iIA �ɓn�����߂̃r���[���j
    out.ibv.BufferLocation = out.ib->GetGPUVirtualAddress();
    out.ibv.Format = DXGI_FORMAT_R32_UINT;  // 32bit �C���f�b�N�X�isrc �� uint32 �O��j
    out.ibv.SizeInBytes = ibSize;

    // ==============================
    // ���b�V����{���
    // ==============================
    out.indexCount = static_cast<UINT>(src.Indices.size()); // DrawIndexedInstanced �ɓn����

    /*
        �g���������i�Ăяo�����j�F
          cmd->IASetVertexBuffers(0, 1, &out.vbv);
          cmd->IASetIndexBuffer(&out.ibv);
          cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
          cmd->DrawIndexedInstanced(out.indexCount, 1, 0, 0, 0);
    */

    return true;
}
