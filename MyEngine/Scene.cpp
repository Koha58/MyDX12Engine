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
    gameObject->m_Scene = shared_from_this(); // GameObject ���������� Scene ��ݒ�

    if (parent) {
        parent->AddChild(gameObject);
    }
    else {
        // �d���o�^�������
        if (std::find(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject) == m_RootGameObjects.end()) {
            m_RootGameObjects.push_back(gameObject);
        }
    }
}

void Scene::RemoveGameObject(std::shared_ptr<GameObject> gameObject)
{
    auto parent = gameObject->m_Parent.lock();

    if (parent) {
        parent->RemoveChild(gameObject); // �����Ń��[�g���i
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

    // �����Ɂu�j���ς݁v�t���O�𗧂Ă�
    gameObject->MarkAsDestroyed();

    // �\��L���[�ɒǉ��i�d���`�F�b�N����j
    if (std::find(m_DestroyQueue.begin(), m_DestroyQueue.end(), gameObject) == m_DestroyQueue.end())
        m_DestroyQueue.push_back(gameObject);
}

void Scene::DestroyAllGameObjects() {
    // m_GameObjects �� m_RootGameObjects �ɕύX
    for (const auto& obj : m_RootGameObjects) {
        if (obj) {
            obj->Destroy();
        }
    }
    m_RootGameObjects.clear(); // ����������킹�ĕύX
}

void Scene::ExecuteDestroy(std::shared_ptr<GameObject> gameObject)
{
    // �q���ċA�I�ɍ폜
    for (auto& child : gameObject->GetChildren())
    {
        ExecuteDestroy(child);
    }

    // �e����؂藣�� or ���[�g���X�g����O��
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

    // Scene�Q�Ƃ����Z�b�g
    gameObject->m_Scene.reset();
}

void Scene::Update(float deltaTime)
{
    for (const auto& go : m_RootGameObjects) {
        go->Update(deltaTime);
    }
}
