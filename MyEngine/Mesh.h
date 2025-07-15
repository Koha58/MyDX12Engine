#pragma once
#include <vector>
#include <DirectXMath.h>
#include <string>
#include <wrl/client.h> // ComPtr

// ★ Direct3D 12 の型定義のために追加
#include <d3d12.h>
#include "d3dx12.h" // もしd3dx12.hを使用しているなら

// Component クラスが定義されている GameObject.h をインクルード
#include "GameObject.h" 

// 前方宣言: D3D12Renderer を MeshRendererComponent のフレンドにするために必要
class D3D12Renderer;

// 頂点構造体
struct Vertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT4 Color;
    // DirectX::XMFLOAT2 TexCoord; // テクスチャ対応時に追加
    // DirectX::XMFLOAT3 Normal;   // ライティング対応時に追加
};

// メッシュデータ
struct MeshData
{
    std::vector<Vertex> Vertices;
    std::vector<unsigned int> Indices; // unsigned short から unsigned int に変更（インデックス数の上限を増やすため）
};

// =====================================================================================
// MeshRendererComponent (GameObjectにメッシュを描画させるためのコンポーネント)
// =====================================================================================
class MeshRendererComponent : public Component
{
    // D3D12Renderer が MeshRendererComponent の private メンバーにアクセスできるようにする
    friend class D3D12Renderer;

public:
    // コンポーネントの型識別子
    static ComponentType StaticType() { return MeshRenderer; }

    MeshRendererComponent(); // コンストラクタで Component のコンストラクタを呼び出す
    virtual ~MeshRendererComponent() = default;

    void SetMesh(const MeshData& meshData);

    // GPUリソース（頂点バッファ、インデックスバッファ）へのポインタ
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer;
    D3D12_INDEX_BUFFER_VIEW IndexBufferView;

    UINT IndexCount; // 描画するインデックス数

    // 初期化はD3D12Renderer::CreateMeshRendererResources で行う
    virtual void Initialize() override {}

private:
    // このメンバーは private のままでOK。フレンド宣言で D3D12Renderer からアクセス可能になる
    MeshData m_MeshData; // メッシュデータ自体はCPU側で保持することもできる
};