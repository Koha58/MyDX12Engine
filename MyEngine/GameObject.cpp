#include "GameObject.h" // GameObject�N���X�̒�`
#include "Scene.h"
#include "Component.h"
#include "TransformComponent.h"
#include <algorithm>    // std::remove ���g�p���邽�߂ɃC���N���[�h

// =====================================================
// GameObject �N���X�̎���
// =====================================================

// -----------------------------------------------------
// �R���X�g���N�^
// @param name : GameObject �ɗ^�����閼�O
// -----------------------------------------------------
GameObject::GameObject(const std::string& name)
    : Name(name) // ���O��������
{
    // ���ׂĂ� GameObject �� TransformComponent ��K�����B
    // �� AddComponent �o�R�Ő������ATransform �����o�ɕێ��B
    //   ����ɂ��A���W/��]/�X�P�[�����������Ȃ� GameObject �͑��݂��Ȃ��݌v�ɂȂ�B
    Transform = AddComponent<TransformComponent>();
}

// -----------------------------------------------------
// �f�X�g���N�^
// -----------------------------------------------------
GameObject::~GameObject()
{
    // �j�����Ɏq�I�u�W�F�N�g�̐e�|�C���^���N���A���Ă����B
    // �� dangling weak_ptr�i�e�����ɉ���ςȂ̂ɎQ�Ƃ�������j��h���B
    for (const auto& child : m_Children)
    {
        child->m_Parent.reset();
    }
}

// -----------------------------------------------------
// Update
// �E���g�Ǝq�I�u�W�F�N�g���X�V����
// �EScene �̍X�V���[�v����Ă΂��
// @param deltaTime : �O�t���[������̌o�ߎ���
// -----------------------------------------------------
void GameObject::Update(float deltaTime)
{
    if (m_Destroyed || !IsActive()) return; // ����/�j���ς݂Ȃ珈�����Ȃ�

    // --- �q�I�u�W�F�N�g�̍X�V ---
    // �ċA�I�ɌĂԂ��ƂŃc���[�\���S�̂��X�V�����
    for (auto& child : m_Children) {
        child->Update(deltaTime);
    }

    // --- �R���|�[�l���g�X�V ---
    for (auto& comp : m_Components)
    {
        if (!comp) continue;

        // Start �͍ŏ��� Update �O�Ɉ�x�����ĂԁiUnity���C�N�j
        if (!comp->HasStarted()) {
            comp->Start();
            comp->MarkStarted();
        }

        // �L����Ԃ̃R���|�[�l���g�̂� Update
        if (comp->IsEnabled()) {
            comp->Update(deltaTime);
        }
    }

    // --- LateUpdate ---
    // Update �̌㏈�����K�v�ȏꍇ�ɌĂ΂��
    for (auto& comp : m_Components)
    {
        if (comp && comp->IsEnabled()) {
            comp->LateUpdate(deltaTime);
        }
    }
}

// -----------------------------------------------------
// �q�I�u�W�F�N�g�̒ǉ�
// @param child : �ǉ����� GameObject
// -----------------------------------------------------
void GameObject::AddChild(std::shared_ptr<GameObject> child)
{
    // ���łɐe������ꍇ�́A���̐e������O��
    if (child->m_Parent.lock())
    {
        child->m_Parent.lock()->RemoveChild(child);
    }

    // �����̎q���X�g�ɒǉ����A�e�|�C���^��ݒ�
    m_Children.push_back(child);
    child->m_Parent = shared_from_this();
}

// -----------------------------------------------------
// �q�I�u�W�F�N�g�̍폜
// @param child : �폜�Ώ�
// -----------------------------------------------------
void GameObject::RemoveChild(std::shared_ptr<GameObject> child)
{
    // remove-erase �C�f�B�I���ō폜
    m_Children.erase(
        std::remove(m_Children.begin(), m_Children.end(), child),
        m_Children.end());

    // �e�|�C���^���N���A
    child->m_Parent.reset();

    // �e����O�ꂽ�I�u�W�F�N�g�� Scene �̃��[�g�ɖ߂�
    if (auto scene = child->m_Scene.lock()) {
        scene->AddGameObject(child, nullptr);
    }
}

// -----------------------------------------------------
// Active ��Ԃ̐ݒ�
// �E���g�Ǝq�� OnEnable/OnDisable ���Ăяo��
// �EScene ���̊Ǘ����X�g�ɔ��f
// -----------------------------------------------------
void GameObject::SetActive(bool active)
{
    if (m_Active == active) return; // �ω��Ȃ��Ȃ牽�����Ȃ�
    m_Active = active;

    // --- �R���|�[�l���g�ɒʒm ---
    for (auto& comp : m_Components)
    {
        if (!comp) continue;
        if (m_Active) comp->OnEnable();
        else comp->OnDisable();
    }

    // --- Scene �Ǘ��ɔ��f ---
    if (auto scene = m_Scene.lock())
    {
        if (m_Active)
        {
            // Scene �̃��[�g���X�g�ɑ��݂��Ȃ��Ȃ�ǉ�
            if (!scene->ContainsRootGameObject(shared_from_this())) {
                scene->AddGameObject(shared_from_this());
            }
        }
        else
        {
            // ���������ꂽ�� Scene �̊Ǘ��Ώۂ���O��
            scene->RemoveGameObject(shared_from_this());
        }
    }

    // --- �q�I�u�W�F�N�g�ɂ��ċA�I�ɓK�p ---
    for (auto& child : m_Children)
    {
        if (child) child->SetActive(active);
    }
}

// -----------------------------------------------------
// Active ��Ԃ̔���
// �E���g�������Ȃ� false
// �E�e�������Ȃ�q������
// -----------------------------------------------------
bool GameObject::IsActive() const
{
    if (!m_Active) return false;

    auto parent = m_Parent.lock();
    if (parent) {
        return parent->IsActive();
    }

    return true;
}

// -----------------------------------------------------
// Destroy
// �E�R���|�[�l���g/�q��j���� OnDestroy ���Ă�
// �E�j���ς݂Ȃ牽�����Ȃ�
// -----------------------------------------------------
void GameObject::Destroy()
{
    if (m_Destroyed) return;
    m_Destroyed = true;

    // --- �R���|�[�l���g�j�� ---
    for (auto& comp : m_Components)
    {
        comp->OnDestroy();
    }
    m_Components.clear();

    // --- �q�I�u�W�F�N�g���ċA�I�ɔj�� ---
    for (auto& child : m_Children)
    {
        child->Destroy();
    }
    m_Children.clear();
}

// -----------------------------------------------------
// Render
// �E���g�̃R���|�[�l���g�`�� �� �q�I�u�W�F�N�g�`��
// -----------------------------------------------------
void GameObject::Render(D3D12Renderer* renderer)
{
    if (!m_Active) return;

    // --- �R���|�[�l���g�̕`�� ---
    for (auto& comp : m_Components)
    {
        if (comp) comp->Render(renderer);
    }

    // --- �q�I�u�W�F�N�g�̕`�� ---
    for (auto& child : m_Children)
    {
        if (child) child->Render(renderer);
    }
}
