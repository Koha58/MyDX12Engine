#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <deque>
#include <memory>
#include <cstdint>

// RenderTarget.h �� include ���Ȃ��i�O���錾�̂݁j
struct RenderTargetHandles;

/** GPU �t�F���X�ɂԂ牺����x���j���L���[ */
class GpuGarbageQueue {
public:
    // RenderTarget ���t�F���X���B�܂ŕێ����ĉ��
    void EnqueueRT(UINT64 fenceValue, RenderTargetHandles&& rt);
    // �t�F���X completedValue �ɒB�������̂������ɉ��
    void Collect(ID3D12Fence* fenceObj);
    // ���S����i�I�����Ȃǁj
    void FlushAll();

    size_t PendingCount() const noexcept { return m_rts.size(); }

private:
    struct Item {
        UINT64 fence = 0;
        // �������^�������o�ɂł��Ȃ����� unique_ptr �ŕێ�
        std::unique_ptr<RenderTargetHandles> rt;
    };
    std::deque<Item> m_rts;
};

/** �݊��w���p�i�����Ăяo�������c�������ꍇ�Ɂj */
void EnqueueRenderTarget(GpuGarbageQueue& q, UINT64 fenceValue, RenderTargetHandles&& rt);
