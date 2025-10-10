#include "Renderer/SceneRenderer.h"
#include <algorithm>
#include <cmath>
#include <functional>

using Microsoft::WRL::ComPtr;

void SceneRenderer::Record(ID3D12GraphicsCommandList* cmd,
    RenderTarget& rt,
    const CameraMatrices& cam,
    const Scene* scene,
    UINT cbBase,
    UINT frameIndex,
    UINT maxObjects)
{
    if (!rt.Color() || !cmd || !m_frames) return;

    // RT ƒZƒbƒg
    rt.TransitionToRT(cmd);
    rt.Bind(cmd);
    rt.Clear(cmd);

    // VP/SC/RS
    D3D12_VIEWPORT vp{ 0.f, 0.f, (float)rt.Width(), (float)rt.Height(), 0.f, 1.f };
    D3D12_RECT     sc{ 0, 0, (LONG)rt.Width(), (LONG)rt.Height() };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->SetGraphicsRootSignature(m_pipe.root.Get());

    if (scene) {
        auto& fr = m_frames->Get(frameIndex);
        UINT8* cbCPU = fr.cpu;
        D3D12_GPU_VIRTUAL_ADDRESS cbGPU = fr.resource->GetGPUVirtualAddress();
        const UINT cbStride = m_frames->GetCBStride();
        UINT slot = 0;

        using namespace DirectX;

        std::function<void(std::shared_ptr<GameObject>)> draw =
            [&](std::shared_ptr<GameObject> go)
            {
                if (!go || slot >= maxObjects) return;

                if (auto mr = go->GetComponent<MeshRendererComponent>())
                {
                    if (mr->VertexBuffer && mr->IndexBuffer && mr->IndexCount > 0)
                    {
                        XMMATRIX world = go->Transform->GetWorldMatrix();
                        XMMATRIX mvp = world * cam.view * cam.proj;

                        XMVECTOR det;
                        XMMATRIX inv = XMMatrixInverse(&det, world);
                        float detScalar = XMVectorGetX(det);
                        if (!std::isfinite(detScalar) || std::fabs(detScalar) < 1e-8f) inv = XMMatrixIdentity();
                        XMMATRIX worldIT = XMMatrixTranspose(inv);

                        SceneConstantBuffer cb{};
                        XMStoreFloat4x4(&cb.mvp, mvp);
                        XMStoreFloat4x4(&cb.world, world);
                        XMStoreFloat4x4(&cb.worldIT, worldIT);
                        XMStoreFloat3(&cb.lightDir, XMVector3Normalize(XMVectorSet(0.0f, -1.0f, -1.0f, 0.0f)));
                        cb.pad = 0.0f;

                        const UINT dst = cbBase + slot;
                        std::memcpy(cbCPU + (UINT64)dst * cbStride, &cb, sizeof(cb));
                        cmd->SetGraphicsRootConstantBufferView(0, cbGPU + (UINT64)dst * cbStride);

                        cmd->IASetVertexBuffers(0, 1, &mr->VertexBufferView);
                        cmd->IASetIndexBuffer(&mr->IndexBufferView);
                        cmd->DrawIndexedInstanced(mr->IndexCount, 1, 0, 0, 0);
                        ++slot;
                    }
                }
                for (auto& ch : go->GetChildren()) draw(ch);
            };

        for (auto& root : scene->GetRootGameObjects()) draw(root);
    }

    rt.TransitionToSRV(cmd);
}