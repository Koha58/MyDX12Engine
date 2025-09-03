#include "GameObject.h" // GameObjectクラスの定義を含むヘッダーファイル
#include "Scene.h"
#include "TransformComponent.h"
#include <algorithm>    // std::removeアルゴリズムを使用するためにインクルード

// --- GameObjectクラスの実装 ---

// GameObjectのコンストラクタ
// @param name: GameObjectに与えられる名前
GameObject::GameObject(const std::string& name)
    : Name(name) // GameObjectの名前を初期化
{
    // すべてのGameObjectはTransformComponentを持つことが前提。
    // AddComponentを通じてTransformComponentを追加し、Transformポインタに保持する。
    // これにより、TransformComponentはGameObjectの寿命管理下に入る。
    Transform = AddComponent<TransformComponent>();
}

// GameObjectのデストラクタ
GameObject::~GameObject()
{
    // このGameObjectが破棄される際、そのすべての子オブジェクトの親ポインタをクリアする。
    // これにより、子オブジェクトが「親なし」の状態になり、無効な親ポインタへのアクセスを防ぐ。
    for (const auto& child : m_Children)
    {
        child->m_Parent.reset(); // 子の親ポインタを解放
    }
}

// GameObjectとそのコンポーネント、子オブジェクトを更新する
// @param deltaTime: 前のフレームからの経過時間
void GameObject::Update(float deltaTime)
{
    // このGameObjectにアタッチされているすべてのコンポーネントを更新する
    for (const auto& comp : m_Components)
    {
        comp->Update(deltaTime);
    }

    // このGameObjectの子オブジェクトを再帰的に更新する
    for (const auto& child : m_Children)
    {
        child->Update(deltaTime);
    }
}

// このGameObjectに子オブジェクトを追加する
// @param child: 追加するGameObjectのshared_ptr
void GameObject::AddChild(std::shared_ptr<GameObject> child)
{
    // 追加しようとしている子オブジェクトに既に親がいるかチェック
    if (child->m_Parent.lock()) // weak_ptrをshared_ptrに変換して親オブジェクトにアクセス
    {
        // 既に親がいる場合は、その親から子オブジェクトを切り離す
        child->m_Parent.lock()->RemoveChild(child);
    }
    m_Children.push_back(child);       // 子オブジェクトのリストに追加
    child->m_Parent = shared_from_this(); // 子オブジェクトの親ポインタをこのオブジェクトに設定
}

// このGameObjectから子オブジェクトを削除する
// @param child: 削除するGameObjectのshared_ptr
void GameObject::RemoveChild(std::shared_ptr<GameObject> child)
{
    // m_Childrenリストから指定された子オブジェクトを削除
    // std::removeは指定された値をリストの末尾に移動させ、新しい論理的な終端を返す
    // eraseはその論理的な終端から物理的な終端までを削除する
    m_Children.erase(std::remove(m_Children.begin(), m_Children.end(), child), m_Children.end());
    child->m_Parent.reset(); // 子オブジェクトの親ポインタをクリアする

    // 親から外れたら Scene のルートに戻す
    if (auto scene = child->m_Scene.lock()) {
        scene->AddGameObject(child, nullptr);
    }
}

void GameObject::Destroy()
{
    if (m_Destroyed) return; // 既に破棄済みなら何もしない
    m_Destroyed = true;

    // コンポーネントに OnDestroy を通知
    for (auto& comp : m_Components)
    {
        comp->OnDestroy();
    }
    m_Components.clear();

    // 子オブジェクトを再帰的に破棄
    for (auto& child : m_Children)
    {
        child->Destroy();
    }
    m_Children.clear();
}
