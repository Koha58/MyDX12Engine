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

void FrameScheduler::Initialize(DeviceResources* dev,
    ID3D12Fence* fence,
    void* fenceEvent,
    FrameResources* frames,
    GpuGarbageQueue* garbage)
{
    m_dev = dev;
    m_fence = fence;
    m_fenceEvent = fenceEvent;
    m_frames = frames;
    m_garbage = garbage;

    // ���ɑ�����Signal����Ă�\�����l������ completed+1 ����J�n
    const std::uint64_t completed = (m_fence ? m_fence->GetCompletedValue() : 0);
    m_nextFence = completed + 1;

    // �C���t���C�g�̃t���[���C���f�b�N�X������
    m_inFlightFrameIndex = 0;
}

FrameScheduler::BeginInfo FrameScheduler::BeginFrame()
{
    // �� ���̎��_�̃o�b�N�o�b�t�@�C���f�b�N�X���L�^�iEndFrame�ōĎ擾���Ȃ��j
    const unsigned fi = m_dev->GetCurrentBackBufferIndex();
    m_inFlightFrameIndex = fi;

    auto& fr = m_frames->Get(fi);

    // �O�t���[�������҂�
    if (fr.fenceValue != 0 && m_fence->GetCompletedValue() < fr.fenceValue) {
        m_fence->SetEventOnCompletion(fr.fenceValue, static_cast<HANDLE>(m_fenceEvent));
        WaitForSingleObject(static_cast<HANDLE>(m_fenceEvent), INFINITE);
    }

    // Reset
    fr.cmdAlloc->Reset();

    // ����̂݃R�}���h���X�g����
    if (!m_cmd) {
        ID3D12Device* dev = m_dev->GetDevice();
        HRESULT hr = dev->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, fr.cmdAlloc.Get(),
            nullptr, IID_PPV_ARGS(&m_cmd));
        if (SUCCEEDED(hr)) {
            m_cmd->Close(); // Reset�O��ɑ�����
        }
    }

    // PSO�͌Ăяo������Set����O��i�����ł�nullptr�j
    m_cmd->Reset(fr.cmdAlloc.Get(), nullptr);

    return { fi, m_cmd };
}

void FrameScheduler::EndFrame(RenderTargetHandles* toDispose)
{
    // �� BeginFrame�ŋL�^�����C���f�b�N�X���g���iPresent��Ɏ�蒼���Ȃ��j
    const unsigned fi = m_inFlightFrameIndex;
    auto& fr = m_frames->Get(fi);

    // Submit & Present
    m_cmd->Close();
    ID3D12CommandList* lists[] = { m_cmd };
    m_dev->GetQueue()->ExecuteCommandLists(1, lists);
    m_dev->Present(1);

    // �O��Signal�������Ă��P���������ێ�
    const std::uint64_t completed = m_fence->GetCompletedValue();
    m_nextFence = std::max<std::uint64_t>(m_nextFence, completed + 1);

    const std::uint64_t sig = m_nextFence++;
    m_dev->GetQueue()->Signal(m_fence, sig);

    // �� ���̃t���[����FrameResource�ɕR�Â���
    fr.fenceValue = sig;

    // ���̃t���[��������ɋ�RT��j��
    if (toDispose && (toDispose->color || toDispose->depth || toDispose->rtvHeap || toDispose->dsvHeap)) {
        if (m_garbage) EnqueueRenderTarget(*m_garbage, sig, std::move(*toDispose));
        *toDispose = {}; // ��d�j���h�~
    }
    if (m_garbage) m_garbage->Collect(m_fence);
}

FrameScheduler::~FrameScheduler()
{
    if (m_cmd) { m_cmd->Release(); m_cmd = nullptr; }
}
