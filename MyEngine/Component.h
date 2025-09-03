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

    virtual void Initialize() {}
    virtual void Update(float deltaTime) {}
    virtual void Render() {}

    // GameObject���j�������Ƃ��ɌĂ΂��
    virtual void OnDestroy() {} // �f�t�H���g�͉������Ȃ�

    // �ǉ��F�I�[�i�[�ݒ�֐�
    void SetOwner(std::shared_ptr<GameObject> owner) { m_Owner = owner; }

    // �ǉ��F�I�[�i�[�擾�֐�
    std::shared_ptr<GameObject> GetOwner() const { return m_Owner.lock(); }

protected:
    ComponentType m_Type;

    // �ǉ��F�I�[�i�[�ւ̎�Q��
    std::weak_ptr<GameObject> m_Owner;
};
