#include "GameObject.h" // GameObject�N���X�̒�`���܂ރw�b�_�[�t�@�C��
#include "Scene.h"
#include "Component.h"
#include "TransformComponent.h"
#include <algorithm>    // std::remove�A���S���Y�����g�p���邽�߂ɃC���N���[�h

// --- GameObject�N���X�̎��� ---

// GameObject�̃R���X�g���N�^
// @param name: GameObject�ɗ^�����閼�O
GameObject::GameObject(const std::string& name)
    : Name(name) // GameObject�̖��O��������
{
    // ���ׂĂ�GameObject��TransformComponent�������Ƃ��O��B
    // AddComponent��ʂ���TransformComponent��ǉ����ATransform�|�C���^�ɕێ�����B
    // ����ɂ��ATransformComponent��GameObject�̎����Ǘ����ɓ���B
    Transform = AddComponent<TransformComponent>();
}

// GameObject�̃f�X�g���N�^
GameObject::~GameObject()
{
    // ����GameObject���j�������ہA���̂��ׂĂ̎q�I�u�W�F�N�g�̐e�|�C���^���N���A����B
    // ����ɂ��A�q�I�u�W�F�N�g���u�e�Ȃ��v�̏�ԂɂȂ�A�����Ȑe�|�C���^�ւ̃A�N�Z�X��h���B
    for (const auto& child : m_Children)
    {
        child->m_Parent.reset(); // �q�̐e�|�C���^�����
    }
}

// GameObject�Ƃ��̃R���|�[�l���g�A�q�I�u�W�F�N�g���X�V����
// @param deltaTime: �O�̃t���[������̌o�ߎ���
void GameObject::Update(float deltaTime)
{
    if (m_Destroyed || !m_Active) return;

    // �q�I�u�W�F�N�g���ɍX�V
    for (auto& child : m_Children) {
        child->Update(deltaTime);
    }

    // �R���|�[�l���g�X�V
    for (auto& comp : m_Components)
    {
        if (!comp) continue;

        // Start �͈�x�����Ă�
        if (!comp->HasStarted()) {
            comp->Start();
            comp->MarkStarted();
        }

        // �L���ȏꍇ�̂� Update
        if (comp->IsEnabled()) {
            comp->Update(deltaTime);
        }
    }

    // LateUpdate
    for (auto& comp : m_Components)
    {
        if (comp && comp->IsEnabled()) {
            comp->LateUpdate(deltaTime);
        }
    }
}

// ����GameObject�Ɏq�I�u�W�F�N�g��ǉ�����
// @param child: �ǉ�����GameObject��shared_ptr
void GameObject::AddChild(std::shared_ptr<GameObject> child)
{
    // �ǉ����悤�Ƃ��Ă���q�I�u�W�F�N�g�Ɋ��ɐe�����邩�`�F�b�N
    if (child->m_Parent.lock()) // weak_ptr��shared_ptr�ɕϊ����Đe�I�u�W�F�N�g�ɃA�N�Z�X
    {
        // ���ɐe������ꍇ�́A���̐e����q�I�u�W�F�N�g��؂藣��
        child->m_Parent.lock()->RemoveChild(child);
    }
    m_Children.push_back(child);       // �q�I�u�W�F�N�g�̃��X�g�ɒǉ�
    child->m_Parent = shared_from_this(); // �q�I�u�W�F�N�g�̐e�|�C���^�����̃I�u�W�F�N�g�ɐݒ�
}

// ����GameObject����q�I�u�W�F�N�g���폜����
// @param child: �폜����GameObject��shared_ptr
void GameObject::RemoveChild(std::shared_ptr<GameObject> child)
{
    // m_Children���X�g����w�肳�ꂽ�q�I�u�W�F�N�g���폜
    // std::remove�͎w�肳�ꂽ�l�����X�g�̖����Ɉړ������A�V�����_���I�ȏI�[��Ԃ�
    // erase�͂��̘_���I�ȏI�[���畨���I�ȏI�[�܂ł��폜����
    m_Children.erase(std::remove(m_Children.begin(), m_Children.end(), child), m_Children.end());
    child->m_Parent.reset(); // �q�I�u�W�F�N�g�̐e�|�C���^���N���A����

    // �e����O�ꂽ�� Scene �̃��[�g�ɖ߂�
    if (auto scene = child->m_Scene.lock()) {
        scene->AddGameObject(child, nullptr);
    }
}

void GameObject::SetActive(bool active)
{
    if (m_Active == active) return; // ��Ԃ��ς��Ȃ��Ȃ牽�����Ȃ�
    m_Active = active;

    // �R���|�[�l���g�ɒʒm
    for (auto& comp : m_Components)
    {
        if (!comp) continue;
        if (m_Active) comp->OnEnable();
        else comp->OnDisable();
    }

    // �q�I�u�W�F�N�g�ɂ��ċA�I�ɓ`����
    for (auto& child : m_Children)
    {
        if (child) child->SetActive(active);
    }
}


void GameObject::Destroy()
{
    if (m_Destroyed) return; // ���ɔj���ς݂Ȃ牽�����Ȃ�
    m_Destroyed = true;

    // �R���|�[�l���g�� OnDestroy ��ʒm
    for (auto& comp : m_Components)
    {
        comp->OnDestroy();
    }
    m_Components.clear();

    // �q�I�u�W�F�N�g���ċA�I�ɔj��
    for (auto& child : m_Children)
    {
        child->Destroy();
    }
    m_Children.clear();
}

void GameObject::Render(D3D12Renderer* renderer)
{
    if (!m_Active) return;

    // �R���|�[�l���g�`��
    for (auto& comp : m_Components)
    {
        if (comp) comp->Render(renderer);
    }

    // �q�I�u�W�F�N�g���ċA�I�ɕ`��
    for (auto& child : m_Children)
    {
        if (child) child->Render(renderer);
    }
}


