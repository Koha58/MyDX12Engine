#pragma once
#include <cstdint>

// ---- forward declarations (�w�b�_���y������) ----
struct ID3D12Fence;
struct ID3D12GraphicsCommandList;
class DeviceResources;
class FrameResources;
class GpuGarbageQueue;
struct RenderTargetHandles;

class FrameScheduler {
public:
    struct BeginInfo {
        unsigned frameIndex;
        ID3D12GraphicsCommandList* cmd;
    };

    // fenceEvent �� HANDLE �� void* �Ŏ󂯂�iwindows.h ���w�b�_�Ɋ܂߂Ȃ����߁j
    void Initialize(DeviceResources* dev,
        ID3D12Fence* fence,
        void* fenceEvent,
        FrameResources* frames,
        GpuGarbageQueue* garbage);

    BeginInfo BeginFrame();                              // �t���[���J�n�i�҂���Reset�j
    void EndFrame(RenderTargetHandles toDispose = {});   // Present/Signal/�x���j��

    ID3D12GraphicsCommandList* GetCmd() const { return m_cmd; }

    ~FrameScheduler(); // m_cmd �̉��

private:
    DeviceResources* m_dev = nullptr;
    FrameResources* m_frames = nullptr;
    GpuGarbageQueue* m_garbage = nullptr;

    ID3D12GraphicsCommandList* m_cmd = nullptr; // ���� BeginFrame �Ő���
    ID3D12Fence* m_fence = nullptr;
    void* m_fenceEvent = nullptr; // HANDLE
    std::uint64_t               m_nextFence = 0;       // ���� Signal ����l
};
