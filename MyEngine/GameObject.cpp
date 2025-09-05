#include "GameObject.h" // GameObjectクラスの定義
#include "Scene.h"
#include "Component.h"
#include "TransformComponent.h"
#include <algorithm>    // std::remove を使用するためにインクルード

// =====================================================
// GameObject クラスの実装
// =====================================================

// -----------------------------------------------------
// コンストラクタ
// @param name : GameObject に与えられる名前
// -----------------------------------------------------
GameObject::GameObject(const std::string& name)
    : Name(name) // 名前を初期化
{
    // すべての GameObject は TransformComponent を必ず持つ。
    // → AddComponent 経由で生成し、Transform メンバに保持。
    //   これにより、座標/回転/スケール情報を持たない GameObject は存在しない設計になる。
    Transform = AddComponent<TransformComponent>();
}

// -----------------------------------------------------
// デストラクタ
// -----------------------------------------------------
GameObject::~GameObject()
{
    // 破棄時に子オブジェクトの親ポインタをクリアしておく。
    // → dangling weak_ptr（親が既に解放済なのに参照し続ける）を防ぐ。
    for (const auto& child : m_Children)
    {
        child->m_Parent.reset();
    }
}

// -----------------------------------------------------
// Update
// ・自身と子オブジェクトを更新する
// ・Scene の更新ループから呼ばれる
// @param deltaTime : 前フレームからの経過時間
// -----------------------------------------------------
void GameObject::Update(float deltaTime)
{
    if (m_Destroyed || !IsActive()) return; // 無効/破棄済みなら処理しない

    // --- 子オブジェクトの更新 ---
    // 再帰的に呼ぶことでツリー構造全体が更新される
    for (auto& child : m_Children) {
        child->Update(deltaTime);
    }

    // --- コンポーネント更新 ---
    for (auto& comp : m_Components)
    {
        if (!comp) continue;

        // Start は最初の Update 前に一度だけ呼ぶ（Unityライク）
        if (!comp->HasStarted()) {
            comp->Start();
            comp->MarkStarted();
        }

        // 有効状態のコンポーネントのみ Update
        if (comp->IsEnabled()) {
            comp->Update(deltaTime);
        }
    }

    // --- LateUpdate ---
    // Update の後処理が必要な場合に呼ばれる
    for (auto& comp : m_Components)
    {
        if (comp && comp->IsEnabled()) {
            comp->LateUpdate(deltaTime);
        }
    }
}

// -----------------------------------------------------
// 子オブジェクトの追加
// @param child : 追加する GameObject
// -----------------------------------------------------
void GameObject::AddChild(std::shared_ptr<GameObject> child)
{
    // すでに親がある場合は、その親から取り外す
    if (child->m_Parent.lock())
    {
        child->m_Parent.lock()->RemoveChild(child);
    }

    // 自分の子リストに追加し、親ポインタを設定
    m_Children.push_back(child);
    child->m_Parent = shared_from_this();
}

// -----------------------------------------------------
// 子オブジェクトの削除
// @param child : 削除対象
// -----------------------------------------------------
void GameObject::RemoveChild(std::shared_ptr<GameObject> child)
{
    // remove-erase イディオムで削除
    m_Children.erase(
        std::remove(m_Children.begin(), m_Children.end(), child),
        m_Children.end());

    // 親ポインタをクリア
    child->m_Parent.reset();

    // 親から外れたオブジェクトは Scene のルートに戻す
    if (auto scene = child->m_Scene.lock()) {
        scene->AddGameObject(child, nullptr);
    }
}

// -----------------------------------------------------
// Active 状態の設定
// ・自身と子の OnEnable/OnDisable を呼び出す
// ・Scene 内の管理リストに反映
// -----------------------------------------------------
void GameObject::SetActive(bool active)
{
    if (m_Active == active) return; // 変化なしなら何もしない
    m_Active = active;

    // --- コンポーネントに通知 ---
    for (auto& comp : m_Components)
    {
        if (!comp) continue;
        if (m_Active) comp->OnEnable();
        else comp->OnDisable();
    }

    // --- Scene 管理に反映 ---
    if (auto scene = m_Scene.lock())
    {
        if (m_Active)
        {
            // Scene のルートリストに存在しないなら追加
            if (!scene->ContainsRootGameObject(shared_from_this())) {
                scene->AddGameObject(shared_from_this());
            }
        }
        else
        {
            // 無効化されたら Scene の管理対象から外す
            scene->RemoveGameObject(shared_from_this());
        }
    }

    // --- 子オブジェクトにも再帰的に適用 ---
    for (auto& child : m_Children)
    {
        if (child) child->SetActive(active);
    }
}

// -----------------------------------------------------
// Active 状態の判定
// ・自身が無効なら false
// ・親が無効なら子も無効
// -----------------------------------------------------
bool GameObject::IsActive() const
{
    if (!m_Active) return false;

    auto parent = m_Parent.lock();
    if (parent) {
        return parent->IsActive();
    }

    return true;
}

// -----------------------------------------------------
// Destroy
// ・コンポーネント/子を破棄し OnDestroy を呼ぶ
// ・破棄済みなら何もしない
// -----------------------------------------------------
void GameObject::Destroy()
{
    if (m_Destroyed) return;
    m_Destroyed = true;

    // --- コンポーネント破棄 ---
    for (auto& comp : m_Components)
    {
        comp->OnDestroy();
    }
    m_Components.clear();

    // --- 子オブジェクトも再帰的に破棄 ---
    for (auto& child : m_Children)
    {
        child->Destroy();
    }
    m_Children.clear();
}

// -----------------------------------------------------
// Render
// ・自身のコンポーネント描画 → 子オブジェクト描画
// -----------------------------------------------------
void GameObject::Render(D3D12Renderer* renderer)
{
    if (!m_Active) return;

    // --- コンポーネントの描画 ---
    for (auto& comp : m_Components)
    {
        if (comp) comp->Render(renderer);
    }

    // --- 子オブジェクトの描画 ---
    for (auto& child : m_Children)
    {
        if (child) child->Render(renderer);
    }
}
