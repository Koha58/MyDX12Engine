#pragma once // ���d�C���N���[�h�h�~

#include <vector>        // �R���|�[�l���g/�q�I�u�W�F�N�g�̕ێ�
#include <string>        // ���O
#include <DirectXMath.h> // Transform �ōs��/�x�N�g�����g��
#include <memory>        // shared_ptr, weak_ptr, enable_shared_from_this
#include <type_traits>   // is_base_of�i�e���v���[�g����j

#include "Components/TransformComponent.h" // �K�{�R���|�[�l���g�i�t�^�� Create() ���ōs���j
#include "Components/Component.h"          // �R���|�[�l���g���

// �O���錾�i���S��`�͕s�v�����A�Q��/�|�C���^�Ƃ��Ďg�����߁j
class Component;
class MeshRendererComponent; // Mesh.h �Œ�`
class Scene;
class D3D12Renderer;

// ============================================================================
// GameObject
//  - �V�[���c���[�̊�{�m�[�h�B�u�R���|�[�l���g�̏W���́v�Ƃ��ĐU�镑���B
//  - Transform �͕K���ێ��i��ԏ�̈ʒu/��]/�X�P�[���̍��j�B
//  - �e�q�֌W�i�K�w�j�� activeSelf / ActiveInHierarchy�i�����L���j/ �j�����Ǘ��B
//  - ���C�t�T�C�N�����j�iB�Ăœ���j
//      * AddComponent: SetOwner �� Awake �𑦎��AActiveInHierarchy ���� enabled �Ȃ� OnEnable ������
//      * Start: �u�ŏ��� Update �̒��O�v�� 1 �񂾂��iGameObject::Update ���� HasStarted �����Ď��s�j
//      * OnEnable/OnDisable: **ActiveInHierarchy �̕ω�**���̂ݔ��΁i�q�� activeSelf �͏��������Ȃ��j
//  - �d�v�Fshared_from_this() ���g�����߁A**�K�� shared_ptr �Ǘ����Ő���**���邱�ƁB
//    �� ���|�C���^ new �ł͂Ȃ� GameObject::Create() ���g���B
// ============================================================================
class GameObject : public std::enable_shared_from_this<GameObject>
{
public:
    // ------------------------------------------------------------------------
    // Create
    //  - ����������i�Bshared_ptr �Ǘ����ŃC���X�^���X�����A**Transform �����S�ɕt�^**����B
    //  - �����i.cpp�j���ŁFmake_shared �� AddComponent<TransformComponent>() �� Transform �ɕێ��A�ȂǁB
    //  - ����ɂ�� ctor ���� shared_from_this() ���g���K�v���Ȃ��Abad_weak_ptr ������ł���B
    // ------------------------------------------------------------------------
    static std::shared_ptr<GameObject> Create(const std::string& name = "GameObject");

    // �R���X�g���N�^
    //  - �����ł� **Transform �����Ȃ�**�ishared_from_this() �����g�p�̂��߁j�B
    //  - Transform �̕t�^�� Create() ���ň��S�Ɏ��{����B
    GameObject(const std::string& name = "GameObject");
    ~GameObject();

    // ���J�t�B�[���h�i�p�r�����m�ŕp�ɂɐG�邽�ߌ��J�j
    std::string Name;                                   // �f�o�b�O/���ʗp
    std::shared_ptr<TransformComponent> Transform;      // �K�{�BCreate() ���� AddComponent ���Ċ�����

    // ============================== �R���|�[�l���g�Ǘ� ==============================
    // AddComponent
    //  - �C�ӂ� Component �h����ǉ��B
    //  - Awake �𑦎��Ăяo���A**���̎��_�� ActiveInHierarchy ���R���|�[�l���g�� Enabled**
    //    �Ȃ� OnEnable �������Ăԁi= �ǉ�����ɓ����o����@�j�B
    //  - ���ӁF�{�̂� shared_from_this() ���g�����߁A**Create() �Ő���������**�ɌĂԂ��ƁB
    template<typename T, typename... Args>
    std::shared_ptr<T> AddComponent(Args&&... args)
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");

        // ���� & ���L���X�g�֒ǉ�
        auto component = std::make_shared<T>(std::forward<Args>(args)...);
        m_Components.push_back(component);

        // Owner �����i�R���|�[�l���g���� GameObject �ɃA�N�Z�X�ł���悤�ɂ���j
        component->SetOwner(shared_from_this()); // �� SetOwner(shared_ptr<GameObject>) ������

        // ���C�t�T�C�N���iB�āj
        component->Awake(); // �ˑ��̔����������͂���

        // Unity �����FActiveInHierarchy && comp.enabled �̂Ƃ����� OnEnable
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
            auto casted = std::dynamic_pointer_cast<T>(comp);
            if (casted) return casted;
        }
        return nullptr;
    }

    // ================================ �V�[���Q�� ================================
    std::shared_ptr<Scene> GetScene() const { return m_Scene.lock(); }
    void SetScene(std::shared_ptr<Scene> scene) { m_Scene = scene; }

    // �e�i���S�� shared_ptr �Ŏ擾�j
    std::shared_ptr<GameObject> GetParent() const { return m_Parent.lock(); }

    // ================================ �j��/���� ================================
    explicit operator bool() const { return !m_Destroyed; }
    bool IsDestroyed() const { return m_Destroyed; }
    void MarkAsDestroyed() { m_Destroyed = true; } // �x���j���L���[�A�g�p�iScene �������p�j
    void Destroy(); // OnDestroy �̔���/�q�̍ċA�j���Ȃǁi���̂� .cpp�j

    // ================================ �L��/���� ================================
    // SetActive(activeSelf �̐ؑ�)
    //  - �q�� **activeSelf �͕ύX���Ȃ�**�iUnity �Ɠ��l�j�B
    //  - ������ԁiActiveInHierarchy�j���ω������m�[�h�ɑ΂��Ă̂� OnEnable/OnDisable �𔭉΁B
    //  - �����ł́u���O�� ActiveInHierarchy�im_LastActiveInHierarchy�j�v�Ɣ�r���č��������o�B
    void SetActive(bool active);

    // IsActive
    //  - ActiveInHierarchy ��Ԃ��i= ������ activeSelf && �e�� IsActive()�j�B
    //  - �e�������Ȃ玩�������������B
    bool IsActive() const;

    // ================================ �`��/�X�V ================================
    // Render: �����̕`��n�R���|�[�l���g �� �q�� Render ���ċA�Ăяo��
    void Render(class D3D12Renderer* renderer);
    // Update: �q�� Update �� �����̃R���|�[�l���g Update/LateUpdate�iStart �͍ŏ��� Update �O�� 1 ��j
    void Update(float deltaTime);

    // ================================ �K�w���� ================================
    // AddChild: �����e����̃f�^�b�` �� �����ɃA�^�b�`
    void AddChild(std::shared_ptr<GameObject> child);
    // RemoveChild: �����̎q���X�g���珜�O�iScene ���|���V�[�Ń��[�g�߂����j
    void RemoveChild(std::shared_ptr<GameObject> child);
    // �q�̈ꗗ�i�ǂݎ���p�j
    const std::vector<std::shared_ptr<GameObject>>& GetChildren() const { return m_Children; }

private:
    // ===== ������� =====
    std::vector<std::shared_ptr<Component>> m_Components; // �A�^�b�`�ς݃R���|�[�l���g
    std::vector<std::shared_ptr<GameObject>> m_Children;  // �q GameObject
    std::weak_ptr<GameObject> m_Parent;                   // �e�i�z�Q�Ɖ���̂��� weak�j
    std::weak_ptr<Scene>      m_Scene;                    // �����V�[���iScene �����L�j

    bool m_Destroyed = false; // �j���\��/�j���ς�
    bool m_Active = true;  // activeSelf�i�������g�� ON/OFF�j�B�f�t�H���g�L���B

    // ���߂� ActiveInHierarchy ���L���b�V�����č������o�iOnEnable/OnDisable �𐳂������΁j
    bool m_LastActiveInHierarchy = true;

    // ===== ActiveInHierarchy �����`�d�w���p�[ =====
    // ComputeActiveInHierarchy: ������ activeSelf �Ɛe�� IsActive() ���������Ԃ��Z�o
    bool ComputeActiveInHierarchy() const;
    // ApplyActiveInHierarchyDelta: ����������� OnEnable/OnDisable �𔭉�
    void ApplyActiveInHierarchyDelta(bool wasActiveInHierarchy, bool nowActiveInHierarchy);
    // RefreshActiveInHierarchyRecursive: �����̍���������A�q�ցu�e�̏�ԁv��n���Ȃ���ċA�K�p
    void RefreshActiveInHierarchyRecursive(bool prevOfThis);

    // Scene ���e�q�� Scene �Q�Ƃ𒼐ڑ���ł���悤��
    friend class Scene;
};

// ================================ �݌v���� ================================
// �E������ Create() ���g���Fshared_ptr �Ǘ����� Transform ��t�^ �� ctor �� shared_from_this() ���Ȃ�
//   �� std::bad_weak_ptr ���m���ɉ���B
// �EAddComponent �� Awake/OnEnable �^�C�~���O�� B�Ăɓ���iScene ���ł� Awake ���Ă΂Ȃ��j�B
// �EStart �� GameObject::Update() �� HasStarted ������ 1 �񂾂��ĂԎ����ɑ����邱�ƁB
// �ESetActive �� activeSelf ������ς���B�q�� activeSelf �͘M��Ȃ��B
//   �� ������� ActiveInHierarchy �̕ω����������o���� OnEnable/OnDisable �𐳂������΁B
// �EGetComponent �� O(N)�B�z�b�g�p�X�̓n���h��/�L���b�V��/�^ID�C���f�b�N�X���������B
// �E�X���b�h�Z�[�t�ł͂Ȃ��i�X�V/�`��/�K�w�ύX�̓��C���X���b�h�O��j�B
// ============================================================================
