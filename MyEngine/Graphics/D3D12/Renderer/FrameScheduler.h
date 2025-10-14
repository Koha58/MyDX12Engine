#pragma once
#include <cstdint>

struct ID3D12Fence;
struct ID3D12GraphicsCommandList;
class DeviceResources;
class FrameResources;
class GpuGarbageQueue;
struct RenderTargetHandles; // �O���錾�̂܂܂�OK

class FrameScheduler {
public:
    struct BeginInfo {
        unsigned frameIndex;
        ID3D12GraphicsCommandList* cmd;
    };

    void Initialize(DeviceResources* dev,
        ID3D12Fence* fence,
        void* fenceEvent,
        FrameResources* frames,
        GpuGarbageQueue* garbage);

    BeginInfo BeginFrame();
    // �� ������l�n�����|�C���^�n���ɕύX�i�f�t�H���g�� nullptr�j
    void EndFrame(RenderTargetHandles* toDispose = nullptr);

    ID3D12GraphicsCommandList* GetCmd() const { return m_cmd; }
    ~FrameScheduler();

private:
    DeviceResources* m_dev = nullptr;
    FrameResources* m_frames = nullptr;
    GpuGarbageQueue* m_garbage = nullptr;
    ID3D12GraphicsCommandList* m_cmd = nullptr;
    ID3D12Fence* m_fence = nullptr;
    void* m_fenceEvent = nullptr;
    std::uint64_t               m_nextFence = 0;
};
