#include "FrameResources.h"
#include "d3dx12.h"

using Microsoft::WRL::ComPtr;

bool FrameResources::Initialize(ID3D12Device* dev, UINT frameCount, UINT cbSize, UINT maxObjects)
{
    m_count = frameCount;
    m_cbStride = (cbSize + 255) & ~255u;
    m_items.resize(frameCount);

    for (UINT i = 0; i < frameCount; ++i) {
        if (FAILED(dev->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_items[i].cmdAlloc))))
            return false;

        const UINT64 bytes = UINT64(m_cbStride) * UINT64(maxObjects);
        auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes);
        if (FAILED(dev->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_items[i].resource))))
            return false;

        CD3DX12_RANGE rr(0, 0);
        if (FAILED(m_items[i].resource->Map(0, &rr, reinterpret_cast<void**>(&m_items[i].cpu))))
            return false;

        m_items[i].fenceValue = 0;
    }
    return true;
}

void FrameResources::Destroy()
{
    for (auto& it : m_items) {
        if (it.resource && it.cpu) { it.resource->Unmap(0, nullptr); it.cpu = nullptr; }
        it.resource.Reset();
        it.cmdAlloc.Reset();
        it.fenceValue = 0;
    }
    m_items.clear();
    m_count = 0;
    m_cbStride = 0;
}
