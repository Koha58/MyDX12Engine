#pragma once // 同じヘッダーファイルが複数回インクルードされるのを防ぐプリプロセッサディレクティブ

#include <vector>        // std::vectorを使用
#include <string>        // std::stringを使用
#include <DirectXMath.h> // DirectX::XMFLOAT3, DirectX::XMMATRIX などを使用
#include <memory>        // std::shared_ptr, std::weak_ptr, std::enable_shared_from_this を使用
#include <type_traits>   // std::is_base_of を使用（テンプレートメタプログラミング）

// 前方宣言:
// これらのクラスの完全な定義はここでは不要だが、ポインタや参照型として使用するために宣言
class Component;
class MeshRendererComponent; // Mesh.h で定義される

// --- Componentのベースクラス ---
// すべての具体的なコンポーネントはこのクラスから派生する
// コンポーネントはGameObjectにアタッチされ、特定の機能を提供する
class Component
{
public:
    // コンポーネントのタイプを識別するための列挙型
    enum ComponentType
    {
        None = 0,        // 未指定またはデフォルト
        Transform,       // 位置、回転、スケールを管理するコンポーネント
        MeshRenderer,    // メッシュの描画を担当するコンポーネント
        // 必要に応じて他のコンポーネントタイプを追加する
        MAX_COMPONENT_TYPES // 型の数を数えるための番兵値
    };

    // コンストラクタ: コンポーネントタイプを受け取る
    Component(ComponentType type) : m_Type(type) {}

    // デストラクタ: ポリモーフィックな削除を可能にするために仮想デストラクタとする
    virtual ~Component() = default;

    // コンポーネントのタイプを取得するゲッター
    ComponentType GetType() const { return m_Type; }

    // 仮想関数: 各コンポーネント固有のロジックを実装するためにオーバーライドされる
    virtual void Initialize() {} // コンポーネントがアタッチされた後の初期化処理
    virtual void Update(float deltaTime) {} // フレームごとの更新処理
    virtual void Render() {} // 描画処理 (通常はレンダリングパス内で処理されることが多い)

protected:
    ComponentType m_Type; // このコンポーネントのタイプ
};

// --- TransformComponentクラス ---
// Componentから派生し、GameObjectの位置、回転、スケールを管理する
class TransformComponent : public Component
{
public:
    DirectX::XMFLOAT3 Position; // オブジェクトの3D空間における位置
    DirectX::XMFLOAT3 Rotation; // オブジェクトのオイラー角による回転 (ラジアン単位)
    DirectX::XMFLOAT3 Scale;    // オブジェクトの3軸方向へのスケール

    // コンストラクタ
    TransformComponent()
        : Component(ComponentType::Transform), // Component基底クラスのコンストラクタを呼び出す
        Position(0.0f, 0.0f, 0.0f),            // 位置を原点で初期化
        Rotation(0.0f, 0.0f, 0.0f),            // 回転を初期化 (無回転)
        Scale(1.0f, 1.0f, 1.0f) {              // スケールを1.0で初期化
    }

    // ワールド行列を計算する関数
    // 位置、回転、スケールからモデルのワールド変換行列を生成する
    DirectX::XMMATRIX GetWorldMatrix() const
    {
        using namespace DirectX; // DirectXMath名前空間をusing宣言
        // スケール行列の生成
        XMMATRIX scaleMatrix = XMMatrixScaling(Scale.x, Scale.y, Scale.z);
        // 各軸周りの回転行列の生成 (Z -> Y -> X の順で乗算されるよう準備)
        XMMATRIX rotationX = XMMatrixRotationX(Rotation.x);
        XMMATRIX rotationY = XMMatrixRotationY(Rotation.y);
        XMMATRIX rotationZ = XMMatrixRotationZ(Rotation.z);
        // 平行移動行列の生成
        XMMATRIX translationMatrix = XMMatrixTranslation(Position.x, Position.y, Position.z);

        // 一般的な変換順序: スケール -> 回転 -> 平行移動
        // 行ベクトルを使う場合、乗算順序は (S * R * T) となる
        return scaleMatrix * rotationX * rotationY * rotationZ * translationMatrix;
    }
};

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

    // Sceneクラスがm_Parentにアクセスできるようにフレンド宣言
    // SceneがGameObjectの親子関係を管理する際に必要となる
    friend class Scene;
};

// --- Sceneクラス ---
// 複数のGameObjectを管理し、シーン全体の更新を行う
// ゲームの論理的な区切り（例: レベル、メニュー画面など）を表す
class Scene
{
public:
    // コンストラクタ
    // @param name: シーンの名前 (デフォルトは"New Scene")
    Scene(const std::string& name = "New Scene");

    // デストラクタ
    ~Scene() = default;

    // シーンにルートGameObjectを追加する
    // @param gameObject: 追加するGameObjectのshared_ptr
    void AddGameObject(std::shared_ptr<GameObject> gameObject);
    // シーンからルートGameObjectを削除する
    // @param gameObject: 削除するGameObjectのshared_ptr
    void RemoveGameObject(std::shared_ptr<GameObject> gameObject);

    // シーン内のすべてのルートGameObjectを更新する
    // @param deltaTime: 前のフレームからの経過時間
    void Update(float deltaTime);

    // シーンのルートGameObjectリストをconst参照で取得するゲッター
    const std::vector<std::shared_ptr<GameObject>>& GetRootGameObjects() const { return m_RootGameObjects; }

private:
    std::string m_Name;                                    // シーンの名前
    std::vector<std::shared_ptr<GameObject>> m_RootGameObjects; // シーンの最上位階層にあるGameObjectのリスト
};