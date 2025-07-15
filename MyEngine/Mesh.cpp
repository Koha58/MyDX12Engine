#include "Mesh.h"
#include <stdexcept>

// Component のコンストラクタを呼び出すように変更
MeshRendererComponent::MeshRendererComponent()
    : Component(ComponentType::MeshRenderer), IndexCount(0)
{
}

void MeshRendererComponent::SetMesh(const MeshData& meshData)
{
    m_MeshData = meshData; // メッシュデータをコピー
    IndexCount = (UINT)meshData.Indices.size();

    // 注: ここではD3D12リソースの作成は行いません。
    // リソースはD3D12Rendererクラス内でGPUにアップロードされるべきです。
    // そのため、この SetMesh 関数はCPU側のデータ設定のみを行います。
    // GPUリソースの作成と設定は、D3D12Renderer の新しいヘルパー関数で行います。
}