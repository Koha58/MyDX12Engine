#include "SceneManager.h"

void SceneManager::AddScene(const std::string& name, std::shared_ptr<Scene> scene)
{
    m_Scenes[name] = scene;
}

void SceneManager::RemoveScene(const std::string& name)
{
    m_Scenes.erase(name);
}

void SceneManager::SwitchScene(const std::string& name)
{
    auto it = m_Scenes.find(name);
    if (it != m_Scenes.end())
    {
        m_CurrentScene = it->second;
    }
}

std::shared_ptr<Scene> SceneManager::GetActiveScene() const
{
    return m_CurrentScene.lock(); // weak_ptr ‚Ìê‡‚Ì‚Ý lock()
}

void SceneManager::Update(float deltaTime)
{
    if (auto activeScene = m_CurrentScene.lock())
    {
        activeScene->Update(deltaTime);
    }
}

