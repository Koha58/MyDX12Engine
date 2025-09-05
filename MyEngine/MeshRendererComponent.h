#pragma once
#include "Component.h"
#include "Mesh.h"
#include <wrl/client.h>  // Microsoft::WRL::ComPtr (スマートポインタ for COM)
#include <d3d12.h>       // DirectX 12 API

class D3D12Renderer;

// ===============================================================
// MeshRendererComponent
// ---------------------------------------------------------------
// ・GameObject にアタッチして「メッシュを描画する」役割を担うコンポーネント
// ・内部に保持する MeshData（CPU 側のデータ）を GPU バッファに変換し、描画する
// ・D3D12Renderer が GPU リソース生成を担当するため、friend としてアクセス可能
// ===============================================================
class MeshRendererComponent : public Component
{
    // D3D12Renderer が VertexBuffer/IndexBuffer を直接操作できるようにする
    friend class D3D12Renderer;

public:
    // 型識別用。ComponentType::MeshRenderer を返す
    static ComponentType StaticType() { return ComponentType::MeshRenderer; }

    MeshRendererComponent();
    virtual ~MeshRendererComponent() = default;

    // -----------------------------------------------------------
    // SetMesh
    // ・描画対象となるメッシュデータを設定する
    // ・引数の MeshData は CPU 側で保持され、後で GPU バッファに変換される
    // -----------------------------------------------------------
    void SetMesh(const MeshData& meshData);

    // -----------------------------------------------------------
    // Render
    // ・D3D12Renderer に対して描画を依頼する
    // ・内部的には VertexBuffer / IndexBuffer を用いて DrawCall を発行
    // -----------------------------------------------------------
    void Render(D3D12Renderer* renderer) override;

private:
    // CPU 側のメッシュデータ
    MeshData m_MeshData;

public:
    // ---------------- GPU 側リソース ----------------
    // ・ComPtr: COM オブジェクトの自動解放スマートポインタ
    // ・D3D12Renderer::CreateMeshRendererResources() にて実際に生成される
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;  // 頂点バッファ (GPU 側)
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView{};          // 頂点バッファビュー（描画コマンド用）
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer;   // インデックスバッファ (GPU 側)
    D3D12_INDEX_BUFFER_VIEW IndexBufferView{};            // インデックスバッファビュー
    UINT IndexCount = 0;                                  // インデックス数（三角形描画に必要）
};
