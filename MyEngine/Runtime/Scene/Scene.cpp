#include "Scene/Scene.h"
#include "Scene/GameObject.h"
#include <algorithm>

// ============================================================================
// Scene 実装
//  - シーンの責務：
//      * ルート GameObject の登録・除外（親なし = ルート）
//      * 更新（Update）と描画（Render）のエントリーポイント
//      * 破棄要求のキューイングとフレーム終端での安全な実破棄
//  - 設計メモ：
//      * GameObject は shared_ptr で管理。Scene はルート配列を持ち、そこから再帰巡回。
//      * 破棄は「即時」ではなく「予約 → フレーム終端で実破棄」でコンテナ変更の不整合を防止。
//      * ライフサイクル呼び出し方針（B案）：AddComponent 側が Awake/OnEnable を担当。
//        Scene::AddGameObject は **Owner の同期のみ** を行い、Awake は呼ばない。
// ============================================================================

// ----------------------------------------------------------------------------
// コンストラクタ
//  - シーン名を受け取り、識別/デバッグに使用
// ----------------------------------------------------------------------------
Scene::Scene(const std::string& name)
    : m_Name(name)
{
    // ここでは名前だけ設定。GameObject の追加は AddGameObject 経由で行う。
}

// ----------------------------------------------------------------------------
// AddGameObject
//  - GameObject をこのシーンに所属させ、階層（親子）へ登録する。
//  - parent が null の場合はルートに追加。
//  - **B案に基づく重要事項**：ここでは Component の Awake/OnEnable は呼ばず、
//    **Owner 同期のみ** を行う（Awake/OnEnable は AddComponent で実行済み）。
// ----------------------------------------------------------------------------
void Scene::AddGameObject(std::shared_ptr<GameObject> gameObject,
    std::shared_ptr<GameObject> parent)
{
    // --- 所属シーンを設定 ---
    // GameObject 側から GetScene() できるように weak_ptr を張る。
    // ここで「このシーンの管理下に入った」ことを明示する。
    gameObject->m_Scene = shared_from_this();

    // --- 階層に登録 ---
    if (parent) {
        // 親がある：親の子リストへ接続（GameObject 側が親子関係を維持）
        parent->AddChild(gameObject);
    }
    else {
        // 親がない：ルートとして保持（重複追加は避ける）
        if (std::find(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject)
            == m_RootGameObjects.end())
        {
            m_RootGameObjects.push_back(gameObject);
        }
    }

    // --- コンポーネント Owner の最終同期（Awake は呼ばない/B案） ---
    // AddComponent 時に Owner が未設定の可能性に備えて、安全側で同期だけ行う。
    for (auto& comp : gameObject->m_Components) {
        comp->SetOwner(gameObject);
    }
}

// ----------------------------------------------------------------------------
// RemoveGameObject
//  - シーン管理から GameObject を外す。
//  - 親付きなら親から外す。ルートならルート配列から除外し、Scene 参照を切る。
//  - 実データ自体の破棄は Destroy 系で行う（ここでは「管理から外す」ことに専念）。
// ----------------------------------------------------------------------------
void Scene::RemoveGameObject(std::shared_ptr<GameObject> gameObject)
{
    // 親の有無で処理を分岐
    auto parent = gameObject->m_Parent.lock();

    if (parent) {
        // 親がいる：親の子配列から取り外す。
        // （GameObject::RemoveChild 側のポリシーで「ルートに戻す」実装もあり得る点に注意）
        parent->RemoveChild(gameObject);
    }
    else {
        // ルートにいる：ルート配列から除外
        m_RootGameObjects.erase(
            std::remove(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject),
            m_RootGameObjects.end()
        );
        // シーン参照を外す（孤立させる）
        gameObject->m_Scene.reset();
    }
}

// ----------------------------------------------------------------------------
//  — DestroyGameObject
//  - GameObject を「破棄予約」する（即時破棄しない）。
//  - コンテナ巡回（Update 中など）と同時の構造変更を避けるため、実破棄はフレーム終端で行う。
// ----------------------------------------------------------------------------
void Scene::DestroyGameObject(std::shared_ptr<GameObject> gameObject)
{
    if (!gameObject) return;

    // Update 中にコンテナを書き換えると不整合やクラッシュの原因。
    // そこで「予約のみ」行い、フレーム終端でまとめて破棄する。
    gameObject->MarkAsDestroyed();

    // 既にキューにあるなら重複追加はしない
    if (std::find(m_DestroyQueue.begin(), m_DestroyQueue.end(), gameObject) == m_DestroyQueue.end())
        m_DestroyQueue.push_back(gameObject);
}

// ----------------------------------------------------------------------------
// DestroyAllGameObjects
//  - シーン配下の全 GameObject を破棄。
//  - ここでは各オブジェクトの Destroy() を明示的に呼んで OnDestroy を発火させ、
//    ルート配列を空にする（アプリ終了時など「一括破棄」用途を想定）。
// ----------------------------------------------------------------------------
void Scene::DestroyAllGameObjects()
{
    // すべてのルートに対して破棄を呼ぶ（子は GameObject 側で再帰破棄される）
    for (const auto& obj : m_RootGameObjects) {
        if (obj) {
            obj->Destroy(); // OnDestroy を発火（内部で子も Destroy される）
        }
    }
    // ルート配列は空に（以降の到達を断つ）。実ストレージの解放はスマートポインタに任せる。
    m_RootGameObjects.clear();
}

// ----------------------------------------------------------------------------
// ExecuteDestroy
//  - 破棄の実体処理（参照の切断・配列からの除外など）。
//  - 子 → 親の順に片付けることで、親の子配列操作が簡単になる。
//  - ここでは GameObject::Destroy() 自体は呼ばない（呼び元で順序を統一管理するため）。
// ----------------------------------------------------------------------------
void Scene::ExecuteDestroy(std::shared_ptr<GameObject> gameObject)
{
    // --- 子を先に再帰破棄 ---
    for (auto& child : gameObject->GetChildren())
    {
        ExecuteDestroy(child);
    }

    // --- 親から外す or ルートから外す ---
    auto parent = gameObject->m_Parent.lock();
    if (parent)
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

    // --- シーン参照を切る ---
    gameObject->m_Scene.reset();

    // 注意：
    // ここでは GameObject::Destroy() は呼ばない。
    // Destroy()（OnDestroy 通知など）を呼ぶ場所をフレーム終端の統一処理に揃え、
    // 破棄順序/副作用を一元管理するため。
}

// ----------------------------------------------------------------------------
// SetGameObjectActive
//  - 指定 GameObject（とその子）を有効/無効化。
//  - Scene 管理への出し入れ（Add/Remove）と GameObject 側のフラグ/イベントを同期させる。
// ----------------------------------------------------------------------------
void Scene::SetGameObjectActive(std::shared_ptr<GameObject> gameObject, bool active)
{
    if (!gameObject) return;

    if (active)
    {
        // アクティブ化：Scene 未登録ならルートへ追加（復帰）
        if (std::find(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject)
            == m_RootGameObjects.end())
        {
            AddGameObject(gameObject);
        }
    }
    else
    {
        // 非アクティブ化：Scene 管理から取り外す（巡回対象から外す）
        RemoveGameObject(gameObject);
    }

    // GameObject 側のフラグも同期
    //  OnEnable/OnDisable を発火する設計なら SetActive で統一。
    gameObject->SetActive(active);

    // 子へも再帰的に適用（親の無効は子にも伝播する設計を明示）
    for (auto& child : gameObject->GetChildren())
    {
        SetGameObjectActive(child, active);
    }
}

// ----------------------------------------------------------------------------
// SetActive（Scene 自体の有効/無効）
//  - シーン全体の ON/OFF。ルートからツリーへ再帰的に反映。
//  - ※ SceneManager 側で「アクティブシーン」の切替と併用する想定。
// ----------------------------------------------------------------------------
void Scene::SetActive(bool active)
{
    if (m_Active == active) return;
    m_Active = active;

    // ルートから再帰的に反映
    for (auto& go : m_RootGameObjects)
    {
        if (go) go->SetActive(active);
    }
}

// ----------------------------------------------------------------------------
// Render
//  - ルート配下を再帰描画（GameObject::Render が子へ伝播）
//  - 実描画の詳細は各コンポーネント（例: MeshRenderer）が Renderer に対して行う。
// ----------------------------------------------------------------------------
void Scene::Render(D3D12Renderer* renderer)
{
    for (auto& obj : m_RootGameObjects)
    {
        if (obj) obj->Render(renderer);
    }
}

// ----------------------------------------------------------------------------
// Update
//  - ルート配下を再帰更新（GameObject::Update が子へ伝播）
//  - フレーム終端で Destroy キューの実行：Destroy() → ExecuteDestroy() の順で統一。
// ----------------------------------------------------------------------------
void Scene::Update(float deltaTime)
{
    // --- 通常更新 ---
    for (const auto& go : m_RootGameObjects) {
        if (go && go->IsActive()) {
            go->Update(deltaTime);
        }
    }

    // --- 破棄の遅延実行（フレーム終端で安全に） ---
    if (!m_DestroyQueue.empty()) {
        for (auto& go : m_DestroyQueue) {
            if (go) {
                // 1) GameObject 内部の破棄フロー（OnDestroy 通知/子の Destroy 再帰など）
                go->Destroy();
                // 2) Scene の管理から外す（参照切断/親子解除）。子は再帰で処理。
                ExecuteDestroy(go);
            }
        }
        m_DestroyQueue.clear();
    }
}
