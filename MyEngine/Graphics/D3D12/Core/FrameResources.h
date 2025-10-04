#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <vector>

struct FrameItem {
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAlloc;
    Microsoft::WRL::ComPtr<ID3D12Resource>         resource;   // Upload CB
    UINT8* cpu = nullptr;
    UINT64                                         fenceValue = 0;
};

class FrameResources {
public:
    FrameResources() = default;
    bool Initialize(ID3D12Device* dev, UINT frameCount, UINT cbSize, UINT maxObjects);
    void Destroy();

    UINT GetCount() const { return m_count; }
    UINT GetCBStride() const { return m_cbStride; }
    FrameItem& Get(UINT idx) { return m_items[idx]; }
    const FrameItem& Get(UINT idx) const { return m_items[idx]; }

private:
    std::vector<FrameItem> m_items;
    UINT m_count = 0;
    UINT m_cbStride = 0;
};
