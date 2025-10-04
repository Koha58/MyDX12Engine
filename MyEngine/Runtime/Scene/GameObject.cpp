#include "GameObject.h" // GameObjectクラスの定義
#include "Scene.h"
#include "Components/Component.h"
#include "Components/TransformComponent.h"
#include <algorithm>    // std::remove を使用するためにインクルード

// ============================================================================
// GameObject
//  - 階層（親子）構造を持つ「シーン内のエンティティ」
//  - 最低限の Transform（位置/回転/スケール）と、任意数の Component を保持
//  - ライフサイクル（Unity準拠寄りの方針Bで統一）:
//      * AddComponent で Awake を即時、（ActiveInHierarchyなら）OnEnable を即時
//      * Start は「最初の Update の直前」に呼ぶ（遅延）
//      * OnEnable/OnDisable は ActiveInHierarchy の変化時のみ発火
//  - Scene との関係:
//      * Scene 側は「ルート GameObject の配列」を持つ想定
//      * 子を親から外した場合は、Scene のルートへ戻す仕様（到達可能性を維持）
//  - 共有所有: GameObject 自身は shared_ptr で扱い、親は weak_ptr で参照（循環参照を避ける）
// ============================================================================

// ---- 内部ヘルパー（読みやすさのための小道具） --------------------------------
namespace {
    // remove-erase を読みやすくまとめたヘルパ
    template<class T, class U>
    void erase_remove(T& v, const U& value) {
        v.erase(std::remove(v.begin(), v.end(), value), v.end());
    }
}

// [CHANGED] ファクトリ実装：make_shared 化（例外安全＆軽量アロケーション）
//  - 必ず shared_ptr 管理下で生成 → 直後に Transform を安全に付与
//  - ctor 内では Transform を作らず、ここで AddComponent する（shared_from_this が安全になる）
std::shared_ptr<GameObject> GameObject::Create(const std::string& name)
{
    auto go = std::make_shared<GameObject>(name);          // ← make_shared に変更
    go->Transform = go->AddComponent<TransformComponent>(); // 必須コンポーネントを即付与
    return go;
}

// ----------------------------------------------------------------------------
// コンストラクタ
// @param name : GameObject に与えられる名前（デバッグ/検索用）
//  - Transform は **ここでは作らない**（Create() 内で安全に付与するため）。
// ----------------------------------------------------------------------------
GameObject::GameObject(const std::string& name)
    : Name(name)
{
    // [NOTE] 以前はここで Transform を Add していたが、ctor 中に shared_from_this() は使えないため
    //        Create() ファクトリへ移動した（std::bad_weak_ptr 回避）。

    // 生成直後の ActiveInHierarchy をキャッシュ（親なし＆self有効なので true）
    m_LastActiveInHierarchy = IsActive();
}

// ----------------------------------------------------------------------------
// デストラクタ
//  - 破棄時に「子オブジェクトの親参照（weak_ptr）」をクリア。
//    ※ 実体の破棄（OnDestroy 呼び出し等）は Destroy() で実施。
// ----------------------------------------------------------------------------
GameObject::~GameObject()
{
    for (const auto& child : m_Children) {
        child->m_Parent.reset();
    }
}

// ----------------------------------------------------------------------------
// Update
//  - Scene の更新ループから毎フレーム呼ばれる。
//  - 処理順:
//     1) 早期 return（破棄済み/非アクティブ階層）
//     2) 子オブジェクトの Update（親→子の順、Transform 依存に自然）
//     3) 自身の全コンポーネント Update（Start は初回だけ、Enabled のみ）
//     4) 自身の全コンポーネント LateUpdate（Enabled のみ）
// ----------------------------------------------------------------------------
void GameObject::Update(float deltaTime)
{
    // 実効的に非アクティブ（= ActiveInHierarchy false）ならスキップ
    if (m_Destroyed || !IsActive()) return;

    // --- 子オブジェクトの更新（親→子） ---
    for (auto& child : m_Children) {
        child->Update(deltaTime);
    }

    // --- コンポーネントの更新 ---
    for (auto& comp : m_Components) {
        if (!comp) continue;

        // [Unityライク] 有効時に限り Start を初回だけ呼ぶ
        if (!comp->HasStarted() && comp->IsEnabled() && IsActive()) {
            comp->Start();
            comp->MarkStarted();
        }

        if (comp->IsEnabled() && IsActive()) {
            comp->Update(deltaTime);
        }
    }

    // --- LateUpdate ---
    for (auto& comp : m_Components) {
        if (comp && comp->IsEnabled()) {
            comp->LateUpdate(deltaTime);
        }
    }
}

// ----------------------------------------------------------------------------
// AddChild
//  - 既に他の親があるなら取り外してから自分に付け替える（孤児化の防止）。
//  - 親子関係の変更は ActiveInHierarchy に影響するため、**差分適用**で
//    OnEnable/OnDisable を正しく発火させる。
// [NOTE] 直前に RemoveChild が「ルートへ戻す + 差分適用」も行うため、
//        「付け替え時に一瞬ルートへ行ってから再度付与」という二段階の変化が起きうる。
//        これを 1 回の変化に抑えたい場合は、Reparent 専用 API を用意して
//        まとめて差分適用するのがベター。
// ----------------------------------------------------------------------------
void GameObject::AddChild(std::shared_ptr<GameObject> child)
{
    const bool prevChildHier = child->IsActive(); // 追加前の子の実効状態を保持

    // 既存の親から取り外す（※ RemoveChild は“ルート戻し + 差分適用”を行う設計）
    if (auto oldParent = child->m_Parent.lock()) {
        oldParent->RemoveChild(child);
    }

    // 自分の子に登録し、親参照を張る
    m_Children.push_back(child);
    child->m_Parent = shared_from_this();

    // Transform の親子関係を同期したい場合はここで
    // 例: child->Transform->SetParent(Transform.get());

    // 新しい親のもとで ActiveInHierarchy が変化したら差分を適用
    child->RefreshActiveInHierarchyRecursive(prevChildHier);
}

// ----------------------------------------------------------------------------
// RemoveChild
//  - 自分の子配列から除外し、親参照をクリア。
//  - 「親から外れたら Scene のルートへ戻す」ポリシーで管理下に残す。
//  - その結果 ActiveInHierarchy が変わる場合は差分適用。
// [NOTE] Reparent（付け替え）シナリオでは AddChild 側でも差分適用するため、
//       状態変化が 2 回連続で起きうる点に注意（必要なら Reparent API を検討）。
// ----------------------------------------------------------------------------
void GameObject::RemoveChild(std::shared_ptr<GameObject> child)
{
    const bool prevChildHier = child->IsActive();

    erase_remove(m_Children, child);
    child->m_Parent.reset();

    // ルートへ戻す（Scene 管理から到達可能に）
    if (auto scene = child->m_Scene.lock()) {
        scene->AddGameObject(child, nullptr); // 親 nullptr = ルート登録
    }

    // ActiveInHierarchy の差分適用
    child->RefreshActiveInHierarchyRecursive(prevChildHier);
}

// ----------------------------------------------------------------------------
/* SetActive（activeSelf の切替）
   重要事項：
   - 子の activeSelf は変更しない（Unity と同様）。親の ON/OFF により “実効状態” だけ変化。
   - OnEnable/OnDisable は ActiveInHierarchy が変化した時のみ発火。
   - Scene 管理（ルート配列）への登録/除外も同期する設計。
     [NOTE] Unity では非アクティブでも Scene には残るが、本エンジンは
           「非アクティブ → 巡回対象から外す」方針。プロジェクト方針に合わせて統一を。
*/
void GameObject::SetActive(bool active)
{
    const bool prevHier = IsActive(); // 変更前の実効状態

    if (m_Active == active) {
        // self に変化なし：Scene 側の管理だけ軽く同期
        if (auto scene = m_Scene.lock()) {
            // [NOTE] 要：Scene::ContainsRootGameObject 実装
            if (m_Active) {
                if (!scene->ContainsRootGameObject(shared_from_this())) {
                    scene->AddGameObject(shared_from_this());
                }
            }
            else {
                scene->RemoveGameObject(shared_from_this());
            }
        }
        return;
    }

    // self を切替
    m_Active = active;

    // Scene 管理へ反映
    if (auto scene = m_Scene.lock()) {
        if (m_Active) {
            if (!scene->ContainsRootGameObject(shared_from_this())) {
                scene->AddGameObject(shared_from_this());
            }
        }
        else {
            scene->RemoveGameObject(shared_from_this());
        }
    }

    // 実効状態の差分を自分→子へ伝播（OnEnable/OnDisable を正しく発火）
    RefreshActiveInHierarchyRecursive(prevHier);
}

// ----------------------------------------------------------------------------
// IsActive（ActiveInHierarchy 相当）
//  - 自身の activeSelf が false なら false
//  - 親があれば親の ActiveInHierarchy に従う（親が無効なら自分も無効）
// ----------------------------------------------------------------------------
bool GameObject::IsActive() const
{
    if (!m_Active) return false;

    if (auto parent = m_Parent.lock()) {
        return parent->IsActive();
    }
    return true; // ルート
}

// ----------------------------------------------------------------------------
// Destroy
//  - 冪等。OnDisable（有効だったコンポーネントのみ）→ OnDestroy → 子も再帰 Destroy。
//  - Scene/Parent からの切り離しは Scene 側の破棄フローで実施（順序を一元管理）。
// ----------------------------------------------------------------------------
void GameObject::Destroy()
{
    if (m_Destroyed) return;
    m_Destroyed = true;

    // 有効だったコンポーネントは先に OnDisable
    if (IsActive()) {
        for (auto& comp : m_Components) {
            if (comp && comp->IsEnabled()) {
                comp->OnDisable();
            }
        }
    }

    // その後 OnDestroy
    for (auto& comp : m_Components) {
        if (comp) comp->OnDestroy();
    }
    m_Components.clear();

    // 子も同様に再帰 Destroy
    for (auto& child : m_Children) {
        child->Destroy();
    }
    m_Children.clear();
}

// ----------------------------------------------------------------------------
// Render
//  - 実効的に非アクティブなら描画しない。
//  - 実描画は各描画系コンポーネントが Renderer に対して行う。
// ----------------------------------------------------------------------------
void GameObject::Render(D3D12Renderer* renderer)
{
    if (!IsActive()) return;

    for (auto& comp : m_Components) {
        if (comp) comp->Render(renderer);
    }
    for (auto& child : m_Children) {
        if (child) child->Render(renderer);
    }
}

// ============================ ここから内部ヘルパー ===========================

// 現在の ActiveInHierarchy を計算して返す（IsActive と同義だが意図が伝わる命名）
bool GameObject::ComputeActiveInHierarchy() const
{
    return IsActive();
}

// ActiveInHierarchy の変化を自分に適用（OnEnable/OnDisable の発火＆キャッシュ更新）
void GameObject::ApplyActiveInHierarchyDelta(bool wasActiveInHierarchy, bool nowActiveInHierarchy)
{
    if (wasActiveInHierarchy == nowActiveInHierarchy) {
        return; // 変化なし
    }

    if (nowActiveInHierarchy) {
        for (auto& comp : m_Components) {
            if (comp && comp->IsEnabled()) comp->OnEnable();
        }
    }
    else {
        for (auto& comp : m_Components) {
            if (comp && comp->IsEnabled()) comp->OnDisable();
        }
    }

    m_LastActiveInHierarchy = nowActiveInHierarchy;
}

// 自分と配下（ツリー全体）について、ActiveInHierarchy の差分を適用
// @param prevOfThis : 呼び出し直前における「このノードの」ActiveInHierarchy
void GameObject::RefreshActiveInHierarchyRecursive(bool prevOfThis)
{
    const bool now = ComputeActiveInHierarchy();
    ApplyActiveInHierarchyDelta(prevOfThis, now);

    // 子へは「それぞれの直前状態（キャッシュ）」を prev として渡す
    for (auto& ch : m_Children) {
        if (!ch) continue;
        const bool childPrev = ch->m_LastActiveInHierarchy;
        ch->RefreshActiveInHierarchyRecursive(childPrev);
    }
}
