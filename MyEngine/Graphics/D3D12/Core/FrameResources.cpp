#include "FrameResources.h"
#include "d3dx12.h"

using Microsoft::WRL::ComPtr;

/*
    FrameResources
    ----------------------------------------------------------------------------
    �����F
      �E�t���[�����ƂɕK�v�ȁu�R�}���h�A���P�[�^�v�Ɓu�A�b�v���[�h�pCB�o�b�t�@(UPL)�v��ێ��B
      �E�_�u��/�g���v���o�b�t�@�����O���́A�e�t���[���C���f�b�N�X�ɑ΂��ēƗ��̃��\�[�X�����B
      �EUPL �́u�i���}�b�v�v���� CPU �������݃|�C���^��ێ��i�������݌����d���j�B

    �p��F
      - frameCount  : �X���b�v�`�F�C���̃t���[�����i��F2 or 3�j
      - cbSize      : 1 �I�u�W�F�N�g���g���萔�o�b�t�@�̃T�C�Y�i�o�C�g�j
      - maxObjects  : 1 �t���[���ōX�V������ő�I�u�W�F�N�g���i�X���b�g���j

    ���ӓ_�F
      - CBV �̃n�[�h�A���C�����g�� 256 �o�C�g�BcbSize �� 256 �̔{���ɐ؂�グ�Ďg���B
      - UPL ���t���[�������m�ۂ��A�e�t���[���������̗̈�ɂ̂ݏ������ސ݌v�B
      - Destroy() ���� Map ��K�� Unmap ���Ă��� Reset ����B
*/

bool FrameResources::Initialize(ID3D12Device* dev, UINT frameCount, UINT cbSize, UINT maxObjects)
{
    // �t���[�����i�����O�o�b�t�@�̒i���j
    m_count = frameCount;

    // ===== �d�v�FCBV �� 256B �A���C�����g�ɐ؂�グ =====
    // D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT (256)
    // ��) cbSize=160 �� stride=256, cbSize=300 �� stride=512
    m_cbStride = (cbSize + 255) & ~255u;

    // �t���[�����Ƃ̃��\�[�X�Z�b�g���m��
    m_items.resize(frameCount);

    for (UINT i = 0; i < frameCount; ++i) {
        // --------------------------------------------------------------------
        // 1) �R�}���h�A���P�[�^�iDIRECT�j
        //    �E�e�t���[����p�� 1 ���p�ӁB
        //    �E�ė��p����Ƃ��� Fence �Ŋ�����҂��Ă��� Reset ����̂���ʑw�̐Ӗ��B
        // --------------------------------------------------------------------
        if (FAILED(dev->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_items[i].cmdAlloc))))
        {
            return false;
        }

        // --------------------------------------------------------------------
        // 2) �萔�o�b�t�@�p�̃A�b�v���[�h�q�[�v�i�t���[�� i �p�j
        //    �E�umaxObjects �~ stride�v���̘A���̈���m�ۂ��āA1�t���[���Ŏg���S�I�u�W�F�N�g���������ɋl�߂�B
        //    �EUPLOAD �� CPU �������݉AGPU �ǂݎ��B���t���X�V�Ɍ����B
        //    �EDefault �q�[�v�ƈႢ�A�o���A�Ǘ��͕s�v�i�������ш�͒x���j�B
        // --------------------------------------------------------------------
        const UINT64 bytes = UINT64(m_cbStride) * UINT64(maxObjects);
        auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes);

        if (FAILED(dev->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, // UPL �Ȃ̂� GENERIC_READ �ŏ풓
            nullptr,                           // CB �p�Ȃ̂� ClearValue �͕s�v
            IID_PPV_ARGS(&m_items[i].resource))))
        {
            return false;
        }

        // --------------------------------------------------------------------
        // 3) �i�� Map
        //    �EUPL �́u�i���}�b�v�v���� OK�iUnmap �R�X�g/�񐔂����炷�j�B
        //    �ECD3DX12_RANGE(0,0) �� CPU ����̏������݂݂̂ŁAGPU ���ւ̓ǂݎ��͈͒ʒm�͕s�v�̈Ӗ��B
        //    �E�߂�|�C���^(m_items[i].cpu)�� "�t���[�� i �� UPL �̐擪" ���w���B
        // --------------------------------------------------------------------
        CD3DX12_RANGE rr(0, 0); // �ǂݎ�薳���i�������݂����j
        if (FAILED(m_items[i].resource->Map(0, &rr, reinterpret_cast<void**>(&m_items[i].cpu))))
        {
            return false;
        }

        // Fence �l�������i�t���[�������҂��̊Ǘ��͏�ʂōs���j
        m_items[i].fenceValue = 0;
    }

    return true;
}

void FrameResources::Destroy()
{
    // �����ς݃t���[�������t���ɕЕt���i�����͕K�{�ł͂Ȃ���������₷�����߁j
    for (auto& it : m_items) {
        // Map ���Ă���ꍇ�� Unmap ���Ă������i�i���}�b�v�����j
        if (it.resource && it.cpu) {
            it.resource->Unmap(0, nullptr);
            it.cpu = nullptr; // ���O�Fdangling pointer�h�~
        }

        // UPL ���\�[�X�ƃA���P�[�^�����
        it.resource.Reset();
        it.cmdAlloc.Reset();

        // Fence �l���O�̂��ߏ�����
        it.fenceValue = 0;
    }

    // �x�N�^���k�����A�S�̂̃��^�������Z�b�g
    m_items.clear();
    m_count = 0;
    m_cbStride = 0;
}
