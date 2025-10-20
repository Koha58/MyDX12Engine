// Renderer/FrameScheduler.cpp
// PCH���g���Ă���Ȃ�
// #include "pch.h"

#include "Renderer/FrameScheduler.h"

#include <windows.h>
#include <d3d12.h>
#include <algorithm>
#include <utility>
#include <cassert>

#include "Core/DeviceResources.h"
#include "Core/FrameResources.h"
#include "Core/GpuGarbage.h"
#include "Core/RenderTarget.h"

/*
    FrameScheduler
    ----------------------------------------------------------------------------
    �ړI�F
      - 1�t���[���́u�J�n�`�I���v�܂ł̋��ʏ������J�v�Z�����B
        * �O�t���[�������҂��iFence�j
        * CommandAllocator/CommandList �� Reset
        * Submit/Present/Signal
        * �t���[�����Ƃ� Fence �l�̕R�t���i�㑱�̑ҋ@�Ɏg���j
        * �x���j���L���[�iGpuGarbageQueue�j�ւ̓����Ɖ��

    �g�����F
      - Initialize() �ŕK�v�ȃ��\�[�X���Z�b�g�B
      - ���t���[�� Render() �����̗���ŁF
         1) auto begin = BeginFrame();  // cmd ���擾���ă��R�[�h�J�n
         2) ... �R�}���h���L�^ ...
         3) EndFrame(&deadRT);          // ��o��Present���x���j���o�^

    ���Ӂi���Ƃ����j�F
      - BackBufferIndex �� Begin �ŕߑ����AEnd �Łu��蒼���Ȃ��v���ƁB
        Present ��ɃC���f�b�N�X���؂�ւ�邽�߁ABegin/End �ŃY�����
        �ʃt���[���� FrameResources ��G���Ă��܂��B
      - Fence �l�́u�O���� Signal ����邱�Ƃ�����v�O��ŒP��������S�ہB
      - GpuGarbageQueue �ւ̓����� Submit �Ɠ����t���[���� Fence �l�ōs���A
        Collect() �͖��t���[���Ă�Ń��[�N��h���B
*/

void FrameScheduler::Initialize(DeviceResources* dev,
    ID3D12Fence* fence,
    void* fenceEvent,
    FrameResources* frames,
    GpuGarbageQueue* garbage)
{
    // �Ăяo�����̏��L���i�����Ǘ��͌Ăяo�����j���u�؂��v
    m_dev = dev;
    m_fence = fence;
    m_fenceEvent = fenceEvent;
    m_frames = frames;
    m_garbage = garbage;

    // ���ɑ�����Signal����Ă�\�����l������ completed+1 ����J�n
    const std::uint64_t completed = (m_fence ? m_fence->GetCompletedValue() : 0);
    m_nextFence = completed + 1;

    // �C���t���C�g�̃t���[���C���f�b�N�X�������iBeginFrame�ōX�V�j
    m_inFlightFrameIndex = 0;
}

FrameScheduler::BeginInfo FrameScheduler::BeginFrame()
{
    // ==============================
    // 1) ���t���[���� BackBufferIndex ��ߑ�
    //    �� EndFrame �ōĎ擾���Ȃ��iPresent �ɂ��؂�ւ�邽�߁j
    // ==============================
    const unsigned fi = m_dev->GetCurrentBackBufferIndex();
    m_inFlightFrameIndex = fi;

    // �t���[����p�̃��\�[�X�Z�b�g�iAllocator/UPL�̈�Ȃǁj
    auto& fr = m_frames->Get(fi);

    // ==============================
    // 2) �O�񂱂̃t���[���C���f�b�N�X�ɓ������d���̊����҂�
    //    �iFence ���������Ȃ�C�x���g�҂��j
    // ==============================
    if (fr.fenceValue != 0 && m_fence->GetCompletedValue() < fr.fenceValue) {
        m_fence->SetEventOnCompletion(fr.fenceValue, static_cast<HANDLE>(m_fenceEvent));
        WaitForSingleObject(static_cast<HANDLE>(m_fenceEvent), INFINITE);
    }

    // ==============================
    // 3) Allocator �� Reset�i���̃t���[���̋L�^���J�n�ł����Ԃցj
    // ==============================
    fr.cmdAlloc->Reset();

    // ==============================
    // 4) ����̂� CommandList �𐶐��i�ȍ~�� Reset �ōė��p�j
    // ==============================
    if (!m_cmd) {
        ID3D12Device* dev = m_dev->GetDevice();
        HRESULT hr = dev->CreateCommandList(
            /*nodeMask=*/0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            fr.cmdAlloc.Get(),
            /*initialPSO=*/nullptr,
            IID_PPV_ARGS(&m_cmd));
        if (SUCCEEDED(hr)) {
            // Reset �O��ɓ��ꂵ�����̂ŁA��x Close ���Ă���
            m_cmd->Close();
        }
    }

    // PSO �͌Ăяo������ Set ����O��i�����ł� nullptr�j
    // ��������Ăяo�������L�^���J�n�ł���
    m_cmd->Reset(fr.cmdAlloc.Get(), nullptr);

    return { fi, m_cmd };
}

void FrameScheduler::EndFrame(RenderTargetHandles* toDispose)
{
    // ==============================
    // 1) BeginFrame �ŕߑ����� fi ���g��
    //    �iPresent �ɂ���ĕς��\�������邽�ߍĎ擾���Ȃ��j
    // ==============================
    const unsigned fi = m_inFlightFrameIndex;
    auto& fr = m_frames->Get(fi);

    // ==============================
    // 2) �R�}���h���o & Present
    // ==============================
    m_cmd->Close();
    ID3D12CommandList* lists[] = { m_cmd };
    m_dev->GetQueue()->ExecuteCommandLists(1, lists);
    m_dev->Present(1); // syncInterval=1�iVSync�L���j�B�K�v�ɉ����ĊO������w�肵�Ă��ǂ��B

    // ==============================
    // 3) Fence �l�̊Ǘ�
    //    - �O��Signal�������Ă��A��Ɂucompleted+1 �ȏ�v������ Signal �Ɏg��
    //      �� Fence �l�̒P��������ۏ�
    // ==============================
    const std::uint64_t completed = m_fence->GetCompletedValue();
    m_nextFence = std::max<std::uint64_t>(m_nextFence, completed + 1);

    const std::uint64_t sig = m_nextFence++;                  // ���t���[���� Signal �l���m��
    m_dev->GetQueue()->Signal(m_fence, sig);                  // GPU �� Signal

    // ���̃t���[���� FrameResource �� fence ��R�Â���i���� Begin �̊����҂��ŎQ�Ɓj
    fr.fenceValue = sig;

    // ==============================
    // 4) �x���j���iRenderTargetHandles�j
    //    - ���̃t���[���̊����i=sig���B�j��ɔj�����������̂�o�^
    //    - �o�^�� toDispose �̓N���A�i���L���̓L���[�ֈړ��ς݁j
    // ==============================
    if (toDispose && (toDispose->color || toDispose->depth || toDispose->rtvHeap || toDispose->dsvHeap)) {
        if (m_garbage) EnqueueRenderTarget(*m_garbage, sig, std::move(*toDispose));
        *toDispose = {}; // ��d�j���h�~�̂��ߋ�ɂ���
    }

    // �j���\�ɂȂ�����������i���t���[���ĂԁF���ߍ��ݖh�~�j
    if (m_garbage) m_garbage->Collect(m_fence);
}

FrameScheduler::~FrameScheduler()
{
    // CommandList �͂����ŉ���iAllocator �� FrameResources �����Ǘ��j
    if (m_cmd) {
        m_cmd->Release();
        m_cmd = nullptr;
    }
}
