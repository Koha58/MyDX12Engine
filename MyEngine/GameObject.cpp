#include "GameObject.h"
#include <algorithm> // for std::remove

GameObject::GameObject(const std::string& name)
    : Name(name)
{
    // ���ׂĂ�GameObject��TransformComponent������
    // TransformComponent �� AddComponent �o�R�Œǉ�����
    Transform = AddComponent<TransformComponent>();
}

GameObject::~GameObject()
{
    // �q�I�u�W�F�N�g�̐e�|�C���^���N���A
    for (const auto& child : m_Children)
    {
        child->m_Parent.reset();
    }
}

void GameObject::Update(float deltaTime)
{
    // �e�R���|�[�l���g�̍X�V
    for (const auto& comp : m_Components)
    {
        comp->Update(deltaTime);
    }

    // �q�I�u�W�F�N�g�̍X�V
    for (const auto& child : m_Children)
    {
        child->Update(deltaTime);
    }
}

void GameObject::AddChild(std::shared_ptr<GameObject> child)
{
    if (child->m_Parent.lock())
    {
        // ���ɐe������ꍇ�́A���̐e����؂藣��
        child->m_Parent.lock()->RemoveChild(child);
    }
    m_Children.push_back(child);
    child->m_Parent = shared_from_this(); // �e��ݒ�
}

void GameObject::RemoveChild(std::shared_ptr<GameObject> child)
{
    m_Children.erase(std::remove(m_Children.begin(), m_Children.end(), child), m_Children.end());
    child->m_Parent.reset(); // �e���N���A
}

Scene::Scene(const std::string& name)
    : m_Name(name)
{
}

void Scene::AddGameObject(std::shared_ptr<GameObject> gameObject)
{
    // ���[�gGameObject�Ƃ��Ēǉ�
    // GameObject�����Ɏq�Ƃ��đ���GameObject�ɒǉ�����Ă���ꍇ�́A�����ł̒ǉ��͓K�؂łȂ��ꍇ������
    // (��: ���̐e�Ɋ��ɒǉ�����Ă���ꍇ�́A���̐e����؂藣����郍�W�b�N��AddChild�ɋL�q����)
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