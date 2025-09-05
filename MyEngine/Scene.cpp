#include "Scene.h"
#include "GameObject.h"
#include <algorithm>

Scene::Scene(const std::string& name)
    : m_Name(name)
{
}

void Scene::AddGameObject(std::shared_ptr<GameObject> gameObject,
    std::shared_ptr<GameObject> parent)
{
    gameObject->m_Scene = shared_from_this(); // GameObject が所属する Scene を設定

    if (parent) {
        parent->AddChild(gameObject);
    }
    else {
        // 重複登録を避ける
        if (std::find(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject) == m_RootGameObjects.end()) {
            m_RootGameObjects.push_back(gameObject);
        }
    }

    // Componentにオーナーを設定し、Awake()を呼ぶ
    for (auto& comp : gameObject->m_Components) {
        comp->SetOwner(gameObject);
        comp->Awake();
    }
}

void Scene::RemoveGameObject(std::shared_ptr<GameObject> gameObject)
{
    auto parent = gameObject->m_Parent.lock();

    if (parent) {
        parent->RemoveChild(gameObject); // 自動でルート昇格
    }
    else {
        m_RootGameObjects.erase(
            std::remove(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject),
            m_RootGameObjects.end()
        );
        gameObject->m_Scene.reset();
    }
}

void Scene::DestroyGameObject(std::shared_ptr<GameObject> gameObject)
{
    if (!gameObject) return;

    // 即座に「破棄済み」フラグを立てる
    gameObject->MarkAsDestroyed();

    // 予約キューに追加（重複チェックあり）
    if (std::find(m_DestroyQueue.begin(), m_DestroyQueue.end(), gameObject) == m_DestroyQueue.end())
        m_DestroyQueue.push_back(gameObject);
}

void Scene::DestroyAllGameObjects() {
    // m_GameObjects を m_RootGameObjects に変更
    for (const auto& obj : m_RootGameObjects) {
        if (obj) {
            DestroyGameObject(obj); // DestroyQueue に積む
        }
    }
    m_RootGameObjects.clear(); // こちらも合わせて変更
}

void Scene::ExecuteDestroy(std::shared_ptr<GameObject> gameObject)
{
    // 子も再帰的に削除
    for (auto& child : gameObject->GetChildren())
    {
        ExecuteDestroy(child);
    }

    // 親から切り離す or ルートリストから外す
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

    // Scene参照をリセット
    gameObject->m_Scene.reset();
}

void Scene::SetGameObjectActive(std::shared_ptr<GameObject> gameObject, bool active)
{
    if (!gameObject) return;

    if (active)
    {
        // Scene に未登録なら追加
        if (std::find(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject)
            == m_RootGameObjects.end())
        {
            AddGameObject(gameObject);
        }
    }
    else
    {
        // Scene から削除
        RemoveGameObject(gameObject);
    }

    // GameObject 側のアクティブフラグを更新
    gameObject->m_Active = active;

    // 子オブジェクトも再帰的に同様に設定
    for (auto& child : gameObject->GetChildren())
    {
        SetGameObjectActive(child, active);
    }
}


void Scene::SetActive(bool active)
{
    if (m_Active == active) return;
    m_Active = active;

    // ルート GameObject すべてに伝播
    for (auto& go : m_RootGameObjects)
    {
        if (go) go->SetActive(active);
    }
}

void Scene::Render(D3D12Renderer* renderer)
{
    for (auto& obj : m_RootGameObjects)
    {
        if (obj) obj->Render(renderer);
    }
}


void Scene::Update(float deltaTime)
{
    for (const auto& go : m_RootGameObjects) {
        if (go && go->IsActive()) {
            go->Update(deltaTime);
        }
    }

    // Update 後に Destroy キューを処理（Unity風の遅延実行）
    if (!m_DestroyQueue.empty()) {
        for (auto& go : m_DestroyQueue) {
            if (go) {
                go->Destroy();        // コンポーネントに OnDestroy 通知
                ExecuteDestroy(go);   // シーンの管理リストから除去
            }
        }
        m_DestroyQueue.clear();
    }
}
