#include "Core/GpuGarbage.h"
#include "Core/RenderTarget.h" // RenderTargetHandles �̒�`���K�v
#include <utility>

/*
    GpuGarbageQueue
    ----------------------------------------------------------------------------
    �ړI�F
      �E�uGPU ���܂��g���Ă��邩������Ȃ� D3D12 ���\�[�X�v���A���S�ɉ�����邽�߂�
        �x���j���L���[�BFence �l�Ŏ������Ǘ����A�����ς݂ɂȂ��Ă��� Release ����B

    �^�p�F
      1) ���\�[�X�̏��L���� RenderTarget �Ȃǂ���u�؂藣��(Detach)�v�B
      2) ���� RenderTargetHandles �� fenceValue(=���̃t���[���� Signal �l)�ƈꏏ�� Enqueue�B
      3) ���t�� Collect() ���ĂсAGetCompletedValue() ���ǂ������������j���B
      4) �V���b�g�_�E������ FlushAll() �ŋ�������i�K�� GPU �����҂��ς݂ŌĂԂ��Ɓj�B

    �݌v�����F
      - RenderTargetHandles �� ComPtr �����W��\���́Bunique_ptr �ŕ��
        �R���e�i���̃��[�u/�|�b�v���Ɋm���� Release �����悤�ɂ���B
      - �L���[�́uFence �̏����v�����ҁi�擪��������𖞂�������j���j�B
      - Fence �� wrap-around �� 64bit �Ȃ̂Ŏ�����l���i�������ғ��ŕK�v�Ȃ��r���H�v�j�B
      - �}���`�X���b�h�Ŏg���Ȃ�A�Ăяo�����œ������邱�Ɓi���̎����͔�X���b�h�Z�[�t�j�B
*/

// �����F��n���h���̔���i�S ComPtr ����j
static bool IsEmpty(const RenderTargetHandles& h) {
    return !h.color && !h.depth && !h.rtvHeap && !h.dsvHeap;
}

void GpuGarbageQueue::EnqueueRT(UINT64 fenceValue, RenderTargetHandles&& rt)
{
    // ���������Ă��Ȃ��Ȃ牽�����Ȃ��i�]�v�ȃm�[�h��ς܂Ȃ��j
    if (IsEmpty(rt)) return;

    // Item ������ď��L�����ڂ��irt �̒��g�͂����Łg����h�����j
    Item item;
    item.fence = fenceValue;

    // unique_ptr �ŕێ��F�R���e�i���� pop ���ꂽ�u�Ԃ� ComPtr �� Release �����
    item.rt = std::make_unique<RenderTargetHandles>(std::move(rt));

    // ���O��F�Ăяo������ fenceValue �̏����i�ʏ�̓t���[�����j�ŗ���z��
    //        �قȂ鏇���œ�������ꍇ�́Am_rts ����בւ��� or Collect �̃��W�b�N��ύX���邱�ƁB
    m_rts.push_back(std::move(item));
}

void GpuGarbageQueue::Collect(ID3D12Fence* fenceObj)
{
    // Fence ��������Ή����ł��Ȃ�
    if (!fenceObj) return;

    // ���߂܂� GPU ���������� fence �l���擾
    const UINT64 done = fenceObj->GetCompletedValue();

    // �擪����u�����ς݁i<= done�j�v�̂��̂������ɔj��
    // �� Item ���� unique_ptr ���X�R�[�v�A�E�g �� RenderTargetHandles �� ComPtr �� Release
    while (!m_rts.empty() && m_rts.front().fence <= done) {
        m_rts.pop_front();
    }
}

void GpuGarbageQueue::FlushAll()
{
    // �����ӁFFlushAll �́g�������ɔj���h����B
    //   �K���Ăяo������ GPU �����҂��iWaitForGPU�j���ς܂��Ă��邱�ƁB
    //   �����łȂ��ƁA�܂��g�p���̃��\�[�X��������Ă��܂����ꂪ����B
    m_rts.clear(); // Item �� unique_ptr ���S�ĉ�� �� ComPtr �� Release
}

// C-style �̃��[�e�B���e�B�����b�p�i�Ăяo������ include ���ŏ����������ꍇ�ɕ֗��j
void EnqueueRenderTarget(GpuGarbageQueue& q, UINT64 fenceValue, RenderTargetHandles&& rt)
{
    q.EnqueueRT(fenceValue, std::move(rt));
}
