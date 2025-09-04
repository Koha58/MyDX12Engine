#pragma once
#include <memory>

// �O���錾
class GameObject;

class Component
{
public:
    enum ComponentType
    {
        None = 0,
        Transform,
        MeshRenderer,
        MAX_COMPONENT_TYPES
    };

    Component(ComponentType type) : m_Type(type) {}
    virtual ~Component() = default;

    ComponentType GetType() const { return m_Type; }

    // --- Unity�����C�t�T�C�N�� ---
    virtual void Awake() {}                    // ���������1�񂾂�
    virtual void OnEnable() {}                 // �L������
    virtual void Start() {}                    // �ŏ���Update�O��1�񂾂�
    virtual void Update(float deltaTime) {}    // ���t���[��
    virtual void LateUpdate(float deltaTime) {}// Update��ɖ��t���[��
    virtual void OnDisable() {}                // ��������
    virtual void OnDestroy() {}                // �j����

    // --- �I�[�i�[�Ǘ� ---
    void SetOwner(std::shared_ptr<GameObject> owner) { m_Owner = owner; }
    std::shared_ptr<GameObject> GetOwner() const { return m_Owner.lock(); }

    // --- ��ԊǗ� ---
    bool IsEnabled() const { return m_Enabled; }
    
    // �L��/����
    void SetEnabled(bool enabled) {
        if (m_Enabled != enabled) {
            m_Enabled = enabled;
            if (m_Enabled) OnEnable();
            else OnDisable();
        }
    }

    bool HasStarted() const { return m_Started; }
    void MarkStarted() { m_Started = true; }

protected:
    ComponentType m_Type;
    std::weak_ptr<GameObject> m_Owner;

    // ��ԃt���O
    bool m_Started = false;  // Start() ���Ă΂ꂽ���t���O
    bool m_Enabled = true;   // OnEnable / OnDisable �p
};
