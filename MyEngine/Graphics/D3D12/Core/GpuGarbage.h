#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <deque>
#include <memory>
#include <cstdint>

// RenderTarget.h �� include ���Ȃ��i�ˑ��̋t�]�E�r���h���ԒZ�k�̂��ߑO���錾�ɗ��߂�j
struct RenderTargetHandles;

/*
    GpuGarbageQueue
    ----------------------------------------------------------------------------
    �ړI�F
      - �u���t���[���؂藣���� GPU ���\�[�X�v���AGPU �̎g�p����������܂�
        �������Ă����A���S�ɉ�����邽�߂̒x���j���L���[�B
      - �t���[�����E�� Fence �� Signal ���A���̒l�ifenceValue�j�ɂЂ��t����
        �j���Ώۂ��G���L���[�B�ȍ~�ACollect() �� completedValue �ɓ��B����
        ���̂��珇�ɉ������B

    �z��t���[�i��FRenderTarget �̃��T�C�Y���j�F
      1) �Â� RT �� Detach() ���� RenderTargetHandles �ɑ��˂�
      2) ���t���[���� Signal ��ɁAEnqueueRT(signalValue, std::move(handles)) ���Ă�
      3) ���t���[�� Collect(fenceObj) ���ĂсA�t�F���X���B�ς݂̂��̂����
      4) �I������ FlushAll() �ő����S����iGPU ������҂��Ă��炪���S�j

    �݌v�����F
      - RenderTargetHandles �͖������^�̂��߁A���ڃ����o�ɂ͎��ĂȂ��B
        �� std::unique_ptr<RenderTargetHandles> �� type-erasure �I�ɕێ��B
      - �L���[�� FIFO�B�t�F���X�l�͒P���������O��B
      - �X���b�h�Z�[�t�ł͂Ȃ��i�Ăяo���X���b�h�� 1 �{�ɑ�����z��j�B
*/
class GpuGarbageQueue {
public:
    /*
        EnqueueRT
        ------------------------------------------------------------------------
        @param fenceValue : �u���̒l�ɓ��B������j�����ėǂ��v���Ƃ������t�F���X�l
        @param rt         : Detach ���ē��� RenderTargetHandles�i���L����D���j
        ����             : rt ���t�F���X�l�ƂƂ��ɃL���[�����֐ς�
    */
    void EnqueueRT(UINT64 fenceValue, RenderTargetHandles&& rt);

    /*
        Collect
        ------------------------------------------------------------------------
        @param fenceObj : GPU �����l��₢���킹��Ώۃt�F���X
        ����           : fenceObj->GetCompletedValue() �ɒB���� Item ��
                         �擪���珇�ɉ�����Ă���
        ����           : �t�F���X�l�͒P���������O��B�B���Ă��Ȃ� Item �͎c���B
    */
    void Collect(ID3D12Fence* fenceObj);

    /*
        FlushAll
        ------------------------------------------------------------------------
        ���� : �ێ����Ă��邷�ׂĂ� Item �𑦍��ɉ������i�t�F���X�l�Ɋ֌W�Ȃ��j�B
               �A�v���I������u���ł� GPU �͊��S��~�ς݁v�ƕ������Ă���ǖʂŎg�p�B
        ���� : ���s���� GPU ���Q�Ƃ��Ă���\��������󋵂ł͎g��Ȃ����ƁB
    */
    void FlushAll();

    // ���݃L���[�ɗ��܂��Ă���ҋ@���A�C�e�����i�f�o�b�O�p�r�j
    size_t PendingCount() const noexcept { return m_rts.size(); }

private:
    struct Item {
        UINT64 fence = 0;                                   // ���B������j�����ėǂ��t�F���X�l
        std::unique_ptr<RenderTargetHandles> rt;            // �j���҂��� RT �Z�b�g
    };
    std::deque<Item> m_rts;                                 // �擪���ł��Â��i�������t�F���X�l�j
};

/*
    �݊��w���p�i���K�V�[�Ăяo�����̕ێ��p�j
    ------------------------------------------------------------------------
    �����R�[�h�� EnqueueRenderTarget(q, fence, std::move(rt)) ���c���Ă���ꍇ�ɁA
    �V API�iGpuGarbageQueue::EnqueueRT�j���Ăяo���������b�p�B
*/
void EnqueueRenderTarget(GpuGarbageQueue& q, UINT64 fenceValue, RenderTargetHandles&& rt);
