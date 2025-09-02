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
    gameObject->m_Scene = shared_from_this(); // GameObject ‚ªŠ‘®‚·‚é Scene ‚ðÝ’è

    if (parent) {
        parent->AddChild(gameObject);
    }
    else {
        m_RootGameObjects.push_back(gameObject);
    }
}

void Scene::RemoveGameObject(std::shared_ptr<GameObject> gameObject)
{
    auto parent = gameObject->m_Parent.lock();

    if (parent) {
        parent->RemoveChild(gameObject); // Ž©“®‚Åƒ‹[ƒg¸Ši
    }
    else {
        m_RootGameObjects.erase(
            std::remove(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject),
            m_RootGameObjects.end()
        );
        gameObject->m_Scene.reset();
    }
}

void Scene::Update(float deltaTime)
{
    for (const auto& go : m_RootGameObjects) {
        go->Update(deltaTime);
    }
}
