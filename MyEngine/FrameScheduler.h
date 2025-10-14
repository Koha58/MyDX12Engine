#pragma once
#include <cstdint>

// ---- forward declarations (ヘッダを軽くする) ----
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

    // fenceEvent は HANDLE を void* で受ける（windows.h をヘッダに含めないため）
    void Initialize(DeviceResources* dev,
        ID3D12Fence* fence,
        void* fenceEvent,
        FrameResources* frames,
        GpuGarbageQueue* garbage);

    BeginInfo BeginFrame();                              // フレーム開始（待ち＆Reset）
    void EndFrame(RenderTargetHandles toDispose = {});   // Present/Signal/遅延破棄

    ID3D12GraphicsCommandList* GetCmd() const { return m_cmd; }

    ~FrameScheduler(); // m_cmd の解放

private:
    DeviceResources* m_dev = nullptr;
    FrameResources* m_frames = nullptr;
    GpuGarbageQueue* m_garbage = nullptr;

    ID3D12GraphicsCommandList* m_cmd = nullptr; // 初回 BeginFrame で生成
    ID3D12Fence* m_fence = nullptr;
    void* m_fenceEvent = nullptr; // HANDLE
    std::uint64_t               m_nextFence = 0;       // 次に Signal する値
};
