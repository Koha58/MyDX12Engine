#pragma once
#include <memory>

// 前方宣言
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

    // GameObjectが破棄されるときに呼ばれる
    virtual void OnDestroy() {} // デフォルトは何もしない

    // 追加：オーナー設定関数
    void SetOwner(std::shared_ptr<GameObject> owner) { m_Owner = owner; }

    // 追加：オーナー取得関数
    std::shared_ptr<GameObject> GetOwner() const { return m_Owner.lock(); }

protected:
    ComponentType m_Type;

    // 追加：オーナーへの弱参照
    std::weak_ptr<GameObject> m_Owner;
};
