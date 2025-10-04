#include "MeshUploader.h"
#include "Debug/DxDebug.h"
#include <cstring>
#include <d3dx12.h>

using Microsoft::WRL::ComPtr;

bool CreateMesh(ID3D12Device* dev, const MeshData& src, MeshGPU& out) {
    if (src.Indices.empty() || src.Vertices.empty()) return false;

    HRESULT hr;
    D3D12_HEAP_PROPERTIES heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // VB
    const UINT vbSize = (UINT)src.Vertices.size() * sizeof(Vertex);
    D3D12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);
    hr = dev->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &vbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&out.vb));
    dxdbg::LogHRESULTError(hr, "Create VB");
    if (FAILED(hr)) return false;

    {
        UINT8* dst = nullptr; CD3DX12_RANGE rr(0, 0);
        hr = out.vb->Map(0, &rr, reinterpret_cast<void**>(&dst));
        dxdbg::LogHRESULTError(hr, "VB Map");
        if (FAILED(hr)) return false;
        std::memcpy(dst, src.Vertices.data(), vbSize);
        out.vb->Unmap(0, nullptr);
    }
    out.vbv.BufferLocation = out.vb->GetGPUVirtualAddress();
    out.vbv.StrideInBytes = sizeof(Vertex);
    out.vbv.SizeInBytes = vbSize;

    // IB
    const UINT ibSize = (UINT)src.Indices.size() * sizeof(unsigned int);
    D3D12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);
    hr = dev->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &ibDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&out.ib));
    dxdbg::LogHRESULTError(hr, "Create IB");
    if (FAILED(hr)) return false;

    {
        UINT8* dst = nullptr; CD3DX12_RANGE rr(0, 0);
        hr = out.ib->Map(0, &rr, reinterpret_cast<void**>(&dst));
        dxdbg::LogHRESULTError(hr, "IB Map");
        if (FAILED(hr)) return false;
        std::memcpy(dst, src.Indices.data(), ibSize);
        out.ib->Unmap(0, nullptr);
    }
    out.ibv.BufferLocation = out.ib->GetGPUVirtualAddress();
    out.ibv.Format = DXGI_FORMAT_R32_UINT;
    out.ibv.SizeInBytes = ibSize;

    out.indexCount = (UINT)src.Indices.size();
    return true;
}
