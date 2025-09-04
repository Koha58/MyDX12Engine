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

    // --- Unity風ライフサイクル ---
    virtual void Awake() {}                    // 生成直後に1回だけ
    virtual void OnEnable() {}                 // 有効化時
    virtual void Start() {}                    // 最初のUpdate前に1回だけ
    virtual void Update(float deltaTime) {}    // 毎フレーム
    virtual void LateUpdate(float deltaTime) {}// Update後に毎フレーム
    virtual void OnDisable() {}                // 無効化時
    virtual void OnDestroy() {}                // 破棄時

    // --- オーナー管理 ---
    void SetOwner(std::shared_ptr<GameObject> owner) { m_Owner = owner; }
    std::shared_ptr<GameObject> GetOwner() const { return m_Owner.lock(); }

    // --- 状態管理 ---
    bool IsEnabled() const { return m_Enabled; }
    
    // 有効/無効
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

    // 状態フラグ
    bool m_Started = false;  // Start() が呼ばれたかフラグ
    bool m_Enabled = true;   // OnEnable / OnDisable 用
};
