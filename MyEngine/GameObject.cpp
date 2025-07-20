#include "GameObject.h" // GameObject�N���X�̒�`���܂ރw�b�_�[�t�@�C��
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
    // ����GameObject�ɃA�^�b�`����Ă��邷�ׂẴR���|�[�l���g���X�V����
    for (const auto& comp : m_Components)
    {
        comp->Update(deltaTime);
    }

    // ����GameObject�̎q�I�u�W�F�N�g���ċA�I�ɍX�V����
    for (const auto& child : m_Children)
    {
        child->Update(deltaTime);
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
}

// --- Scene�N���X�̎��� ---

// Scene�̃R���X�g���N�^
// @param name: �V�[���ɗ^�����閼�O
Scene::Scene(const std::string& name)
    : m_Name(name) // �V�[���̖��O��������
{
}

// �V�[���Ƀ��[�gGameObject��ǉ�����
// @param gameObject: �ǉ�����GameObject��shared_ptr
void Scene::AddGameObject(std::shared_ptr<GameObject> gameObject)
{
    // �V�[���̃��[�gGameObject���X�g��GameObject��ǉ�����
    // ����: GameObject�����ɕʂ�GameObject�̎q�Ƃ��Ēǉ�����Ă���ꍇ�A
    // AddChild���W�b�N������GameObject���ȑO�̐e����؂藣�����߁A
    // �����Ń��[�g�Ƃ��Ēǉ�����͓̂K�؂łȂ��ꍇ������B
    // �ʏ�A���[�g�I�u�W�F�N�g�͐e�������Ȃ��B
    m_RootGameObjects.push_back(gameObject);
}

// �V�[�����烋�[�gGameObject���폜����
// @param gameObject: �폜����GameObject��shared_ptr
void Scene::RemoveGameObject(std::shared_ptr<GameObject> gameObject)
{
    // m_RootGameObjects���X�g����w�肳�ꂽGameObject���폜
    m_RootGameObjects.erase(std::remove(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject), m_RootGameObjects.end());
}

// �V�[�����̂��ׂẴ��[�gGameObject���X�V����
// @param deltaTime: �O�̃t���[������̌o�ߎ���
void Scene::Update(float deltaTime)
{
    // �V�[�����̊e���[�gGameObject���X�V����
    // �eGameObject��Update���\�b�h�����g�̎q�I�u�W�F�N�g���ċA�I�ɍX�V���邽�߁A
    // �����ł̓��[�g�I�u�W�F�N�g�݂̂��C�e���[�g����΂悢
    for (const auto& go : m_RootGameObjects)
    {
        go->Update(deltaTime);
    }
}