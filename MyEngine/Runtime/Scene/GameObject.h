#pragma once // ���d�C���N���[�h�h�~

#include <vector>        // �R���|�[�l���g/�q�I�u�W�F�N�g�̕ێ�
#include <string>        // ���O
#include <memory>        // shared_ptr, weak_ptr, enable_shared_from_this
#include <type_traits>   // is_base_of�i�e���v���[�g����j

#include "Components/TransformComponent.h" // �K�{�R���|�[�l���g�i�t�^�� Create() ���ōs���j
#include "Components/Component.h"          // �R���|�[�l���g���

// �O���錾�i���S��`�͕s�v�����A�Q��/�|�C���^�Ƃ��Ďg�����߁j
class Component;
class MeshRendererComponent; // MeshRendererComponent �͕ʃw�b�_�Œ�`
class Scene;
class D3D12Renderer;

// ============================================================================
// GameObject
// ----------------------------------------------------------------------------
// �E�V�[���c���[�̊�{�m�[�h�B�u�R���|�[�l���g�̏W���́v�Ƃ��ĐU�镑���B
// �ETransform �͕K���ێ��i��ԏ�̈ʒu/��]/�X�P�[���̍��j�B
// �E�e�q�֌W�i�K�w�j�� activeSelf / ActiveInHierarchy�i�����L���j/ �j�����Ǘ��B
// �E���C�t�T�C�N�����j�iB�Ăœ���j
//    - AddComponent: SetOwner �� Awake �𑦎��AActiveInHierarchy ���� enabled �Ȃ� OnEnable ������
//    - Start: �u�ŏ��� Update �̒��O�v�� 1 �񂾂��iGameObject::Update ���� HasStarted �����Ď��s�j
//    - OnEnable/OnDisable: **ActiveInHierarchy �̕ω�**���̂ݔ��΁i�q�� activeSelf �͏��������Ȃ��j
// �Eshared_from_this() ���g�����߁A**�K�� shared_ptr �Ǘ����Ő���**���邱�ƁB
//   �� new ���Ăтł͂Ȃ� GameObject::Create() ���g���B
// ============================================================================
class GameObject : public std::enable_shared_from_this<GameObject>
{
public:
    //--------------------------------------------------------------------------
    // Create
    //  ����������i�Bshared_ptr �Ǘ����ŃC���X�^���X�����A**Transform �����S�ɕt�^**����B
    //  �����i.cpp�j���ŁFmake_shared �� AddComponent<TransformComponent>() �� Transform �ɕێ��B
    //  ����ɂ�� ctor ���� shared_from_this() ���g���K�v���Ȃ��Abad_weak_ptr ������ł���B
    //--------------------------------------------------------------------------
    static std::shared_ptr<GameObject> Create(const std::string& name = "GameObject");

    // �R���X�g���N�^
    //  - �����ł� **Transform �����Ȃ�**�iCreate() ���ň��S�ɕt�^���邽�߁j�B
    explicit GameObject(const std::string& name = "GameObject");
    ~GameObject();

    // ���J�t�B�[���h�i�p�r�����m�ŕp�ɂɐG�邽�ߌ��J�j
    std::string Name;                                   // �f�o�b�O/���ʗp
    std::shared_ptr<TransformComponent> Transform;      // �K�{�BCreate() ���� AddComponent ���Ċ�����

    // ============================== �R���|�[�l���g�Ǘ� ==============================
    /**
     * @brief �C�ӂ� Component �h����ǉ�����B
     * @details
     *  1) �R���|�[�l���g�𐶐����ď��L���X�g�ɒǉ�
     *  2) Owner ������(shared_from_this())�ɐݒ�
     *  3) Awake() �𑦎��Ăяo��
     *  4) �ǉ����_�� ActiveInHierarchy && comp.enabled �Ȃ� OnEnable() �𑦎��Ă�
     * @note Create() �Ő��������ushared_ptr �Ǘ����v�ŌĂԂ��Ɓishared_from_this ���S���j�B
     */
    template<typename T, typename... Args>
    std::shared_ptr<T> AddComponent(Args&&... args)
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");

        // 1) ���� & ���L���X�g�֒ǉ�
        auto component = std::make_shared<T>(std::forward<Args>(args)...);
        m_Components.push_back(component);

        // 2) Owner �����i�R���|�[�l���g���� GameObject �ɃA�N�Z�X�ł���悤�ɂ���j
        component->SetOwner(shared_from_this()); // �� Component ���� SetOwner(shared_ptr<GameObject>) ������O��

        // 3) Awake�i�ˑ��̔����������͂����Ŋ���������j
        component->Awake();

        // 4) �ǉ����ォ�瓮�삳�������ꍇ�A�����I�ɗL���Ȃ� OnEnable �𑦎�����
        const bool activeInHierarchy = IsActive();
        if (activeInHierarchy && component->IsEnabled()) {
            component->OnEnable();
        }
        return component;
    }

    // GetComponent�i�ŏ��Ɍ������� 1 ����Ԃ��j
    template<typename T>
    std::shared_ptr<T> GetComponent()
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");
        for (const auto& comp : m_Components)
        {
            if (auto casted = std::dynamic_pointer_cast<T>(comp)) {
                return casted;
            }
        }
        return nullptr;
    }

    // ================================ �V�[���Q�� ================================
    std::shared_ptr<Scene> GetScene() const { return m_Scene.lock(); }
    void SetScene(std::shared_ptr<Scene> scene) { m_Scene = std::move(scene); }

    // �e�i���S�� shared_ptr �Ŏ擾�j
    std::shared_ptr<GameObject> GetParent() const { return m_Parent.lock(); }

    // ================================ �j��/���� ================================
    explicit operator bool() const { return !m_Destroyed; }
    bool IsDestroyed() const { return m_Destroyed; }

    // Destroy �́u�\�񁨃t���[���I�[���s�v�^�p�ɍ��킹�邽�߁A�܂��t���O�������Ă�B
    void MarkAsDestroyed() { m_Destroyed = true; }

    // ���̔j���iOnDestroy ����/�q�̍ċA�j���Ȃǁj�B�Ăяo�����iScene�j�������𓝈�Ǘ��B
    void Destroy();

    // ================================ �L��/���� ================================
    /**
     * @brief activeSelf �̐ؑ�
     * @details
     *  - �q�� **activeSelf �͕ύX���Ȃ�**�iUnity �Ɠ��l�j
     *  - ActiveInHierarchy �̕ω����������o���� OnEnable/OnDisable �𔭉�
     *  - Scene �Ǘ��i���[�g�z��o�^/���O�j�Ƃ̓����� .cpp �������ōs��
     */
    void SetActive(bool active);

    /**
     * @brief ActiveInHierarchy ��Ԃ�
     * @details ������ activeSelf �� false �Ȃ� false�B�e������ΐe�� IsActive() �ɏ]���B
     */
    bool IsActive() const;

    // ================================ �`��/�X�V ================================
    // Render: �����̕`��n�R���|�[�l���g �� �q�� Render ���ċA�Ăяo��
    void Render(class D3D12Renderer* renderer);

    // Update: �q�� Update �� �����̃R���|�[�l���g Update/LateUpdate
    //         Start �́u�ŏ��� Update �O�v�� 1 �񂾂��iHasStarted �t���O�ŊǗ��j
    void Update(float deltaTime);

    // ================================ �K�w���� ================================
    /**
     * @brief �q��ǉ�
     * @details �����̐e������΂����炩��f�^�b�`���Ă��玩���փA�^�b�`����B
     *          ActiveInHierarchy �̍����K�p�𐳂����`�d����B
     */
    void AddChild(std::shared_ptr<GameObject> child);

    /**
     * @brief �q���O��
     * @details �����̎q�z�񂩂珜�O�BScene ���|���V�[�ɂ��u���[�g�֖߂��v���̉^�p���\�B
     *          ActiveInHierarchy �̍����K�p�𐳂����`�d����B
     */
    void RemoveChild(std::shared_ptr<GameObject> child);

    // �q�̈ꗗ�i�ǂݎ���p�j
    const std::vector<std::shared_ptr<GameObject>>& GetChildren() const { return m_Children; }

private:
    // ===== ���L�R���e�i =====
    std::vector<std::shared_ptr<Component>>  m_Components; // �A�^�b�`�ς݃R���|�[�l���g
    std::vector<std::shared_ptr<GameObject>> m_Children;   // �q GameObject

    // ===== �֘A�Q�Ɓi�z�Q�Ɖ���̂��� weak�j=====
    std::weak_ptr<GameObject> m_Parent; // �e
    std::weak_ptr<Scene>      m_Scene;  // �����V�[���iScene �����L�j

    // ===== ��ԃt���O =====
    bool m_Destroyed = false;  // �j���\��/�j���ς�
    bool m_Active = true;   // activeSelf�i�������g�� ON/OFF�j�B�f�t�H���g�L���B

    // ���߂� ActiveInHierarchy ���L���b�V�����č������o�iOnEnable/OnDisable �𐳂������΁j
    bool m_LastActiveInHierarchy = true;

    // ===== ActiveInHierarchy �����`�d�w���p�[ =====
    // ������ activeSelf �Ɛe�� IsActive() ���������Ԃ��Z�o
    bool ComputeActiveInHierarchy() const;

    // ����������� OnEnable/OnDisable �𔭉΂��A�L���b�V�����X�V
    void ApplyActiveInHierarchyDelta(bool wasActiveInHierarchy, bool nowActiveInHierarchy);

    // �����̍���������A�q�ցu���ꂼ��̒��O��ԁv��n���Ȃ���ċA�K�p
    void RefreshActiveInHierarchyRecursive(bool prevOfThis);

    // Scene ���e�q�� Scene �Q�Ƃ𒼐ڑ���ł���悤��
    friend class Scene;
};

// ================================ �݌v���� ================================
// �E������ Create() ���g���Fshared_ptr �Ǘ����� Transform ��t�^ �� ctor �� shared_from_this() ���Ȃ�
//   �� std::bad_weak_ptr ���m���ɉ���B
// �EAddComponent �� Awake/OnEnable �^�C�~���O�� B�Ăɓ���iScene ���ł� Awake ���Ă΂Ȃ��j�B
// �EStart �� GameObject::Update() �� HasStarted ������ 1 �񂾂��ĂԎ����ɑ����邱�ƁB
// �ESetActive �� activeSelf �̂ݕύX�B�q�� activeSelf �͘M��Ȃ��B
//   �� ������� ActiveInHierarchy �̕ω����������o���� OnEnable/OnDisable �𐳂������΁B
// �EGetComponent �� O(N)�B�z�b�g�p�X�̓n���h��/�L���b�V��/�^ID�C���f�b�N�X���������B
// �E�X���b�h�Z�[�t�ł͂Ȃ��i�X�V/�`��/�K�w�ύX�̓��C���X���b�h�O��j�B
// ============================================================================
