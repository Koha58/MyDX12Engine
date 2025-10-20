#pragma once // 多重インクルード防止

#include <vector>        // コンポーネント/子オブジェクトの保持
#include <string>        // 名前
#include <memory>        // shared_ptr, weak_ptr, enable_shared_from_this
#include <type_traits>   // is_base_of（テンプレート制約）

#include "Components/TransformComponent.h" // 必須コンポーネント（付与は Create() 側で行う）
#include "Components/Component.h"          // コンポーネント基底

// 前方宣言（完全定義は不要だが、参照/ポインタとして使うため）
class Component;
class MeshRendererComponent; // MeshRendererComponent は別ヘッダで定義
class Scene;
class D3D12Renderer;

// ============================================================================
// GameObject
// ----------------------------------------------------------------------------
// ・シーンツリーの基本ノード。「コンポーネントの集合体」として振る舞う。
// ・Transform は必ず保持（空間上の位置/回転/スケールの根）。
// ・親子関係（階層）と activeSelf / ActiveInHierarchy（実効有効）/ 破棄を管理。
// ・ライフサイクル方針（B案で統一）
//    - AddComponent: SetOwner → Awake を即時、ActiveInHierarchy かつ enabled なら OnEnable も即時
//    - Start: 「最初の Update の直前」に 1 回だけ（GameObject::Update 内で HasStarted を見て実行）
//    - OnEnable/OnDisable: **ActiveInHierarchy の変化**時のみ発火（子の activeSelf は書き換えない）
// ・shared_from_this() を使うため、**必ず shared_ptr 管理下で生成**すること。
//   → new 直呼びではなく GameObject::Create() を使う。
// ============================================================================
class GameObject : public std::enable_shared_from_this<GameObject>
{
public:
    //--------------------------------------------------------------------------
    // Create
    //  推奨生成手段。shared_ptr 管理下でインスタンス化し、**Transform を安全に付与**する。
    //  実装（.cpp）側で：make_shared → AddComponent<TransformComponent>() → Transform に保持。
    //  これにより ctor 内で shared_from_this() を使う必要がなく、bad_weak_ptr を回避できる。
    //--------------------------------------------------------------------------
    static std::shared_ptr<GameObject> Create(const std::string& name = "GameObject");

    // コンストラクタ
    //  - ここでは **Transform を作らない**（Create() 側で安全に付与するため）。
    explicit GameObject(const std::string& name = "GameObject");
    ~GameObject();

    // 公開フィールド（用途が明確で頻繁に触るため公開）
    std::string Name;                                   // デバッグ/識別用
    std::shared_ptr<TransformComponent> Transform;      // 必須。Create() 側で AddComponent して割当て

    // ============================== コンポーネント管理 ==============================
    /**
     * @brief 任意の Component 派生を追加する。
     * @details
     *  1) コンポーネントを生成して所有リストに追加
     *  2) Owner を自分(shared_from_this())に設定
     *  3) Awake() を即時呼び出し
     *  4) 追加時点で ActiveInHierarchy && comp.enabled なら OnEnable() を即時呼ぶ
     * @note Create() で生成した「shared_ptr 管理下」で呼ぶこと（shared_from_this 安全化）。
     */
    template<typename T, typename... Args>
    std::shared_ptr<T> AddComponent(Args&&... args)
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");

        // 1) 生成 & 所有リストへ追加
        auto component = std::make_shared<T>(std::forward<Args>(args)...);
        m_Components.push_back(component);

        // 2) Owner 同期（コンポーネントから GameObject にアクセスできるようにする）
        component->SetOwner(shared_from_this()); // ※ Component 側に SetOwner(shared_ptr<GameObject>) がある前提

        // 3) Awake（依存の薄い初期化はここで完了させる）
        component->Awake();

        // 4) 追加直後から動作させたい場合、実効的に有効なら OnEnable を即時発火
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
            if (auto casted = std::dynamic_pointer_cast<T>(comp)) {
                return casted;
            }
        }
        return nullptr;
    }

    // ================================ シーン参照 ================================
    std::shared_ptr<Scene> GetScene() const { return m_Scene.lock(); }
    void SetScene(std::shared_ptr<Scene> scene) { m_Scene = std::move(scene); }

    // 親（安全な shared_ptr で取得）
    std::shared_ptr<GameObject> GetParent() const { return m_Parent.lock(); }

    // ================================ 破棄/生存 ================================
    explicit operator bool() const { return !m_Destroyed; }
    bool IsDestroyed() const { return m_Destroyed; }

    // Destroy は「予約→フレーム終端実行」運用に合わせるため、まずフラグだけ立てる。
    void MarkAsDestroyed() { m_Destroyed = true; }

    // 実体破棄（OnDestroy 発火/子の再帰破棄など）。呼び出し側（Scene）が順序を統一管理。
    void Destroy();

    // ================================ 有効/無効 ================================
    /**
     * @brief activeSelf の切替
     * @details
     *  - 子の **activeSelf は変更しない**（Unity と同様）
     *  - ActiveInHierarchy の変化を差分検出して OnEnable/OnDisable を発火
     *  - Scene 管理（ルート配列登録/除外）との同期は .cpp 実装側で行う
     */
    void SetActive(bool active);

    /**
     * @brief ActiveInHierarchy を返す
     * @details 自分の activeSelf が false なら false。親がいれば親の IsActive() に従う。
     */
    bool IsActive() const;

    // ================================ 描画/更新 ================================
    // Render: 自分の描画系コンポーネント → 子の Render を再帰呼び出し
    void Render(class D3D12Renderer* renderer);

    // Update: 子の Update → 自分のコンポーネント Update/LateUpdate
    //         Start は「最初の Update 前」に 1 回だけ（HasStarted フラグで管理）
    void Update(float deltaTime);

    // ================================ 階層操作 ================================
    /**
     * @brief 子を追加
     * @details 既存の親があればそちらからデタッチしてから自分へアタッチする。
     *          ActiveInHierarchy の差分適用を正しく伝播する。
     */
    void AddChild(std::shared_ptr<GameObject> child);

    /**
     * @brief 子を外す
     * @details 自分の子配列から除外。Scene 側ポリシーにより「ルートへ戻す」等の運用も可能。
     *          ActiveInHierarchy の差分適用を正しく伝播する。
     */
    void RemoveChild(std::shared_ptr<GameObject> child);

    // 子の一覧（読み取り専用）
    const std::vector<std::shared_ptr<GameObject>>& GetChildren() const { return m_Children; }

private:
    // ===== 所有コンテナ =====
    std::vector<std::shared_ptr<Component>>  m_Components; // アタッチ済みコンポーネント
    std::vector<std::shared_ptr<GameObject>> m_Children;   // 子 GameObject

    // ===== 関連参照（循環参照回避のため weak）=====
    std::weak_ptr<GameObject> m_Parent; // 親
    std::weak_ptr<Scene>      m_Scene;  // 所属シーン（Scene が所有）

    // ===== 状態フラグ =====
    bool m_Destroyed = false;  // 破棄予約/破棄済み
    bool m_Active = true;   // activeSelf（自分自身の ON/OFF）。デフォルト有効。

    // 直近の ActiveInHierarchy をキャッシュして差分検出（OnEnable/OnDisable を正しく発火）
    bool m_LastActiveInHierarchy = true;

    // ===== ActiveInHierarchy 差分伝播ヘルパー =====
    // 自分の activeSelf と親の IsActive() から実効状態を算出
    bool ComputeActiveInHierarchy() const;

    // 差分があれば OnEnable/OnDisable を発火し、キャッシュを更新
    void ApplyActiveInHierarchyDelta(bool wasActiveInHierarchy, bool nowActiveInHierarchy);

    // 自分の差分処理後、子へ「それぞれの直前状態」を渡しながら再帰適用
    void RefreshActiveInHierarchyRecursive(bool prevOfThis);

    // Scene が親子や Scene 参照を直接操作できるように
    friend class Scene;
};

// ================================ 設計メモ ================================
// ・生成は Create() を使う：shared_ptr 管理下で Transform を付与 → ctor で shared_from_this() しない
//   → std::bad_weak_ptr を確実に回避。
// ・AddComponent の Awake/OnEnable タイミングは B案に統一（Scene 側では Awake を呼ばない）。
// ・Start は GameObject::Update() で HasStarted を見て 1 回だけ呼ぶ実装に揃えること。
// ・SetActive は activeSelf のみ変更。子の activeSelf は弄らない。
//   → 実効状態 ActiveInHierarchy の変化を差分検出して OnEnable/OnDisable を正しく発火。
// ・GetComponent は O(N)。ホットパスはハンドル/キャッシュ/型IDインデックス化も検討。
// ・スレッドセーフではない（更新/描画/階層変更はメインスレッド前提）。
// ============================================================================
