#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include "Core/RenderTarget.h"              // �I�t�X�N���[��RT�Ǘ�
#include "Core/FrameResources.h"            // �t���[�������O�iUpload CB ���j
#include "Pipeline/PipelineStateBuilder.h"  // PipelineSet ��`
#include "Scene/Scene.h"
#include "Scene/GameObject.h"
#include "Graphics/SceneConstantBuffer.h"
#include "Components/MeshRendererComponent.h"

// ===== ���ʕ`��p�X(1�J������1RT) =====
struct CameraMatrices
{
    DirectX::XMMATRIX view;
    DirectX::XMMATRIX proj;
};

class SceneRenderer
{
public:

    // FrameResources�Ǝg�p����PipelineSet���󂯎���ĕێ�
    void Initialize(ID3D12Device* /*dev*/, const PipelineSet& pipe, FrameResources* frames)
    {
        m_pipe = pipe;
        m_frames = frames;
    }

    // 1�J������1RT�̕`��
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