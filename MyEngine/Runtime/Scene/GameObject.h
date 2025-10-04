#pragma once // 多重インクルード防止

#include <vector>        // コンポーネント/子オブジェクトの保持
#include <string>        // 名前
#include <DirectXMath.h> // Transform で行列/ベクトルを使う
#include <memory>        // shared_ptr, weak_ptr, enable_shared_from_this
#include <type_traits>   // is_base_of（テンプレート制約）

#include "Components/TransformComponent.h" // 必須コンポーネント（付与は Create() 側で行う）
#include "Components/Component.h"          // コンポーネント基底

// 前方宣言（完全定義は不要だが、参照/ポインタとして使うため）
class Component;
class MeshRendererComponent; // Mesh.h で定義
class Scene;
class D3D12Renderer;

// ============================================================================
// GameObject
//  - シーンツリーの基本ノード。「コンポーネントの集合体」として振る舞う。
//  - Transform は必ず保持（空間上の位置/回転/スケールの根）。
//  - 親子関係（階層）と activeSelf / ActiveInHierarchy（実効有効）/ 破棄を管理。
//  - ライフサイクル方針（B案で統一）
//      * AddComponent: SetOwner → Awake を即時、ActiveInHierarchy かつ enabled なら OnEnable も即時
//      * Start: 「最初の Update の直前」に 1 回だけ（GameObject::Update 内で HasStarted を見て実行）
//      * OnEnable/OnDisable: **ActiveInHierarchy の変化**時のみ発火（子の activeSelf は書き換えない）
//  - 重要：shared_from_this() を使うため、**必ず shared_ptr 管理下で生成**すること。
//    → 生ポインタ new ではなく GameObject::Create() を使う。
// ============================================================================
class GameObject : public std::enable_shared_from_this<GameObject>
{
public:
    // ------------------------------------------------------------------------
    // Create
    //  - 推奨生成手段。shared_ptr 管理下でインスタンス化し、**Transform を安全に付与**する。
    //  - 実装（.cpp）側で：make_shared → AddComponent<TransformComponent>() → Transform に保持、など。
    //  - これにより ctor 内で shared_from_this() を使う必要がなく、bad_weak_ptr を回避できる。
    // ------------------------------------------------------------------------
    static std::shared_ptr<GameObject> Create(const std::string& name = "GameObject");

    // コンストラクタ
    //  - ここでは **Transform を作らない**（shared_from_this() が未使用のため）。
    //  - Transform の付与は Create() 内で安全に実施する。
    GameObject(const std::string& name = "GameObject");
    ~GameObject();

    // 公開フィールド（用途が明確で頻繁に触るため公開）
    std::string Name;                                   // デバッグ/識別用
    std::shared_ptr<TransformComponent> Transform;      // 必須。Create() 側で AddComponent して割当て

    // ============================== コンポーネント管理 ==============================
    // AddComponent
    //  - 任意の Component 派生を追加。
    //  - Awake を即時呼び出し、**その時点で ActiveInHierarchy かつコンポーネントが Enabled**
    //    なら OnEnable も即時呼ぶ（= 追加直後に動き出す作法）。
    //  - 注意：本体で shared_from_this() を使うため、**Create() で生成した後**に呼ぶこと。
    template<typename T, typename... Args>
    std::shared_ptr<T> AddComponent(Args&&... args)
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");

        // 生成 & 所有リストへ追加
        auto component = std::make_shared<T>(std::forward<Args>(args)...);
        m_Components.push_back(component);

        // Owner 同期（コンポーネントから GameObject にアクセスできるようにする）
        component->SetOwner(shared_from_this()); // ※ SetOwner(shared_ptr<GameObject>) を仮定

        // ライフサイクル（B案）
        component->Awake(); // 依存の薄い初期化はここ

        // Unity 準拠：ActiveInHierarchy && comp.enabled のときだけ OnEnable
        const bool activeInHierarchy = IsActive();
        if (activeInHierarchy && component->IsEnabled()) {
            component->OnEnable();
        }
        return component;
    }

    // GetComponent（最初に見つかった 1 件を返す）
    template<typename T>
    std::shared_ptr<T> GetComponent()
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");
        for (const auto& comp : m_Components)
        {
            auto casted = std::dynamic_pointer_cast<T>(comp);
            if (casted) return casted;
        }
        return nullptr;
    }

    // ================================ シーン参照 ================================
    std::shared_ptr<Scene> GetScene() const { return m_Scene.lock(); }
    void SetScene(std::shared_ptr<Scene> scene) { m_Scene = scene; }

    // 親（安全な shared_ptr で取得）
    std::shared_ptr<GameObject> GetParent() const { return m_Parent.lock(); }

    // ================================ 破棄/生存 ================================
    explicit operator bool() const { return !m_Destroyed; }
    bool IsDestroyed() const { return m_Destroyed; }
    void MarkAsDestroyed() { m_Destroyed = true; } // 遅延破棄キュー連携用（Scene 側が利用）
    void Destroy(); // OnDestroy の発火/子の再帰破棄など（実体は .cpp）

    // ================================ 有効/無効 ================================
    // SetActive(activeSelf の切替)
    //  - 子の **activeSelf は変更しない**（Unity と同様）。
    //  - 実効状態（ActiveInHierarchy）が変化したノードに対してのみ OnEnable/OnDisable を発火。
    //  - 実装では「直前の ActiveInHierarchy（m_LastActiveInHierarchy）」と比較して差分を検出。
    void SetActive(bool active);

    // IsActive
    //  - ActiveInHierarchy を返す（= 自分の activeSelf && 親の IsActive()）。
    //  - 親が無効なら自分も無効扱い。
    bool IsActive() const;

    // ================================ 描画/更新 ================================
    // Render: 自分の描画系コンポーネント → 子の Render を再帰呼び出し
    void Render(class D3D12Renderer* renderer);
    // Update: 子の Update → 自分のコンポーネント Update/LateUpdate（Start は最初の Update 前に 1 回）
    void Update(float deltaTime);

    // ================================ 階層操作 ================================
    // AddChild: 既存親からのデタッチ → 自分にアタッチ
    void AddChild(std::shared_ptr<GameObject> child);
    // RemoveChild: 自分の子リストから除外（Scene 側ポリシーでルート戻し等）
    void RemoveChild(std::shared_ptr<GameObject> child);
    // 子の一覧（読み取り専用）
    const std::vector<std::shared_ptr<GameObject>>& GetChildren() const { return m_Children; }

private:
    // ===== 内部状態 =====
    std::vector<std::shared_ptr<Component>> m_Components; // アタッチ済みコンポーネント
    std::vector<std::shared_ptr<GameObject>> m_Children;  // 子 GameObject
    std::weak_ptr<GameObject> m_Parent;                   // 親（循環参照回避のため weak）
    std::weak_ptr<Scene>      m_Scene;                    // 所属シーン（Scene が所有）

    bool m_Destroyed = false; // 破棄予約/破棄済み
    bool m_Active = true;  // activeSelf（自分自身の ON/OFF）。デフォルト有効。

    // 直近の ActiveInHierarchy をキャッシュして差分検出（OnEnable/OnDisable を正しく発火）
    bool m_LastActiveInHierarchy = true;

    // ===== ActiveInHierarchy 差分伝播ヘルパー =====
    // ComputeActiveInHierarchy: 自分の activeSelf と親の IsActive() から実効状態を算出
    bool ComputeActiveInHierarchy() const;
    // ApplyActiveInHierarchyDelta: 差分があれば OnEnable/OnDisable を発火
    void ApplyActiveInHierarchyDelta(bool wasActiveInHierarchy, bool nowActiveInHierarchy);
    // RefreshActiveInHierarchyRecursive: 自分の差分処理後、子へ「親の状態」を渡しながら再帰適用
    void RefreshActiveInHierarchyRecursive(bool prevOfThis);

    // Scene が親子や Scene 参照を直接操作できるように
    friend class Scene;
};

// ================================ 設計メモ ================================
// ・生成は Create() を使う：shared_ptr 管理下で Transform を付与 → ctor で shared_from_this() しない
//   → std::bad_weak_ptr を確実に回避。
// ・AddComponent の Awake/OnEnable タイミングは B案に統一（Scene 側では Awake を呼ばない）。
// ・Start は GameObject::Update() で HasStarted を見て 1 回だけ呼ぶ実装に揃えること。
// ・SetActive は activeSelf だけを変える。子の activeSelf は弄らない。
//   → 実効状態 ActiveInHierarchy の変化を差分検出して OnEnable/OnDisable を正しく発火。
// ・GetComponent は O(N)。ホットパスはハンドル/キャッシュ/型IDインデックス化も検討。
// ・スレッドセーフではない（更新/描画/階層変更はメインスレッド前提）。
// ============================================================================
