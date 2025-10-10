#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include "Core/RenderTarget.h"              // オフスクリーンRT管理
#include "Core/FrameResources.h"            // フレームリング（Upload CB 等）
#include "Pipeline/PipelineStateBuilder.h"  // PipelineSet 定義
#include "Scene/Scene.h"
#include "Scene/GameObject.h"
#include "Graphics/SceneConstantBuffer.h"
#include "Components/MeshRendererComponent.h"

// ===== 共通描画パス(1カメラ→1RT) =====
struct CameraMatrices
{
    DirectX::XMMATRIX view;
    DirectX::XMMATRIX proj;
};

class SceneRenderer
{
public:

    // FrameResourcesと使用するPipelineSetを受け取って保持
    void Initialize(ID3D12Device* /*dev*/, const PipelineSet& pipe, FrameResources* frames)
    {
        m_pipe = pipe;
        m_frames = frames;
    }

    // 1カメラ→1RTの描画
    void Record(ID3D12GraphicsCommandList* cmd,
                RenderTarget& rt,
                const CameraMatrices& cam,
                const Scene* scene,
                UINT cbBase,
                UINT frameIndex,
                UINT maxObjects);

private:

    PipelineSet     m_pipe{};
    FrameResources* m_frames = nullptr;

};