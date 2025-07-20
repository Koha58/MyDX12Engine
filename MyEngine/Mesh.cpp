#include "Mesh.h"      // MeshData構造体とMeshRendererComponentクラスの定義を含むヘッダーファイル
#include <stdexcept>   // エラー処理（ここでは直接使用されていないが、関連するクラスで使われる可能性を考慮）

// MeshRendererComponentのコンストラクタ
// 親クラスのComponentコンストラクタを呼び出し、コンポーネントタイプをMeshRendererに設定する
MeshRendererComponent::MeshRendererComponent()
    : Component(ComponentType::MeshRenderer), // Component基底クラスのコンストラクタを呼び出す
    IndexCount(0)                         // インデックス数を0で初期化
{
    // 追加の初期化が必要な場合はここに記述
}

// メッシュデータを設定する関数
// @param meshData: 設定するメッシュデータ（頂点とインデックス）
void MeshRendererComponent::SetMesh(const MeshData& meshData)
{
    m_MeshData = meshData; // 提供されたメッシュデータを内部のm_MeshDataメンバにコピーする
    IndexCount = (UINT)meshData.Indices.size(); // メッシュのインデックス数を更新する

    // 注意点:
    // この関数はCPU側のメッシュデータを設定するのみで、
    // Direct3D 12の**GPUリソース（頂点バッファやインデックスバッファ）の作成は行わない**。
    // GPUリソースの作成とアップロードは、`D3D12Renderer`クラスのような
    // レンダリングAPIにアクセスできる場所で処理されるべき。
    // これは、コンポーネントがレンダリングAPIの具体的な実装に依存しないようにするための設計。
    // GPUリソースの作成は、`D3D12Renderer::CreateMeshRendererResources`のような
    // 専用のヘルパー関数で行われる。
}