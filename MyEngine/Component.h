#pragma once
#include <memory>

// 前方宣言（ヘッダ間依存を避けるため GameObject の完全な定義は不要）
class GameObject;

// ================================================
// Component 基底クラス
// --------------------------------
// ・全てのコンポーネントの共通基底クラス
// ・Transform, MeshRenderer, 自作の MoveComponent などがここから派生する
// ・Unity の MonoBehaviour に近いライフサイクルを想定している
// ================================================
class Component
{
public:
    // ----------------------------
    // コンポーネントの種類を識別する列挙型
    // ----------------------------
    enum ComponentType
    {
        None = 0,          // 汎用/未指定
        Transform,         // Transform コンポーネント
        MeshRenderer,      // MeshRenderer コンポーネント
        MAX_COMPONENT_TYPES
    };

    // コンストラクタでタイプを受け取って保持
    Component(ComponentType type) : m_Type(type) {}
    virtual ~Component() = default; // 継承クラスでもデストラクタが正しく呼ばれるよう仮想化

    // 自身の種類を取得
    ComponentType GetType() const { return m_Type; }

    // ----------------------------
    // Unity風ライフサイクル関数
    // ----------------------------
    // ※必要に応じて派生クラスでオーバーライドする
    virtual void Awake() {}                     // コンポーネント生成直後に一度だけ呼ばれる
    virtual void OnEnable() {}                  // 有効化された時に呼ばれる
    virtual void Start() {}                     // 最初の Update が呼ばれる直前に一度だけ呼ばれる
    virtual void Update(float deltaTime) {}     // 毎フレーム呼ばれる更新処理
    virtual void LateUpdate(float deltaTime) {} // Update の後に呼ばれる（順序制御用）
    virtual void OnDisable() {}                 // 無効化された時に呼ばれる
    virtual void OnDestroy() {}                 // 破棄される直前に呼ばれる

    // 描画用。MeshRenderer など描画処理を持つコンポーネントでオーバーライド
    virtual void Render(class D3D12Renderer* renderer) {}

    // ----------------------------
    // オーナー(GameObject)管理
    // ----------------------------
    // ・どの GameObject に属しているかを管理する
    // ・shared_ptr を避けて weak_ptr にすることで循環参照を防ぐ
    void SetOwner(std::shared_ptr<GameObject> owner) { m_Owner = owner; }
    std::shared_ptr<GameObject> GetOwner() const { return m_Owner.lock(); }

    // ----------------------------
    // 有効/無効フラグ管理
    // ----------------------------
    bool IsEnabled() const { return m_Enabled; }

    // 状態を切り替えると OnEnable/OnDisable を自動で呼び出す
    void SetEnabled(bool enabled) {
        if (m_Enabled != enabled) {
            m_Enabled = enabled;
            if (m_Enabled) OnEnable();
            else OnDisable();
        }
    }

    // ----------------------------
    // Start() 呼び出し管理
    // ----------------------------
    // ・Start は1回しか呼ばれないため、フラグで制御する
    bool HasStarted() const { return m_Started; }
    void MarkStarted() { m_Started = true; }

protected:
    ComponentType m_Type;           // このコンポーネントの種類
    std::weak_ptr<GameObject> m_Owner; // 所属する GameObject（循環参照を避けるため weak_ptr）

    // 状態フラグ
    bool m_Started = false; // Start() が既に呼ばれたかどうか
    bool m_Enabled = true;  // 現在有効かどうか（OnEnable/OnDisable の呼び出し制御用）
};
