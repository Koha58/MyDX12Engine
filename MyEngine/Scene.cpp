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

    // Component�ɃI�[�i�[��ݒ肵�AAwake()���Ă�
    for (auto& comp : gameObject->m_Components) {
        comp->SetOwner(gameObject);
        comp->Awake();
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
            DestroyGameObject(obj); // DestroyQueue �ɐς�
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

void Scene::SetActive(bool active)
{
    if (m_Active == active) return;
    m_Active = active;

    // ���[�g GameObject ���ׂĂɓ`�d
    for (auto& go : m_RootGameObjects)
    {
        if (go) go->SetActive(active);
    }
}

void Scene::Render(D3D12Renderer* renderer)
{
    for (auto& obj : m_GameObjects)
    {
        if (obj) obj->Render(renderer);
    }
}


void Scene::Update(float deltaTime)
{
    for (const auto& go : m_RootGameObjects) {
        go->Update(deltaTime);
    }

    // Update ��� Destroy �L���[�������iUnity���̒x�����s�j
    if (!m_DestroyQueue.empty()) {
        for (auto& go : m_DestroyQueue) {
            if (go) {
                go->Destroy();        // �R���|�[�l���g�� OnDestroy �ʒm
                ExecuteDestroy(go);   // �V�[���̊Ǘ����X�g���珜��
            }
        }
        m_DestroyQueue.clear();
    }
}
