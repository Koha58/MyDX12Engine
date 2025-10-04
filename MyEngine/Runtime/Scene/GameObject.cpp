#include "GameObject.h" // GameObject�N���X�̒�`
#include "Scene.h"
#include "Components/Component.h"
#include "Components/TransformComponent.h"
#include <algorithm>    // std::remove ���g�p���邽�߂ɃC���N���[�h

// ============================================================================
// GameObject
//  - �K�w�i�e�q�j�\�������u�V�[�����̃G���e�B�e�B�v
//  - �Œ���� Transform�i�ʒu/��]/�X�P�[���j�ƁA�C�Ӑ��� Component ��ێ�
//  - ���C�t�T�C�N���iUnity�������̕��jB�œ���j:
//      * AddComponent �� Awake �𑦎��A�iActiveInHierarchy�Ȃ�jOnEnable �𑦎�
//      * Start �́u�ŏ��� Update �̒��O�v�ɌĂԁi�x���j
//      * OnEnable/OnDisable �� ActiveInHierarchy �̕ω����̂ݔ���
//  - Scene �Ƃ̊֌W:
//      * Scene ���́u���[�g GameObject �̔z��v�����z��
//      * �q��e����O�����ꍇ�́AScene �̃��[�g�֖߂��d�l�i���B�\�����ێ��j
//  - ���L���L: GameObject ���g�� shared_ptr �ň����A�e�� weak_ptr �ŎQ�Ɓi�z�Q�Ƃ������j
// ============================================================================

// ---- �����w���p�[�i�ǂ݂₷���̂��߂̏�����j --------------------------------
namespace {
    // remove-erase ��ǂ݂₷���܂Ƃ߂��w���p
    template<class T, class U>
    void erase_remove(T& v, const U& value) {
        v.erase(std::remove(v.begin(), v.end(), value), v.end());
    }
}

// [CHANGED] �t�@�N�g�������Fmake_shared ���i��O���S���y�ʃA���P�[�V�����j
//  - �K�� shared_ptr �Ǘ����Ő��� �� ����� Transform �����S�ɕt�^
//  - ctor ���ł� Transform ����炸�A������ AddComponent ����ishared_from_this �����S�ɂȂ�j
std::shared_ptr<GameObject> GameObject::Create(const std::string& name)
{
    auto go = std::make_shared<GameObject>(name);          // �� make_shared �ɕύX
    go->Transform = go->AddComponent<TransformComponent>(); // �K�{�R���|�[�l���g�𑦕t�^
    return go;
}

// ----------------------------------------------------------------------------
// �R���X�g���N�^
// @param name : GameObject �ɗ^�����閼�O�i�f�o�b�O/�����p�j
//  - Transform �� **�����ł͍��Ȃ�**�iCreate() ���ň��S�ɕt�^���邽�߁j�B
// ----------------------------------------------------------------------------
GameObject::GameObject(const std::string& name)
    : Name(name)
{
    // [NOTE] �ȑO�͂����� Transform �� Add ���Ă������Actor ���� shared_from_this() �͎g���Ȃ�����
    //        Create() �t�@�N�g���ֈړ������istd::bad_weak_ptr ����j�B

    // ��������� ActiveInHierarchy ���L���b�V���i�e�Ȃ���self�L���Ȃ̂� true�j
    m_LastActiveInHierarchy = IsActive();
}

// ----------------------------------------------------------------------------
// �f�X�g���N�^
//  - �j�����Ɂu�q�I�u�W�F�N�g�̐e�Q�Ɓiweak_ptr�j�v���N���A�B
//    �� ���̂̔j���iOnDestroy �Ăяo�����j�� Destroy() �Ŏ��{�B
// ----------------------------------------------------------------------------
GameObject::~GameObject()
{
    for (const auto& child : m_Children) {
        child->m_Parent.reset();
    }
}

// ----------------------------------------------------------------------------
// Update
//  - Scene �̍X�V���[�v���疈�t���[���Ă΂��B
//  - ������:
//     1) ���� return�i�j���ς�/��A�N�e�B�u�K�w�j
//     2) �q�I�u�W�F�N�g�� Update�i�e���q�̏��ATransform �ˑ��Ɏ��R�j
//     3) ���g�̑S�R���|�[�l���g Update�iStart �͏��񂾂��AEnabled �̂݁j
//     4) ���g�̑S�R���|�[�l���g LateUpdate�iEnabled �̂݁j
// ----------------------------------------------------------------------------
void GameObject::Update(float deltaTime)
{
    // �����I�ɔ�A�N�e�B�u�i= ActiveInHierarchy false�j�Ȃ�X�L�b�v
    if (m_Destroyed || !IsActive()) return;

    // --- �q�I�u�W�F�N�g�̍X�V�i�e���q�j ---
    for (auto& child : m_Children) {
        child->Update(deltaTime);
    }

    // --- �R���|�[�l���g�̍X�V ---
    for (auto& comp : m_Components) {
        if (!comp) continue;

        // [Unity���C�N] �L�����Ɍ��� Start �����񂾂��Ă�
        if (!comp->HasStarted() && comp->IsEnabled() && IsActive()) {
            comp->Start();
            comp->MarkStarted();
        }

        if (comp->IsEnabled() && IsActive()) {
            comp->Update(deltaTime);
        }
    }

    // --- LateUpdate ---
    for (auto& comp : m_Components) {
        if (comp && comp->IsEnabled()) {
            comp->LateUpdate(deltaTime);
        }
    }
}

// ----------------------------------------------------------------------------
// AddChild
//  - ���ɑ��̐e������Ȃ���O���Ă��玩���ɕt���ւ���i�ǎ����̖h�~�j�B
//  - �e�q�֌W�̕ύX�� ActiveInHierarchy �ɉe�����邽�߁A**�����K�p**��
//    OnEnable/OnDisable �𐳂������΂�����B
// [NOTE] ���O�� RemoveChild ���u���[�g�֖߂� + �����K�p�v���s�����߁A
//        �u�t���ւ����Ɉ�u���[�g�֍s���Ă���ēx�t�^�v�Ƃ�����i�K�̕ω����N������B
//        ����� 1 ��̕ω��ɗ}�������ꍇ�́AReparent ��p API ��p�ӂ���
//        �܂Ƃ߂č����K�p����̂��x�^�[�B
// ----------------------------------------------------------------------------
void GameObject::AddChild(std::shared_ptr<GameObject> child)
{
    const bool prevChildHier = child->IsActive(); // �ǉ��O�̎q�̎�����Ԃ�ێ�

    // �����̐e������O���i�� RemoveChild �́g���[�g�߂� + �����K�p�h���s���݌v�j
    if (auto oldParent = child->m_Parent.lock()) {
        oldParent->RemoveChild(child);
    }

    // �����̎q�ɓo�^���A�e�Q�Ƃ𒣂�
    m_Children.push_back(child);
    child->m_Parent = shared_from_this();

    // Transform �̐e�q�֌W�𓯊��������ꍇ�͂�����
    // ��: child->Transform->SetParent(Transform.get());

    // �V�����e�̂��Ƃ� ActiveInHierarchy ���ω������獷����K�p
    child->RefreshActiveInHierarchyRecursive(prevChildHier);
}

// ----------------------------------------------------------------------------
// RemoveChild
//  - �����̎q�z�񂩂珜�O���A�e�Q�Ƃ��N���A�B
//  - �u�e����O�ꂽ�� Scene �̃��[�g�֖߂��v�|���V�[�ŊǗ����Ɏc���B
//  - ���̌��� ActiveInHierarchy ���ς��ꍇ�͍����K�p�B
// [NOTE] Reparent�i�t���ւ��j�V�i���I�ł� AddChild ���ł������K�p���邽�߁A
//       ��ԕω��� 2 ��A���ŋN������_�ɒ��Ӂi�K�v�Ȃ� Reparent API �������j�B
// ----------------------------------------------------------------------------
void GameObject::RemoveChild(std::shared_ptr<GameObject> child)
{
    const bool prevChildHier = child->IsActive();

    erase_remove(m_Children, child);
    child->m_Parent.reset();

    // ���[�g�֖߂��iScene �Ǘ����瓞�B�\�Ɂj
    if (auto scene = child->m_Scene.lock()) {
        scene->AddGameObject(child, nullptr); // �e nullptr = ���[�g�o�^
    }

    // ActiveInHierarchy �̍����K�p
    child->RefreshActiveInHierarchyRecursive(prevChildHier);
}

// ----------------------------------------------------------------------------
/* SetActive�iactiveSelf �̐ؑցj
   �d�v�����F
   - �q�� activeSelf �͕ύX���Ȃ��iUnity �Ɠ��l�j�B�e�� ON/OFF �ɂ�� �g������ԁh �����ω��B
   - OnEnable/OnDisable �� ActiveInHierarchy ���ω��������̂ݔ��΁B
   - Scene �Ǘ��i���[�g�z��j�ւ̓o�^/���O����������݌v�B
     [NOTE] Unity �ł͔�A�N�e�B�u�ł� Scene �ɂ͎c�邪�A�{�G���W����
           �u��A�N�e�B�u �� ����Ώۂ���O���v���j�B�v���W�F�N�g���j�ɍ��킹�ē�����B
*/
void GameObject::SetActive(bool active)
{
    const bool prevHier = IsActive(); // �ύX�O�̎������

    if (m_Active == active) {
        // self �ɕω��Ȃ��FScene ���̊Ǘ������y������
        if (auto scene = m_Scene.lock()) {
            // [NOTE] �v�FScene::ContainsRootGameObject ����
            if (m_Active) {
                if (!scene->ContainsRootGameObject(shared_from_this())) {
                    scene->AddGameObject(shared_from_this());
                }
            }
            else {
                scene->RemoveGameObject(shared_from_this());
            }
        }
        return;
    }

    // self ��ؑ�
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

// ----------------------------------------------------------------------------
// IsActive�iActiveInHierarchy �����j
//  - ���g�� activeSelf �� false �Ȃ� false
//  - �e������ΐe�� ActiveInHierarchy �ɏ]���i�e�������Ȃ玩���������j
// ----------------------------------------------------------------------------
bool GameObject::IsActive() const
{
    if (!m_Active) return false;

    if (auto parent = m_Parent.lock()) {
        return parent->IsActive();
    }
    return true; // ���[�g
}

// ----------------------------------------------------------------------------
// Destroy
//  - �p���BOnDisable�i�L���������R���|�[�l���g�̂݁j�� OnDestroy �� �q���ċA Destroy�B
//  - Scene/Parent ����̐؂藣���� Scene ���̔j���t���[�Ŏ��{�i�������ꌳ�Ǘ��j�B
// ----------------------------------------------------------------------------
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

    // �q�����l�ɍċA Destroy
    for (auto& child : m_Children) {
        child->Destroy();
    }
    m_Children.clear();
}

// ----------------------------------------------------------------------------
// Render
//  - �����I�ɔ�A�N�e�B�u�Ȃ�`�悵�Ȃ��B
//  - ���`��͊e�`��n�R���|�[�l���g�� Renderer �ɑ΂��čs���B
// ----------------------------------------------------------------------------
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

// ���݂� ActiveInHierarchy ���v�Z���ĕԂ��iIsActive �Ɠ��`�����Ӑ}���`��閽���j
bool GameObject::ComputeActiveInHierarchy() const
{
    return IsActive();
}

// ActiveInHierarchy �̕ω��������ɓK�p�iOnEnable/OnDisable �̔��΁��L���b�V���X�V�j
void GameObject::ApplyActiveInHierarchyDelta(bool wasActiveInHierarchy, bool nowActiveInHierarchy)
{
    if (wasActiveInHierarchy == nowActiveInHierarchy) {
        return; // �ω��Ȃ�
    }

    if (nowActiveInHierarchy) {
        for (auto& comp : m_Components) {
            if (comp && comp->IsEnabled()) comp->OnEnable();
        }
    }
    else {
        for (auto& comp : m_Components) {
            if (comp && comp->IsEnabled()) comp->OnDisable();
        }
    }

    m_LastActiveInHierarchy = nowActiveInHierarchy;
}

// �����Ɣz���i�c���[�S�́j�ɂ��āAActiveInHierarchy �̍�����K�p
// @param prevOfThis : �Ăяo�����O�ɂ�����u���̃m�[�h�́vActiveInHierarchy
void GameObject::RefreshActiveInHierarchyRecursive(bool prevOfThis)
{
    const bool now = ComputeActiveInHierarchy();
    ApplyActiveInHierarchyDelta(prevOfThis, now);

    // �q�ւ́u���ꂼ��̒��O��ԁi�L���b�V���j�v�� prev �Ƃ��ēn��
    for (auto& ch : m_Children) {
        if (!ch) continue;
        const bool childPrev = ch->m_LastActiveInHierarchy;
        ch->RefreshActiveInHierarchyRecursive(childPrev);
    }
}
