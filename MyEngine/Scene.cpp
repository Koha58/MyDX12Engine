#include "Scene.h"
#include "GameObject.h"
#include <algorithm>

// =====================================================
// Scene ����
// �E���[�g GameObject �̊Ǘ��A�X�V�A�`��A�j���\��/���s��S��
// =====================================================

Scene::Scene(const std::string& name)
    : m_Name(name)
{
    // �����ł͖��O�����ݒ�BGameObject �̒ǉ��� AddGameObject �o�R�ōs���B
}

void Scene::AddGameObject(std::shared_ptr<GameObject> gameObject,
    std::shared_ptr<GameObject> parent)
{
    // --- �����V�[����ݒ� ---
    // GameObject ������ GetScene() �ŎQ�Ƃł���悤�� weak_ptr �𒣂�
    gameObject->m_Scene = shared_from_this();

    // --- �K�w�ɓo�^ ---
    if (parent) {
        // �e���w�肳��Ă���ꍇ�͐e�̎q���X�g�ցi�e�q�֌W�̊m���j
        parent->AddChild(gameObject);
    }
    else {
        // �e�����������[�g�I�u�W�F�N�g�B�d���o�^������Ēǉ��B
        if (std::find(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject) == m_RootGameObjects.end()) {
            m_RootGameObjects.push_back(gameObject);
        }
    }

    // --- �R���|�[�l���g�������̍ŏI���� ---
    // ����: AddComponent ���ł��ł� Owner �ݒ� + Awake �ς݂̐݌v�Ȃ�
    //       �����ł� Awake �Ăяo���́u��d�ɂȂ�v�\��������B
    // �E�{�����ł́uScene �ɓ���^�C�~���O�� Owner ��ۏ؂� Awake ���Ăԁv�Ƃ������S���̐݌v�B
    // �E���� Awake �̓�d�Ăяo������������ꍇ�́A
    //   1) AddComponent ���� Owner �ݒ�� Awake ������������iScene ���͐G��Ȃ��j
    //   2) �����ł� Awake ���Ă΂��AStart �^�C�~���O���� Scene/Update �œ���Ǘ�
    // �ȂǁA�v���W�F�N�g���j�ɍ��킹�Ĉ�{�����邱�ƁB
    for (auto& comp : gameObject->m_Components) {
        comp->SetOwner(gameObject); // Owner �����ݒ�ł������ŕK�����������
        comp->Awake();              // ��������̏������i�|���V�[����ō폜�j
    }
}

void Scene::RemoveGameObject(std::shared_ptr<GameObject> gameObject)
{
    // �e�̗L���ŏ����𕪊�
    auto parent = gameObject->m_Parent.lock();

    if (parent) {
        // �e������F�e������O���iGameObject::RemoveChild ���Ń��[�g���i�̎戵������݌v�j
        parent->RemoveChild(gameObject);
    }
    else {
        // ���[�g�ɂ���F���[�g���X�g���珜�O
        m_RootGameObjects.erase(
            std::remove(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject),
            m_RootGameObjects.end()
        );
        // �V�[���Q�Ƃ��O���i�Ǘ���������j
        gameObject->m_Scene.reset();
    }
}

void Scene::DestroyGameObject(std::shared_ptr<GameObject> gameObject)
{
    if (!gameObject) return;

    // --- �����j�����Ȃ����R ---
    // Update ���ɃR���e�i��ύX����ƃC�e���[�V�������̕s������N���b�V���������B
    // ���̂��߁u�j���\��iMarkAsDestroyed�j�v���u�t���[���I�[�Ŏ��j���v�̓�i�K�ɂ���B
    gameObject->MarkAsDestroyed();

    // �\��L���[�ɐςށi�d���ςݖh�~�j
    if (std::find(m_DestroyQueue.begin(), m_DestroyQueue.end(), gameObject) == m_DestroyQueue.end())
        m_DestroyQueue.push_back(gameObject);
}

void Scene::DestroyAllGameObjects() {
    // ���ׂẴ��[�g��j���\��i�q�� ExecuteDestroy �ōċA�I�ɏ��������j
    for (const auto& obj : m_RootGameObjects) {
        if (obj) {
            DestroyGameObject(obj);
        }
    }
    // ���[�g�z��͋�Ɂi���f�[�^�̔j���� Update �I�����ɍs����j
    m_RootGameObjects.clear();
}

void Scene::ExecuteDestroy(std::shared_ptr<GameObject> gameObject)
{
    // --- �q���ɍċA�j�� ---
    // �q���e�̏��ŊO���ƁA�e�̎q���X�g���삪�Ȍ��ɂȂ�
    for (auto& child : gameObject->GetChildren())
    {
        ExecuteDestroy(child);
    }

    // --- �e����O�� or ���[�g����O�� ---
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

    // --- �V�[���Q�Ƃ�؂� ---
    gameObject->m_Scene.reset();

    // �����ł� GameObject::Destroy() ���g�͌Ă΂Ȃ��i�Ăяo���� Update �̃L���[�������ōs���j
    // ���R�FOnDestroy �ʒm��R���|�[�l���g�j���̏������ꌳ�����邽��
}

void Scene::SetGameObjectActive(std::shared_ptr<GameObject> gameObject, bool active)
{
    if (!gameObject) return;

    if (active)
    {
        // �A�N�e�B�u���FScene ���o�^�Ȃ烋�[�g�֒ǉ�
        if (std::find(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject)
            == m_RootGameObjects.end())
        {
            AddGameObject(gameObject);
        }
    }
    else
    {
        // ��A�N�e�B�u���FScene ������O��
        RemoveGameObject(gameObject);
    }

    // GameObject ���̃t���O�������iOnEnable/OnDisable �𔭉΂���݌v�Ȃ� SetActive �o�R�ł��悢�j
    gameObject->m_Active = active;

    // �q�ɂ��ċA�K�p�i�e�������Ȃ�q�������A�Ƃ����݌v�𖾎����j
    for (auto& child : gameObject->GetChildren())
    {
        SetGameObjectActive(child, active);
    }
}

void Scene::SetActive(bool active)
{
    if (m_Active == active) return;
    m_Active = active;

    // �V�[���S�̂̈ꊇ�L��/�����F���[�g����c���[�֍ċA�I�ɔ��f
    for (auto& go : m_RootGameObjects)
    {
        if (go) go->SetActive(active);
    }
}

void Scene::Render(D3D12Renderer* renderer)
{
    // ���[�g�z�����ċA�`��iGameObject::Render ���q�֓`�d�j
    for (auto& obj : m_RootGameObjects)
    {
        if (obj) obj->Render(renderer);
    }
}

void Scene::Update(float deltaTime)
{
    // --- �ʏ�X�V ---
    // ���[�g�z�����ċA�X�V�iGameObject::Update ���q�֓`�d�j
    for (const auto& go : m_RootGameObjects) {
        if (go && go->IsActive()) {
            go->Update(deltaTime);
        }
    }

    // --- �j���̒x�����s�i�t���[���I�[�ň��S�Ɂj ---
    if (!m_DestroyQueue.empty()) {
        for (auto& go : m_DestroyQueue) {
            if (go) {
                // 1) GameObject ������ OnDestroy �ʒm + �q�� Destroy()
                go->Destroy();
                // 2) Scene �̊Ǘ����X�g/�e�q������O���i�q�� ExecuteDestroy ���ōċA�I�ɏ����j
                ExecuteDestroy(go);
            }
        }
        m_DestroyQueue.clear();
    }
}
