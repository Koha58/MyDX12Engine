#pragma once // 同じヘッダーファイルが複数回インクルードされるのを防ぐプリプロセッサディレクティブ

#include <vector>        // std::vectorを使用
#include <string>        // std::stringを使用
#include <DirectXMath.h> // DirectX::XMFLOAT3, DirectX::XMMATRIX などを使用
#include <memory>        // std::shared_ptr, std::weak_ptr, std::enable_shared_from_this を使用
#include <type_traits>   // std::is_base_of を使用（テンプレートメタプログラミング）

#include "TransformComponent.h" // これを追加
#include "Component.h"

// 前方宣言:
// これらのクラスの完全な定義はここでは不要だが、ポインタや参照型として使用するために宣言
class Component;
class MeshRendererComponent; // Mesh.h で定義される
class Scene;


// --- GameObjectクラス ---
// シーン内のすべてのオブジェクトの基盤となるクラス
// コンポーネントの集合体として、ゲームロジックと描画可能なアセットを表現する
class GameObject : public std::enable_shared_from_this<GameObject> // shared_ptrから自身のshared_ptrを取得可能にする
{
public:
    std::string Name;                                  // GameObjectの名前
    std::shared_ptr<TransformComponent> Transform;     // すべてのGameObjectはTransformComponentを持つ（公開アクセス）

    // コンストラクタ
    // @param name: GameObjectの名前 (デフォルトは"GameObject")
    GameObject(const std::string& name = "GameObject");

    // デストラクタ
    ~GameObject();

    // コンポーネントを追加するテンプレート関数
    // @typename T: 追加するコンポーネントの型 (Componentから派生している必要がある)
    // @param Args&&... args: コンポーネントのコンストラクタに渡す引数
    template<typename T, typename... Args>
    std::shared_ptr<T> AddComponent(Args&&... args)
    {
        // 静的アサート: T が Component の基底クラスであるかをコンパイル時にチェック
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");

        // コンポーネントのshared_ptrを作成し、コンストラクタに引数を転送
        std::shared_ptr<T> component = std::make_shared<T>(std::forward<Args>(args)...);
        m_Components.push_back(component); // 内部のコンポーネントリストに追加
        component->Initialize();           // コンポーネントのInitializeメソッドを呼び出す
        return component;                  // 追加されたコンポーネントのshared_ptrを返す
    }

    // 特定の型のコンポーネントを取得するテンプレート関数
    // @typename T: 取得したいコンポーネントの型
    // @return: 指定された型のコンポーネントのshared_ptr。見つからない場合はnullptr
    template<typename T>
    std::shared_ptr<T> GetComponent()
    {
        // 静的アサート: T が Component の基底クラスであるかをコンパイル時にチェック
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");

        // すべてのコンポーネントをイテレート
        for (const auto& comp : m_Components)
        {
            // 動的キャストを試みる: 基底クラスポインタから派生クラスポインタへの安全な変換
            std::shared_ptr<T> specificComp = std::dynamic_pointer_cast<T>(comp);
            if (specificComp) // キャストが成功した場合（つまり型が一致した場合）
            {
                return specificComp; // そのコンポーネントを返す
            }
        }
        return nullptr; // 見つからなかった場合はnullptrを返す
    }

    std::shared_ptr<Scene> GetScene() const { return m_Scene.lock(); }
    void SetScene(std::shared_ptr<Scene> scene) { m_Scene = scene; }

    // 親を安全に取得するヘルパー（Scene.cpp から使う）
    std::shared_ptr<GameObject> GetParent() const { return m_Parent.lock(); }

    // 破棄フラグつきの「生存判定」
    explicit operator bool() const {
        return !m_Destroyed;
    }

    bool IsDestroyed() const { return m_Destroyed; }

    // Destroy 予約されたら Scene から即フラグを立てる
    void MarkAsDestroyed() { m_Destroyed = true; }

    // GameObjectが破棄されるときに呼ぶ関数
    void Destroy();

    // GameObjectの更新ロジック
    // シーンの更新ループから呼ばれ、自身と子オブジェクト、アタッチされたコンポーネントを更新する
    // @param deltaTime: 前のフレームからの経過時間
    void Update(float deltaTime);

    // 子オブジェクトの管理
    // @param child: 追加する子GameObjectのshared_ptr
    void AddChild(std::shared_ptr<GameObject> child);
    // @param child: 削除する子GameObjectのshared_ptr
    void RemoveChild(std::shared_ptr<GameObject> child);
    // 子オブジェクトのリストをconst参照で取得するゲッター
    const std::vector<std::shared_ptr<GameObject>>& GetChildren() const { return m_Children; }

private:
    std::vector<std::shared_ptr<Component>> m_Components; // このGameObjectにアタッチされたコンポーネントのリスト
    std::vector<std::shared_ptr<GameObject>> m_Children;  // このGameObjectの子オブジェクトのリスト
    std::weak_ptr<GameObject> m_Parent;                   // 親GameObjectへの弱い参照（循環参照を防ぐため）
    std::weak_ptr<Scene> m_Scene; // 自分が所属するシーン

    bool m_Destroyed = false;

    // Sceneクラスがm_Parentにアクセスできるようにフレンド宣言
    // SceneがGameObjectの親子関係を管理する際に必要となる
    friend class Scene;
};