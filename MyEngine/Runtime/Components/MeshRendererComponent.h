#pragma once
#include "Components/Component.h"
#include "Assets/Mesh.h"
#include <wrl/client.h>
#include <d3d12.h>

class D3D12Renderer;

class MeshRendererComponent : public Component
{
    // D3D12Renderer が GPU リソースを直接埋めるための許可
    friend class D3D12Renderer;

public:
    static ComponentType StaticType() { return ComponentType::MeshRenderer; }

    MeshRendererComponent();
    ~MeshRendererComponent() override = default;

    // CPU 側メッシュを設定
    void SetMesh(const MeshData& meshData);

    // レンダラに描画依頼（GameObject が Active のときのみ）
    void Render(D3D12Renderer* renderer) override;

    // 読み取り用アクセサ
    const MeshData& GetMeshData() const { return m_MeshData; }
    MeshData& GetMeshData() { return m_MeshData; }

    // ---- GPU リソース（Renderer が生成/更新）----
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;  // VB
    D3D12_VERTEX_BUFFER_VIEW               VertexBufferView{};
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer;   // IB
    D3D12_INDEX_BUFFER_VIEW                IndexBufferView{};
    UINT                                   IndexCount = 0; // 参照用

private:
    // ★ここにだけ1回だけ定義（重複を解消）
    MeshData m_MeshData;
};
