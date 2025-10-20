#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <vector>

/*
    FrameResources
    ----------------------------------------------------------------------------
    �����F
      - �t���[���C���t���C�g�����i��F2�`3�j�� �g�t���[���ʃ��\�[�X�h ���܂Ƃ߂ĊǗ��B
        * �R�}���h�A���P�[�^�iID3D12CommandAllocator�j
        * �A�b�v���[�h�p�̒萔�o�b�t�@�i1 ���\�[�X�� N �I�u�W�F�N�g�Ԃ���l�߂�j
        * CPU �}�b�v��|�C���^�icb �������ݗp�j
        * �t���[�����������ʂ���t�F���X�l

    �g�����i�T�^�j�F
      1) Initialize(dev, frameCount, sizeof(SceneCB), maxObjects)
      2) �t���[���擪�� current = Get(frameIndex)
         - current.cmdAlloc->Reset()
         - cmdList->Reset(current.cmdAlloc.Get(), �c)
      3) �萔�o�b�t�@�� current.cpu + (objectIndex * GetCBStride()) �ɏ�������
         - GPU ���� current.resource->GetGPUVirtualAddress() + (objectIndex * GetCBStride())
      4) Submit ��A�擾�����t�F���X�l�� current.fenceValue �ɋL�^
      5) ���񓯂� frameIndex ���g���O�� fence �̊�����҂�

    ���ӓ_�F
      - cbStride �� 256 �o�C�g���E�ɑ�����iD3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT�j
      - Upload ���\�[�X�͏펞�}�b�v�iMap once �� Unmap never�j�̉^�p��z��
*/

struct FrameItem {
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAlloc;  // ���̃t���[����p�̃R�}���h�A���P�[�^
    Microsoft::WRL::ComPtr<ID3D12Resource>         resource;  // Upload CB�i�A���̈�� maxObjects �Ԃ�j
    UINT8* cpu = nullptr; // �펞�}�b�v��inullptr �̂Ƃ����}�b�v�j
    UINT64                                         fenceValue = 0; // ���̃t���[���̊����������t�F���X�l
};

class FrameResources {
public:
    FrameResources() = default;

    /*
        Initialize
        ------------------------------------------------------------------------
        @param dev         : D3D12 �f�o�C�X
        @param frameCount  : �t���[���C���t���C�g���i�o�b�N�o�b�t�@���Ɉ�v������̂���ʓI�j
        @param cbSize      : 1 �I�u�W�F�N�g�Ԃ�̒萔�o�b�t�@ struct �T�C�Y�i��Fsizeof(SceneCB)�j
        @param maxObjects  : 1 �t���[�����őz�肷��ő�I�u�W�F�N�g���i�X���b�g���j
        �߂�l�F������ true
        ���\  �F�e�t���[���ɂ� CommandAllocator �� �gmaxObjects �~ cbStride�h �T�C�Y��
                Upload �o�b�t�@���m�ۂ��AMap ���� cpu �ɕێ�����B
    */
    bool Initialize(ID3D12Device* dev, UINT frameCount, UINT cbSize, UINT maxObjects);

    /*
        Destroy
        ------------------------------------------------------------------------
        - �ێ����\�[�X��j���BMap �𖾎��I�� Unmap ����K�v�͂Ȃ��i���S���� Unmap ���Ă� OK�j�B
        - �A���P�[�^�� Upload �o�b�t�@�ACPU �|�C���^���N���A�B
    */
    void Destroy();

    // ------------------------- �A�N�Z�T -------------------------
    UINT GetCount() const { return m_count; }

    // 1 �I�u�W�F�N�g������� CB �X�g���C�h�i256 �o�C�g���E�ɑ������l�j
    UINT GetCBStride() const { return m_cbStride; }

    // ���݃t���[���̃n���h���擾�i�������݁^�ǂݏo���j
    FrameItem& Get(UINT idx) { return m_items[idx]; }
    const FrameItem& Get(UINT idx) const { return m_items[idx]; }

private:
    std::vector<FrameItem> m_items; // frameCount �v�f�Ԃ�
    UINT m_count = 0;             // = frameCount
    UINT m_cbStride = 0;            // = Align(cbSize, 256)
};
