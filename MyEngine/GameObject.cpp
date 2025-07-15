#include "GameObject.h"
#include <algorithm> // for std::remove

GameObject::GameObject(const std::string& name)
    : Name(name)
{
    // すべてのGameObjectはTransformComponentを持つ
    // TransformComponent を AddComponent 経由で追加する
    Transform = AddComponent<TransformComponent>();
}

GameObject::~GameObject()
{
    // 子オブジェクトの親ポインタをクリア
    for (const auto& child : m_Children)
    {
        child->m_Parent.reset();
    }
}

void GameObject::Update(float deltaTime)
{
    // 各コンポーネントの更新
    for (const auto& comp : m_Components)
    {
        comp->Update(deltaTime);
    }

    // 子オブジェクトの更新
    for (const auto& child : m_Children)
    {
        child->Update(deltaTime);
    }
}

void GameObject::AddChild(std::shared_ptr<GameObject> child)
{
    if (child->m_Parent.lock())
    {
        // 既に親がいる場合は、その親から切り離す
        child->m_Parent.lock()->RemoveChild(child);
    }
    m_Children.push_back(child);
    child->m_Parent = shared_from_this(); // 親を設定
}

void GameObject::RemoveChild(std::shared_ptr<GameObject> child)
{
    m_Children.erase(std::remove(m_Children.begin(), m_Children.end(), child), m_Children.end());
    child->m_Parent.reset(); // 親をクリア
}

Scene::Scene(const std::string& name)
    : m_Name(name)
{
}

void Scene::AddGameObject(std::shared_ptr<GameObject> gameObject)
{
    // ルートGameObjectとして追加
    // GameObjectが既に子として他のGameObjectに追加されている場合は、ここでの追加は適切でない場合がある
    // (例: 他の親に既に追加されている場合は、その親から切り離されるロジックをAddChildに記述する)
    m_RootGameObjects.push_back(gameObject);
}

void Scene::RemoveGameObject(std::shared_ptr<GameObject> gameObject)
{
    m_RootGameObjects.erase(std::remove(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject), m_RootGameObjects.end());
}

void Scene::Update(float deltaTime)
{
    for (const auto& go : m_RootGameObjects)
    {
        go->Update(deltaTime);
    }
}