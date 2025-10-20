#include "Scene/Scene.h"
#include "Scene/GameObject.h"
#include <algorithm> // std::find, std::remove

// ============================================================================
// Scene.cpp
// ----------------------------------------------------------------------------
// 役割：シーン（= ルート GameObject 群の所有者）
//   - ルート GameObject の登録/除外（親なし = ルート）
//   - 毎フレームの更新(Update)・描画(Render)のエントリーポイント
//   - 破棄要求のキューイングとフレーム終端での“安全な”実破棄
//
// 設計ポリシー：
//   * GameObject は shared_ptr 管理、親参照は weak_ptr（循環参照を避ける）
//   * AddGameObject は「所属シーンの同期」と「階層への接続」に専念
//     （Awake/OnEnable は AddComponent 側で実行する“方針B”）
//   * Destroy は “予約 → フレーム終端で実行” で、巡回中のコンテナ破壊による不整合を回避
//   * Active/ActiveInHierarchy の実効管理は GameObject 側に一元化
// ============================================================================

// ----------------------------------------------------------------------------
// コンストラクタ：シーン名を受け取って保存（識別/デバッグ用）
// ----------------------------------------------------------------------------
Scene::Scene(const std::string& name)
    : m_Name(name)
{
    // ここでは名前のみ設定。ルート配列は空、破棄キューも空。
}

// ----------------------------------------------------------------------------
// AddGameObject
//  - GameObject をこのシーンに所属させ、親が無ければルートに追加。
//  - parent があれば親の子に接続（GameObject 側で親子関係/イベント整合を担保）。
//  - “方針B”により、Awake/OnEnable はここでは呼ばない（AddComponent が担当）。
// ----------------------------------------------------------------------------
void Scene::AddGameObject(std::shared_ptr<GameObject> gameObject,
    std::shared_ptr<GameObject> parent)
{
    if (!gameObject) return;

    // 所属シーンを同期（GameObject::GetScene() 用の weak_ptr を張る）
    gameObject->m_Scene = shared_from_this();

    // 親あり：親の子として追加（内部で ActiveInHierarchy 差分適用などを行う）
    if (parent)
    {
        parent->AddChild(gameObject);
    }
    // 親なし：ルート配列へ（重複登録は防ぐ）
    else
    {
        if (std::find(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject)
            == m_RootGameObjects.end())
        {
            m_RootGameObjects.push_back(gameObject);
        }
    }

    // コンポーネントの Owner を最終同期（Awake は呼ばない）
    for (auto& comp : gameObject->m_Components) {
        if (comp) comp->SetOwner(gameObject);
    }
}

// ----------------------------------------------------------------------------
// RemoveGameObject
//  - シーン管理から GameObject を外す（親付きなら親から外す）
//  - ルートにいた場合はルート配列から除外し、Scene 参照も切る
//  - 実体破棄は Destroy 系で行う（この関数は“管理から外す”責務）
// ----------------------------------------------------------------------------
void Scene::RemoveGameObject(std::shared_ptr<GameObject> gameObject)
{
    if (!gameObject) return;

    auto parent = gameObject->m_Parent.lock();

    if (parent)
    {
        // 親がある場合：親から外す（GameObject 側で“ルート戻し”ポリシーを持つ実装もあり得る）
        parent->RemoveChild(gameObject);
    }
    else
    {
        // 親なし（= ルート）：ルート配列から除外
        m_RootGameObjects.erase(
            std::remove(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject),
            m_RootGameObjects.end()
        );
        // シーン参照を切って孤立させる（所有は shared_ptr に任せる）
        gameObject->m_Scene.reset();
    }
}

// ----------------------------------------------------------------------------
// DestroyGameObject
//  - 即時破棄せず、破棄予約としてキューに積む
//  - Update 中にコンテナを書き換えないことで不整合を回避
// ----------------------------------------------------------------------------
void Scene::DestroyGameObject(std::shared_ptr<GameObject> gameObject)
{
    if (!gameObject) return;

    // まず予約フラグを立てる（内部処理で分岐に使える）
    gameObject->MarkAsDestroyed();

    // 既に登録済みでなければ破棄キューへ
    if (std::find(m_DestroyQueue.begin(), m_DestroyQueue.end(), gameObject) == m_DestroyQueue.end())
        m_DestroyQueue.push_back(gameObject);
}

// ----------------------------------------------------------------------------
// DestroyAllGameObjects
//  - シーン配下の全 GameObject を破棄（アプリ終了/シーンアンロード用）
//  - OnDestroy を発火させたいので、明示的に Destroy() を呼ぶ
// ----------------------------------------------------------------------------
void Scene::DestroyAllGameObjects()
{
    for (const auto& obj : m_RootGameObjects) {
        if (obj) {
            obj->Destroy(); // 子は GameObject 側で再帰破棄
        }
    }
    m_RootGameObjects.clear(); // 到達を断つ（実リソース解放は shared_ptr に任せる）
}

// ----------------------------------------------------------------------------
// ExecuteDestroy（内部ユーティリティ）
//  - 破棄の実体処理（シーン管理からの切断、親子解除、Scene 参照クリア）
//  - 子 → 親の順で片付ける（親の子配列操作を簡単にする）
//  - ※ ここでは GameObject::Destroy() は呼ばない（呼び元で順序を一元化）
// ----------------------------------------------------------------------------
void Scene::ExecuteDestroy(std::shared_ptr<GameObject> gameObject)
{
    if (!gameObject) return;

    // 子を先に片付ける
    for (auto& child : gameObject->GetChildren()) {
        ExecuteDestroy(child);
    }

    // 親から外す（親なし＝ルートならルート配列から除外）
    if (auto parent = gameObject->m_Parent.lock())
    {
        parent->RemoveChild(gameObject);
    }
    else
    {
        m_RootGameObjects.erase(
            std::remove(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject),
            m_RootGameObjects.end()
        );
    }

    // Scene 参照を切る（以降は Scene 非所属）
    gameObject->m_Scene.reset();

    // ここでは Destroy() を呼ばないことに注意。
    // Destroy()（OnDestroy 通知など）はフレーム終端の統一処理で先に呼んでいる。
}

// ----------------------------------------------------------------------------
// SetGameObjectActive
//  - 指定 GameObject を有効/無効化し、Scene 管理との整合も取る
//  - 親子に再帰的に適用（親を無効にすれば子も無効）
// ----------------------------------------------------------------------------
void Scene::SetGameObjectActive(std::shared_ptr<GameObject> gameObject, bool active)
{
    if (!gameObject) return;

    if (active)
    {
        // アクティブ化：Scene 未登録ならルートへ復帰
        if (std::find(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject)
            == m_RootGameObjects.end())
        {
            AddGameObject(gameObject);
        }
    }
    else
    {
        // 非アクティブ化：巡回対象から外す（本エンジンの方針）
        RemoveGameObject(gameObject);
    }

    // GameObject 側のフラグ/イベントを発火（OnEnable/OnDisable は内部で適切に処理）
    gameObject->SetActive(active);

    // 子へも再帰適用（親の無効 → 子も無効の設計を明示）
    for (auto& child : gameObject->GetChildren())
    {
        SetGameObjectActive(child, active);
    }
}

// ----------------------------------------------------------------------------
// SetActive（Scene 自体の有効/無効）
//  - シーン全体の ON/OFF（ルートからツリーへ伝播）
//  - SceneManager 側の“アクティブシーン”切替と併用する想定
// ----------------------------------------------------------------------------
void Scene::SetActive(bool active)
{
    if (m_Active == active) return;
    m_Active = active;

    for (auto& go : m_RootGameObjects) {
        if (go) go->SetActive(active);
    }
}

// ----------------------------------------------------------------------------
// Render
//  - ルート配下を再帰描画（GameObject::Render が子へ伝播）
//  - 実際の描画コマンドは各描画系コンポーネントが Renderer に対して発行
// ----------------------------------------------------------------------------
void Scene::Render(D3D12Renderer* renderer)
{
    for (auto& obj : m_RootGameObjects) {
        if (obj) obj->Render(renderer);
    }
}

// ----------------------------------------------------------------------------
/* Update
   - ルート配下を再帰更新（GameObject::Update が子へ伝播）
   - フレーム終端で Destroy キューを処理：
       1) go->Destroy() で OnDestroy / 子の Destroy 再帰など“内部破棄フロー”を実行
       2) ExecuteDestroy(go) でシーン管理から切断（親子解除/Scene参照クリア）
   ※ 巡回中に構造を変えないことで、イテレーションの安全性を保つ。
*/
// ----------------------------------------------------------------------------
void Scene::Update(float deltaTime)
{
    // --- 通常更新（有効なルートのみ） ---
    for (const auto& go : m_RootGameObjects) {
        if (go && go->IsActive()) {
            go->Update(deltaTime);
        }
    }

    // --- 破棄の遅延実行（フレーム終端） ---
    if (!m_DestroyQueue.empty()) {
        for (auto& go : m_DestroyQueue) {
            if (!go) continue;

            // 1) 内部破棄フロー（OnDestroy 通知/子の Destroy 再帰）
            go->Destroy();

            // 2) シーン管理からの切断（親子解除/Scene参照クリア）
            ExecuteDestroy(go);
        }
        m_DestroyQueue.clear();
    }
}
