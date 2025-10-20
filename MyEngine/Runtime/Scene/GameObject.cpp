// GameObject.cpp
//------------------------------------------------------------------------------
// �����F�V�[�����̃G���e�B�e�B�i�m�[�h�j��\�� GameObject �̎����B
//       - �e�q�i�c���[�j�\���̊Ǘ��iAddChild / RemoveChild�j
//       - Active�i���ȁj�� ActiveInHierarchy�i�����j�̈���
//       - Component �Q�̃��C�t�T�C�N�� (Awake/OnEnable/Start/Update/LateUpdate/OnDisable/OnDestroy)
//       - �j���t���[�iDestroy�j
// �݌v�����F
//   * GameObject �� shared_ptr �Ǘ��i�e: weak_ptr�j�ŏz�Q�Ƃ����
//   * TransformComponent �͕K�{�BCreate() �t�@�N�g���ň��S�ɕt�^�ictor �ł͕t�^���Ȃ��j
//   * ���C�t�T�C�N���́u���jB�v�FAddComponent �� �� Awake�AActiveInHierarchy �Ȃ瑦 OnEnable�B
//     Start �́u�ŏ��� Update ���O�v�ɒx�����s
//   * ActiveInHierarchy�i= ���Ȃ��L�� && �e�� ActiveInHierarchy�j�ω����̂� OnEnable/OnDisable �𔭉�
//   * RemoveChild �����q�� Scene �̃��[�g�֖߂��i���B�s�\�I�u�W�F�N�g�����Ȃ��j
//------------------------------------------------------------------------------

#include "GameObject.h"              // GameObject �N���X�̒�`
#include "Scene.h"                   // Scene �ւ̓o�^/�Ɖ�
#include "Components/Component.h"    // ���R���|�[�l���g
#include "Components/TransformComponent.h"
#include <algorithm>                 // std::remove

// ============================================================================
// �����w���p�[�i�ǂ݂₷���̂��߂̏�����j
// ============================================================================
namespace {
    // remove-erase �C�f�B�I����ǂ݂₷���܂Ƃ߂�
    template<class T, class U>
    void erase_remove(T& v, const U& value) {
        v.erase(std::remove(v.begin(), v.end(), value), v.end());
    }
}

// ============================================================================
// Create�i�t�@�N�g���j
//  - ��O���S���ꊇ�A���P�[�V�����̂��� make_shared ���g�p
//  - TransformComponent �͕K�{�Ȃ̂ŁA�����Ŋm���ɕt�^����
//  - ctor �ł� shared_from_this() ���g���Ȃ����߁A������ AddComponent ����݌v
// ============================================================================
std::shared_ptr<GameObject> GameObject::Create(const std::string& name)
{
    auto go = std::make_shared<GameObject>(name);           // ���L���L�ɏ悹��
    go->Transform = go->AddComponent<TransformComponent>(); // �K�{ Transform �𑦕t�^
    return go;
}

// ============================================================================
// ctor / dtor
//  - ctor�FTransform �͕t�^���Ȃ��iCreate() ���ň��S�ɕt�^�j
//  - dtor�F�q�̐e�Q�Ɓiweak�j��؂邾���BOnDestroy �� Destroy() �ōs��
// ============================================================================
GameObject::GameObject(const std::string& name)
    : Name(name)
{
    // ��������́u������ԁiActiveInHierarchy�j�v�L���b�V��
    // �i�e�Ȃ������ȗL���������l�Ȃ̂� true �����j
    m_LastActiveInHierarchy = IsActive();
}

GameObject::~GameObject()
{
    // �q�̐e�Q�Ɓiweak_ptr�j���N���A�i���L�� shared_ptr ���j
    for (const auto& child : m_Children) {
        child->m_Parent.reset();
    }
}

// ============================================================================
// Update�i���t���[���Ăяo���j
// �������F
//   1) �j���ς� or ��A�N�e�B�u�K�w�Ȃ瑁�� return
//   2) �q�� Update�i�e���q�BTransform �ˑ������R�j
//   3) ���g�̃R���|�[�l���g Update�iStart �͏���̂� / �L���Ȃ��̂̂݁j
//   4) ���g�̃R���|�[�l���g LateUpdate�i�L���Ȃ��̂̂݁j
// ============================================================================
void GameObject::Update(float deltaTime)
{
    // �����I�ɔ�A�N�e�B�u�Ȃ牽�����Ȃ�
    if (m_Destroyed || !IsActive()) return;

    // --- �q�I�u�W�F�N�g�̍X�V�i�e �� �q�j ---
    for (auto& child : m_Children) {
        child->Update(deltaTime);
    }

    // --- �R���|�[�l���g�̍X�V ---
    for (auto& comp : m_Components) {
        if (!comp) continue;

        // Start �́u�L���v���u�܂� Start �ς݂łȂ��v���A�ŏ��� Update �̒��O�� 1 �񂾂�
        if (!comp->HasStarted() && comp->IsEnabled() && IsActive()) {
            comp->Start();
            comp->MarkStarted();
        }

        // ���t���[���� Update�i�L���������A�N�e�B�u�j
        if (comp->IsEnabled() && IsActive()) {
            comp->Update(deltaTime);
        }
    }

    // --- LateUpdate�i������ Update ��j ---
    for (auto& comp : m_Components) {
        if (comp && comp->IsEnabled()) {
            comp->LateUpdate(deltaTime);
        }
    }
}

// ============================================================================
// AddChild
//  - �q�ɂ���ΏۂɌ��̐e������΁A�܂������炩����O���i�ǎ����h�~�j
//  - �e�q�֌W�̕ύX�� ActiveInHierarchy �ɉe�� �� �����K�p�� OnEnable/OnDisable �𐳂�������
//  - Transform �̐e�q�𓯊��������ꍇ�́A�v���W�F�N�g���j�ɍ��킹�Ă����ōs��
// ============================================================================
void GameObject::AddChild(std::shared_ptr<GameObject> child)
{
    // �ǉ����O�́u�q�̎�����ԁv��ۑ��i��������Ɏg�p�j
    const bool prevChildHier = child->IsActive();

    // �����̐e������O���iRemoveChild �� �g���[�g�߂� + �����K�p�h ���s���݌v�j
    if (auto oldParent = child->m_Parent.lock()) {
        oldParent->RemoveChild(child);
    }

    // �����̎q�Ƃ��ēo�^���A�e�Q�Ƃ𒣂�
    m_Children.push_back(child);
    child->m_Parent = shared_from_this();

    // Transform �̐e�q�֌W�𓯊��������ꍇ�͂����Ŏ��{
    // ��: child->Transform->SetParent(Transform.get());

    // �e���ς�������ʁA�q�� ActiveInHierarchy ���ς��Ȃ獷���K�p
    child->RefreshActiveInHierarchyRecursive(prevChildHier);
}

// ============================================================================
// RemoveChild
//  - �����̎q�z�񂩂珜�O���A�e�Q�Ƃ��N���A
//  - �u�e����O�ꂽ�� Scene �̃��[�g�֖߂��v�|���V�[�œ��B�\�����ێ�
//  - ActiveInHierarchy ���ω������獷���K�p�iOnEnable/OnDisable�j
//  - Reparent�i�t���ւ��j���� AddChild ���ł��������s�����߁A������2��ω�������_�ɒ���
// ============================================================================
void GameObject::RemoveChild(std::shared_ptr<GameObject> child)
{
    const bool prevChildHier = child->IsActive();

    erase_remove(m_Children, child);
    child->m_Parent.reset();

    // ���[�g�֖߂��iScene �Ǘ����Ɏc���j
    if (auto scene = child->m_Scene.lock()) {
        scene->AddGameObject(child, nullptr); // �e nullptr �� ���[�g�o�^
    }

    // ������Ԃ̍����K�p�i�C�x���g���΁j
    child->RefreshActiveInHierarchyRecursive(prevChildHier);
}

// ============================================================================
// SetActive�i���ȃt���O�̐ؑցj
//  �|���V�[�F
//    - �q�� activeSelf �͕ύX���Ȃ��iUnity �Ɠ��l�j�B�e�̖������Ŏ����������ɂȂ�
//    - OnEnable/OnDisable �� ActiveInHierarchy �̕ω����̂ݔ���
//    - Scene �Ǘ��i���[�g�z��j�Ƃ̐������Ƃ�i�{�G���W�����j�j
//      �� Unity �ƈႢ�u��A�N�e�B�u �� ����Ώۂ���O���v���j�ɂ��Ă����
// ============================================================================
void GameObject::SetActive(bool active)
{
    // �ύX�O�̎�����Ԃ��擾�i��������p�j
    const bool prevHier = IsActive();

    // ���łɓ�����ԂȂ� Scene ���Ƃ̐������y������ďI��
    if (m_Active == active) {
        if (auto scene = m_Scene.lock()) {
            if (m_Active) {
                // ���[�g�z��ɑ��݂��Ȃ���Βǉ�
                if (!scene->ContainsRootGameObject(shared_from_this())) {
                    scene->AddGameObject(shared_from_this());
                }
            }
            else {
                // ��A�N�e�B�u�Ȃ珄��Ώۂ���O���i���j����j
                scene->RemoveGameObject(shared_from_this());
            }
        }
        return;
    }

    // ���ȃt���O��ؑ�
    m_Active = active;

    // Scene �Ǘ��֔��f
    if (auto scene = m_Scene.lock()) {
        if (m_Active) {
            if (!scene->ContainsRootGameObject(shared_from_this())) {
                scene->AddGameObject(shared_from_this());
            }
        }
        else {
            scene->RemoveGameObject(shared_from_this());
        }
    }

    // ������Ԃ̍������������q�֓`�d�iOnEnable/OnDisable �𐳂������΁j
    RefreshActiveInHierarchyRecursive(prevHier);
}

// ============================================================================
// IsActive�iActiveInHierarchy �����j
//  - ���Ȃ������Ȃ� false
//  - �e������ΐe�� ActiveInHierarchy �ɏ]��
//  - ���[�g�Ȃ玩�Ȃ� activeSelf �����̂܂܎���
// ============================================================================
bool GameObject::IsActive() const
{
    if (!m_Active) return false;
    if (auto parent = m_Parent.lock()) {
        return parent->IsActive();
    }
    return true; // �e�Ȃ��i���[�g�j
}

// ============================================================================
// Destroy�i�p���j
//  - �L���ȃR���|�[�l���g�� OnDisable ���ɒʒm �� ���̌� OnDestroy
//  - �q���ċA�I�� Destroy
//  - Scene/Parent ����̐؂藣���� Scene ���̔j���t���[�ōs���i�����̈ꌳ�Ǘ��j
// ============================================================================
void GameObject::Destroy()
{
    if (m_Destroyed) return;
    m_Destroyed = true;

    // �L���������R���|�[�l���g�͐�� OnDisable
    if (IsActive()) {
        for (auto& comp : m_Components) {
            if (comp && comp->IsEnabled()) {
                comp->OnDisable();
            }
        }
    }

    // ���̌� OnDestroy
    for (auto& comp : m_Components) {
        if (comp) comp->OnDestroy();
    }
    m_Components.clear();

    // �q�����l�ɔj���i�ċA�j
    for (auto& child : m_Children) {
        child->Destroy();
    }
    m_Children.clear();
}

// ============================================================================
// Render
//  - �����I�ɔ�A�N�e�B�u�Ȃ�`�悵�Ȃ�
//  - �e�`��n�R���|�[�l���g�� Renderer �ɑ΂��ĕ`��v�����s���O��
// ============================================================================
void GameObject::Render(D3D12Renderer* renderer)
{
    if (!IsActive()) return;

    for (auto& comp : m_Components) {
        if (comp) comp->Render(renderer);
    }
    for (auto& child : m_Children) {
        if (child) child->Render(renderer);
    }
}

// ============================ ������������w���p�[ ===========================

// ���݂� ActiveInHierarchy ���ĕ]�����ĕԂ��iIsActive �Ɠ��`�����Ӑ}���m���j
bool GameObject::ComputeActiveInHierarchy() const
{
    return IsActive();
}

// ActiveInHierarchy �̕ω��������ɓK�p�iOnEnable/OnDisable ���΁��L���b�V���X�V�j
void GameObject::ApplyActiveInHierarchyDelta(bool wasActiveInHierarchy, bool nowActiveInHierarchy)
{
    if (wasActiveInHierarchy == nowActiveInHierarchy) {
        return; // �ω��Ȃ� �� �������Ȃ�
    }

    if (nowActiveInHierarchy) {
        // �����I�ɗL���ɂȂ����F�L���ȃR���|�[�l���g�� OnEnable
        for (auto& comp : m_Components) {
            if (comp && comp->IsEnabled()) comp->OnEnable();
        }
    }
    else {
        // �����I�ɖ����ɂȂ����F�L���ȃR���|�[�l���g�� OnDisable
        for (auto& comp : m_Components) {
            if (comp && comp->IsEnabled()) comp->OnDisable();
        }
    }

    // �L���b�V�����ŐV���i�q�̍����K�p���� prev �Ƃ��ēn���j
    m_LastActiveInHierarchy = nowActiveInHierarchy;
}

// �����Ɣz���i�c���[�S�́j�ɂ��� ActiveInHierarchy �̍�����K�p
// @param prevOfThis : �Ăяo�����O�ɂ�����u���̃m�[�h���g�v�̎������
void GameObject::RefreshActiveInHierarchyRecursive(bool prevOfThis)
{
    const bool now = ComputeActiveInHierarchy();

    // �܂������ɓK�p�i������ m_LastActiveInHierarchy ���X�V�����j
    ApplyActiveInHierarchyDelta(prevOfThis, now);

    // �q�ւ́u���ꂼ��̒��O��ԁi�L���b�V���j�v�� prev �Ƃ��ēn���A�ċA�K�p
    for (auto& ch : m_Children) {
        if (!ch) continue;
        const bool childPrev = ch->m_LastActiveInHierarchy;
        ch->RefreshActiveInHierarchyRecursive(childPrev);
    }
}
