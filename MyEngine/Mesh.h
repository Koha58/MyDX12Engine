#pragma once // 同じヘッダーファイルが複数回インクルードされるのを防ぐプリプロセッサディレクティブ

#include <vector>        // std::vectorを使用
#include <DirectXMath.h> // DirectX::XMFLOAT3, DirectX::XMFLOAT4 などを使用
#include <string>        // (今回は直接使われていないが、他の部分で使う可能性を考慮)
#include <wrl/client.h>  // Microsoft::WRL::ComPtr を使用（COMオブジェクトのスマートポインタ）

// DirectX 12 の型定義のために追加
#include <d3d12.h>       // Direct3D 12 APIのコアヘッダー
#include "d3dx12.h"      // D3D12オブジェクトの作成を簡素化するユーティリティ関数を提供

// Component クラスが定義されている GameObject.h をインクルード
#include "GameObject.h"  // Component基底クラスとGameObjectクラスの定義
#include "Component.h"
#include "TransformComponent.h"

// 前方宣言:
// D3D12Renderer クラスが MeshRendererComponent のプライベートメンバーにアクセスできるように、
// フレンドクラスとして宣言するために必要
class D3D12Renderer;

// --- 頂点構造体 ---
// シェーダーの入力レイアウトと同期させる必要がある
struct Vertex
{
    DirectX::XMFLOAT3 Position; // 頂点の3D空間座標 (x, y, z)
    DirectX::XMFLOAT4 Color;    // 頂点の色 (r, g, b, a)
    // DirectX::XMFLOAT2 TexCoord; // テクスチャマッピングを実装する際に必要となるテクスチャ座標
    // DirectX::XMFLOAT3 Normal;   // ライティングを実装する際に必要となる法線ベクトル
};

// --- メッシュデータ構造体 ---
// CPU側でメッシュの頂点データとインデックスデータを保持するための構造体
struct MeshData
{
    std::vector<Vertex> Vertices;       // 頂点データのリスト
    std::vector<unsigned int> Indices;  // インデックスデータのリスト
    // unsigned short から unsigned int に変更することで、
    // 65535以上の大量の頂点を持つメッシュにも対応可能になる
};

// --- MeshRendererComponentクラス ---
// GameObjectにメッシュを描画させるためのコンポーネント。
// 実際の描画処理はD3D12Rendererが行うが、描画に必要なリソース情報はこのコンポーネントが保持する。
class MeshRendererComponent : public Component
{
    // D3D12Renderer クラスをフレンドとして宣言することで、
    // D3D12Renderer がこのクラスの private/protected メンバーに直接アクセスできるようになる。
    // これは、D3D12Renderer がこのコンポーネントのGPUリソース（VertexBuffer, IndexBuffer）を
    // 直接初期化・管理するために用いられる設計パターン。
    friend class D3D12Renderer;

public:
    // このコンポーネントの静的な型識別子を返す関数。
    // GetComponent<T>() のようなテンプレート関数で型を識別するために使用される。
    static ComponentType StaticType() { return MeshRenderer; }

    // コンストラクタ:
    // 基底クラスである Component のコンストラクタを呼び出し、タイプを MeshRenderer に設定する。
    MeshRendererComponent();

    // デストラクタ:
    // デフォルトのデストラクタを使用。ComPtrが自動的にリソースを解放するため、特別な処理は不要。
    virtual ~MeshRendererComponent() = default;

    // メッシュデータを設定する関数:
    // CPU側のMeshDataをこのコンポーネントに設定する。
    // GPUリソースの作成はここではなく、D3D12Renderer::CreateMeshRendererResourcesで行われる。
    void SetMesh(const MeshData& meshData);

    // --- GPUリソースとビュー ---
    // これらはD3D12RendererによってGPU上に作成され、このコンポーネントがそのハンドルを保持する。
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;      // 頂点データを格納するGPUリソース
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView;                // 頂点バッファをGPUがどのように解釈するかを定義するビュー

    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffer;       // インデックスデータを格納するGPUリソース
    D3D12_INDEX_BUFFER_VIEW IndexBufferView;                  // インデックスバッファをGPUがどのように解釈するかを定義するビュー

    UINT IndexCount; // 描画するインデックスの総数。DrawIndexedInstanced呼び出しに必要。

    // 初期化関数:
    // このコンポーネント独自の初期化ロジックはD3D12Renderer::CreateMeshRendererResourcesで行われるため、
    // ここでは基底クラスのInitializeをオーバーライドして空の実装とする。
    virtual void Initialize() override {}

private:
    // CPU側で保持するメッシュデータ。
    // 必要に応じてGPUにアップロードされる生データ。D3D12Rendererからアクセスされる。
    MeshData m_MeshData;
};