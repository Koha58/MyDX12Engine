#include "Scene.h"
#include "GameObject.h"
#include <algorithm>

// =====================================================
// Scene 実装
// ・ルート GameObject の管理、更新、描画、破棄予約/実行を担う
// =====================================================

Scene::Scene(const std::string& name)
    : m_Name(name)
{
    // ここでは名前だけ設定。GameObject の追加は AddGameObject 経由で行う。
}

void Scene::AddGameObject(std::shared_ptr<GameObject> gameObject,
    std::shared_ptr<GameObject> parent)
{
    // --- 所属シーンを設定 ---
    // GameObject 側から GetScene() で参照できるように weak_ptr を張る
    gameObject->m_Scene = shared_from_this();

    // --- 階層に登録 ---
    if (parent) {
        // 親が指定されている場合は親の子リストへ（親子関係の確立）
        parent->AddChild(gameObject);
    }
    else {
        // 親が無い＝ルートオブジェクト。重複登録を避けて追加。
        if (std::find(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject) == m_RootGameObjects.end()) {
            m_RootGameObjects.push_back(gameObject);
        }
    }

    // --- コンポーネント初期化の最終同期 ---
    // 注意: AddComponent 側ですでに Owner 設定 + Awake 済みの設計なら
    //       ここでの Awake 呼び出しは「二重になる」可能性がある。
    // ・本実装では「Scene に入るタイミングで Owner を保証し Awake を呼ぶ」という安全寄りの設計。
    // ・もし Awake の二重呼び出しを避けたい場合は、
    //   1) AddComponent 内で Owner 設定と Awake を完結させる（Scene 側は触らない）
    //   2) ここでは Awake を呼ばず、Start タイミングだけ Scene/Update で統一管理
    // など、プロジェクト方針に合わせて一本化すること。
    for (auto& comp : gameObject->m_Components) {
        comp->SetOwner(gameObject); // Owner が未設定でもここで必ず同期される
        comp->Awake();              // 生成直後の初期化（ポリシー次第で削除可）
    }
}

void Scene::RemoveGameObject(std::shared_ptr<GameObject> gameObject)
{
    // 親の有無で処理を分岐
    auto parent = gameObject->m_Parent.lock();

    if (parent) {
        // 親がいる：親から取り外す（GameObject::RemoveChild 内でルート昇格の取扱がある設計）
        parent->RemoveChild(gameObject);
    }
    else {
        // ルートにいる：ルートリストから除外
        m_RootGameObjects.erase(
            std::remove(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject),
            m_RootGameObjects.end()
        );
        // シーン参照を外す（孤立化させる）
        gameObject->m_Scene.reset();
    }
}

void Scene::DestroyGameObject(std::shared_ptr<GameObject> gameObject)
{
    if (!gameObject) return;

    // --- 即時破棄しない理由 ---
    // Update 中にコンテナを変更するとイテレーション中の不整合やクラッシュを招く。
    // そのため「破棄予約（MarkAsDestroyed）」→「フレーム終端で実破棄」の二段階にする。
    gameObject->MarkAsDestroyed();

    // 予約キューに積む（重複積み防止）
    if (std::find(m_DestroyQueue.begin(), m_DestroyQueue.end(), gameObject) == m_DestroyQueue.end())
        m_DestroyQueue.push_back(gameObject);
}

void Scene::DestroyAllGameObjects() {
    // すべてのルートを破棄予約（子は ExecuteDestroy で再帰的に処理される）
    for (const auto& obj : m_RootGameObjects) {
        if (obj) {
            DestroyGameObject(obj);
        }
    }
    // ルート配列は空に（実データの破棄は Update 終了時に行われる）
    m_RootGameObjects.clear();
}

void Scene::ExecuteDestroy(std::shared_ptr<GameObject> gameObject)
{
    // --- 子を先に再帰破棄 ---
    // 子→親の順で外すと、親の子リスト操作が簡潔になる
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

    // ここでは GameObject::Destroy() 自身は呼ばない（呼び出し元 Update のキュー処理側で行う）
    // 理由：OnDestroy 通知やコンポーネント破棄の順序を一元化するため
}

void Scene::SetGameObjectActive(std::shared_ptr<GameObject> gameObject, bool active)
{
    if (!gameObject) return;

    if (active)
    {
        // アクティブ化：Scene 未登録ならルートへ追加
        if (std::find(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject)
            == m_RootGameObjects.end())
        {
            AddGameObject(gameObject);
        }
    }
    else
    {
        // 非アクティブ化：Scene から取り外し
        RemoveGameObject(gameObject);
    }

    // GameObject 側のフラグも同期（OnEnable/OnDisable を発火する設計なら SetActive 経由でもよい）
    gameObject->m_Active = active;

    // 子にも再帰適用（親が無効なら子も無効、という設計を明示化）
    for (auto& child : gameObject->GetChildren())
    {
        SetGameObjectActive(child, active);
    }
}

void Scene::SetActive(bool active)
{
    if (m_Active == active) return;
    m_Active = active;

    // シーン全体の一括有効/無効：ルートからツリーへ再帰的に反映
    for (auto& go : m_RootGameObjects)
    {
        if (go) go->SetActive(active);
    }
}

void Scene::Render(D3D12Renderer* renderer)
{
    // ルート配下を再帰描画（GameObject::Render が子へ伝播）
    for (auto& obj : m_RootGameObjects)
    {
        if (obj) obj->Render(renderer);
    }
}

void Scene::Update(float deltaTime)
{
    // --- 通常更新 ---
    // ルート配下を再帰更新（GameObject::Update が子へ伝播）
    for (const auto& go : m_RootGameObjects) {
        if (go && go->IsActive()) {
            go->Update(deltaTime);
        }
    }

    // --- 破棄の遅延実行（フレーム終端で安全に） ---
    if (!m_DestroyQueue.empty()) {
        for (auto& go : m_DestroyQueue) {
            if (go) {
                // 1) GameObject 内部で OnDestroy 通知 + 子の Destroy()
                go->Destroy();
                // 2) Scene の管理リスト/親子から取り外す（子は ExecuteDestroy 内で再帰的に処理）
                ExecuteDestroy(go);
            }
        }
        m_DestroyQueue.clear();
    }
}
