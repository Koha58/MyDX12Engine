#pragma once
#include <vector>
#include <string>
#include <DirectXMath.h>
#include <memory> // std::shared_ptr
#include <type_traits> // std::is_base_of

// 前方宣言
class Component;
class MeshRendererComponent;

// =====================================================================================
// コンポーネントのベースクラス
// すべての具体的なコンポーネントはこのクラスから派生する
// =====================================================================================
class Component
{
public:
    enum ComponentType
    {
        None = 0,
        Transform,
        MeshRenderer,
        // 必要に応じて他のコンポーネントタイプを追加
        MAX_COMPONENT_TYPES // 型の数を数えるための番兵
    };

    Component(ComponentType type) : m_Type(type) {}
    virtual ~Component() = default;

    ComponentType GetType() const { return m_Type; }

    // 初期化、更新、描画など、コンポーネント固有のロジック
    virtual void Initialize() {}
    virtual void Update(float deltaTime) {}
    // 描画はレンダリングパス内で処理されることが多いので、ここではシンプルな例に留める
    virtual void Render() {}

protected:
    ComponentType m_Type;
};

// =====================================================================================
// トランスフォームコンポーネント
// Component から派生させることで、共通のインターフェースを使用できるようになる
// =====================================================================================
class TransformComponent : public Component
{
public:
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Rotation; // オイラー角 (ラジアン)
    DirectX::XMFLOAT3 Scale;

    TransformComponent()
        : Component(ComponentType::Transform), // Component のコンストラクタを呼び出す
        Position(0.0f, 0.0f, 0.0f),
        Rotation(0.0f, 0.0f, 0.0f),
        Scale(1.0f, 1.0f, 1.0f) {
    }

    // ワールド行列を計算する
    DirectX::XMMATRIX GetWorldMatrix() const
    {
        using namespace DirectX;
        XMMATRIX scaleMatrix = XMMatrixScaling(Scale.x, Scale.y, Scale.z);
        XMMATRIX rotationX = XMMatrixRotationX(Rotation.x);
        XMMATRIX rotationY = XMMatrixRotationY(Rotation.y);
        XMMATRIX rotationZ = XMMatrixRotationZ(Rotation.z);
        XMMATRIX translationMatrix = XMMatrixTranslation(Position.x, Position.y, Position.z);

        // スケール -> 回転 -> 並行移動 の順で適用
        return scaleMatrix * rotationX * rotationY * rotationZ * translationMatrix;
    }
};

// =====================================================================================
// GameObjectクラス
// シーン内のすべてのオブジェクトの基盤となるクラス
// =====================================================================================
class GameObject : public std::enable_shared_from_this<GameObject>
{
public:
    std::string Name;
    std::shared_ptr<TransformComponent> Transform; // すべてのGameObjectはTransformを持つ

    GameObject(const std::string& name = "GameObject");
    ~GameObject();

    // コンポーネントを追加するテンプレート関数
    template<typename T, typename... Args>
    std::shared_ptr<T> AddComponent(Args&&... args)
    {
        // T は Component から派生している必要がある
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");

        std::shared_ptr<T> component = std::make_shared<T>(std::forward<Args>(args)...);
        m_Components.push_back(component);
        component->Initialize(); // コンポーネントの初期化を呼び出す
        return component;
    }

    // 特定の型のコンポーネントを取得する
    template<typename T>
    std::shared_ptr<T> GetComponent()
    {
        // T は Component から派生している必要がある
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");

        for (const auto& comp : m_Components)
        {
            // 動的キャストを使用して型をチェック
            std::shared_ptr<T> specificComp = std::dynamic_pointer_cast<T>(comp);
            if (specificComp)
            {
                return specificComp;
            }
        }
        return nullptr;
    }

    // シーンの更新ループから呼ばれる
    void Update(float deltaTime);

    // 子オブジェクトの管理
    void AddChild(std::shared_ptr<GameObject> child);
    void RemoveChild(std::shared_ptr<GameObject> child);
    const std::vector<std::shared_ptr<GameObject>>& GetChildren() const { return m_Children; }

private:
    std::vector<std::shared_ptr<Component>> m_Components;
    std::vector<std::shared_ptr<GameObject>> m_Children;
    std::weak_ptr<GameObject> m_Parent; // 親への参照（循環参照を防ぐためweak_ptr）
};

// =====================================================================================
// シーンクラス
// 複数のGameObjectを管理し、シーン全体の更新を行う
// =====================================================================================
class Scene
{
public:
    Scene(const std::string& name = "New Scene");
    ~Scene() = default;

    void AddGameObject(std::shared_ptr<GameObject> gameObject);
    void RemoveGameObject(std::shared_ptr<GameObject> gameObject);

    void Update(float deltaTime);

    const std::vector<std::shared_ptr<GameObject>>& GetRootGameObjects() const { return m_RootGameObjects; }

private:
    std::string m_Name;
    std::vector<std::shared_ptr<GameObject>> m_RootGameObjects; // ルート階層のGameObject
};