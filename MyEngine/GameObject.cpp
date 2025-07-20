#include "GameObject.h" // GameObjectクラスの定義を含むヘッダーファイル
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
}

// --- Sceneクラスの実装 ---

// Sceneのコンストラクタ
// @param name: シーンに与えられる名前
Scene::Scene(const std::string& name)
    : m_Name(name) // シーンの名前を初期化
{
}

// シーンにルートGameObjectを追加する
// @param gameObject: 追加するGameObjectのshared_ptr
void Scene::AddGameObject(std::shared_ptr<GameObject> gameObject)
{
    // シーンのルートGameObjectリストにGameObjectを追加する
    // 注意: GameObjectが既に別のGameObjectの子として追加されている場合、
    // AddChildロジックがそのGameObjectを以前の親から切り離すため、
    // ここでルートとして追加するのは適切でない場合がある。
    // 通常、ルートオブジェクトは親を持たない。
    m_RootGameObjects.push_back(gameObject);
}

// シーンからルートGameObjectを削除する
// @param gameObject: 削除するGameObjectのshared_ptr
void Scene::RemoveGameObject(std::shared_ptr<GameObject> gameObject)
{
    // m_RootGameObjectsリストから指定されたGameObjectを削除
    m_RootGameObjects.erase(std::remove(m_RootGameObjects.begin(), m_RootGameObjects.end(), gameObject), m_RootGameObjects.end());
}

// シーン内のすべてのルートGameObjectを更新する
// @param deltaTime: 前のフレームからの経過時間
void Scene::Update(float deltaTime)
{
    // シーン内の各ルートGameObjectを更新する
    // 各GameObjectのUpdateメソッドが自身の子オブジェクトも再帰的に更新するため、
    // ここではルートオブジェクトのみをイテレートすればよい
    for (const auto& go : m_RootGameObjects)
    {
        go->Update(deltaTime);
    }
}