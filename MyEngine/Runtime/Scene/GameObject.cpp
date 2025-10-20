// GameObject.cpp
//------------------------------------------------------------------------------
// 役割：シーン内のエンティティ（ノード）を表す GameObject の実装。
//       - 親子（ツリー）構造の管理（AddChild / RemoveChild）
//       - Active（自己）と ActiveInHierarchy（実効）の扱い
//       - Component 群のライフサイクル (Awake/OnEnable/Start/Update/LateUpdate/OnDisable/OnDestroy)
//       - 破棄フロー（Destroy）
// 設計メモ：
//   * GameObject は shared_ptr 管理（親: weak_ptr）で循環参照を回避
//   * TransformComponent は必須。Create() ファクトリで安全に付与（ctor では付与しない）
//   * ライフサイクルは「方針B」：AddComponent → 即 Awake、ActiveInHierarchy なら即 OnEnable。
//     Start は「最初の Update 直前」に遅延実行
//   * ActiveInHierarchy（= 自己が有効 && 親が ActiveInHierarchy）変化時のみ OnEnable/OnDisable を発火
//   * RemoveChild した子は Scene のルートへ戻す（到達不能オブジェクトを作らない）
//------------------------------------------------------------------------------

#include "GameObject.h"              // GameObject クラスの定義
#include "Scene.h"                   // Scene への登録/照会
#include "Components/Component.h"    // 基底コンポーネント
#include "Components/TransformComponent.h"
#include <algorithm>                 // std::remove

// ============================================================================
// 内部ヘルパー（読みやすさのための小道具）
// ============================================================================
namespace {
    // remove-erase イディオムを読みやすくまとめる
    template<class T, class U>
    void erase_remove(T& v, const U& value) {
        v.erase(std::remove(v.begin(), v.end(), value), v.end());
    }
}

// ============================================================================
// Create（ファクトリ）
//  - 例外安全＆一括アロケーションのため make_shared を使用
//  - TransformComponent は必須なので、ここで確実に付与する
//  - ctor では shared_from_this() が使えないため、ここで AddComponent する設計
// ============================================================================
std::shared_ptr<GameObject> GameObject::Create(const std::string& name)
{
    auto go = std::make_shared<GameObject>(name);           // 共有所有に乗せる
    go->Transform = go->AddComponent<TransformComponent>(); // 必須 Transform を即付与
    return go;
}

// ============================================================================
// ctor / dtor
//  - ctor：Transform は付与しない（Create() 内で安全に付与）
//  - dtor：子の親参照（weak）を切るだけ。OnDestroy は Destroy() で行う
// ============================================================================
GameObject::GameObject(const std::string& name)
    : Name(name)
{
    // 生成直後の「実効状態（ActiveInHierarchy）」キャッシュ
    // （親なし＆自己有効が初期値なので true 相当）
    m_LastActiveInHierarchy = IsActive();
}

GameObject::~GameObject()
{
    // 子の親参照（weak_ptr）をクリア（所有は shared_ptr 側）
    for (const auto& child : m_Children) {
        child->m_Parent.reset();
    }
}

// ============================================================================
// Update（毎フレーム呼び出し）
// 処理順：
//   1) 破棄済み or 非アクティブ階層なら早期 return
//   2) 子の Update（親→子。Transform 依存が自然）
//   3) 自身のコンポーネント Update（Start は初回のみ / 有効なもののみ）
//   4) 自身のコンポーネント LateUpdate（有効なもののみ）
// ============================================================================
void GameObject::Update(float deltaTime)
{
    // 実効的に非アクティブなら何もしない
    if (m_Destroyed || !IsActive()) return;

    // --- 子オブジェクトの更新（親 → 子） ---
    for (auto& child : m_Children) {
        child->Update(deltaTime);
    }

    // --- コンポーネントの更新 ---
    for (auto& comp : m_Components) {
        if (!comp) continue;

        // Start は「有効」かつ「まだ Start 済みでない」時、最初の Update の直前に 1 回だけ
        if (!comp->HasStarted() && comp->IsEnabled() && IsActive()) {
            comp->Start();
            comp->MarkStarted();
        }

        // 毎フレームの Update（有効＆実効アクティブ）
        if (comp->IsEnabled() && IsActive()) {
            comp->Update(deltaTime);
        }
    }

    // --- LateUpdate（順序は Update 後） ---
    for (auto& comp : m_Components) {
        if (comp && comp->IsEnabled()) {
            comp->LateUpdate(deltaTime);
        }
    }
}

// ============================================================================
// AddChild
//  - 子にする対象に元の親がいれば、まずそちらから取り外す（孤児化防止）
//  - 親子関係の変更は ActiveInHierarchy に影響 → 差分適用で OnEnable/OnDisable を正しく発火
//  - Transform の親子を同期したい場合は、プロジェクト方針に合わせてここで行う
// ============================================================================
void GameObject::AddChild(std::shared_ptr<GameObject> child)
{
    // 追加直前の「子の実効状態」を保存（差分判定に使用）
    const bool prevChildHier = child->IsActive();

    // 既存の親から取り外す（RemoveChild は “ルート戻し + 差分適用” を行う設計）
    if (auto oldParent = child->m_Parent.lock()) {
        oldParent->RemoveChild(child);
    }

    // 自分の子として登録し、親参照を張る
    m_Children.push_back(child);
    child->m_Parent = shared_from_this();

    // Transform の親子関係を同期したい場合はここで実施
    // 例: child->Transform->SetParent(Transform.get());

    // 親が変わった結果、子の ActiveInHierarchy が変わるなら差分適用
    child->RefreshActiveInHierarchyRecursive(prevChildHier);
}

// ============================================================================
// RemoveChild
//  - 自分の子配列から除外し、親参照をクリア
//  - 「親から外れたら Scene のルートへ戻す」ポリシーで到達可能性を維持
//  - ActiveInHierarchy が変化したら差分適用（OnEnable/OnDisable）
//  - Reparent（付け替え）時は AddChild 側でも差分を行うため、続けて2回変化しうる点に注意
// ============================================================================
void GameObject::RemoveChild(std::shared_ptr<GameObject> child)
{
    const bool prevChildHier = child->IsActive();

    erase_remove(m_Children, child);
    child->m_Parent.reset();

    // ルートへ戻す（Scene 管理下に残す）
    if (auto scene = child->m_Scene.lock()) {
        scene->AddGameObject(child, nullptr); // 親 nullptr → ルート登録
    }

    // 実効状態の差分適用（イベント発火）
    child->RefreshActiveInHierarchyRecursive(prevChildHier);
}

// ============================================================================
// SetActive（自己フラグの切替）
//  ポリシー：
//    - 子の activeSelf は変更しない（Unity と同様）。親の無効化で実効も無効になる
//    - OnEnable/OnDisable は ActiveInHierarchy の変化時のみ発火
//    - Scene 管理（ルート配列）との整合をとる（本エンジン方針）
//      ※ Unity と違い「非アクティブ → 巡回対象から外す」方針にしている例
// ============================================================================
void GameObject::SetActive(bool active)
{
    // 変更前の実効状態を取得（差分判定用）
    const bool prevHier = IsActive();

    // すでに同じ状態なら Scene 側との整合を軽く取って終了
    if (m_Active == active) {
        if (auto scene = m_Scene.lock()) {
            if (m_Active) {
                // ルート配列に存在しなければ追加
                if (!scene->ContainsRootGameObject(shared_from_this())) {
                    scene->AddGameObject(shared_from_this());
                }
            }
            else {
                // 非アクティブなら巡回対象から外す（方針次第）
                scene->RemoveGameObject(shared_from_this());
            }
        }
        return;
    }

    // 自己フラグを切替
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

// ============================================================================
// IsActive（ActiveInHierarchy 相当）
//  - 自己が無効なら false
//  - 親があれば親の ActiveInHierarchy に従う
//  - ルートなら自己の activeSelf がそのまま実効
// ============================================================================
bool GameObject::IsActive() const
{
    if (!m_Active) return false;
    if (auto parent = m_Parent.lock()) {
        return parent->IsActive();
    }
    return true; // 親なし（ルート）
}

// ============================================================================
// Destroy（冪等）
//  - 有効なコンポーネントに OnDisable を先に通知 → その後 OnDestroy
//  - 子も再帰的に Destroy
//  - Scene/Parent からの切り離しは Scene 側の破棄フローで行う（順序の一元管理）
// ============================================================================
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

    // 子も同様に破棄（再帰）
    for (auto& child : m_Children) {
        child->Destroy();
    }
    m_Children.clear();
}

// ============================================================================
// Render
//  - 実効的に非アクティブなら描画しない
//  - 各描画系コンポーネントが Renderer に対して描画要求を行う前提
// ============================================================================
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

// 現在の ActiveInHierarchy を再評価して返す（IsActive と同義だが意図明確化）
bool GameObject::ComputeActiveInHierarchy() const
{
    return IsActive();
}

// ActiveInHierarchy の変化を自分に適用（OnEnable/OnDisable 発火＆キャッシュ更新）
void GameObject::ApplyActiveInHierarchyDelta(bool wasActiveInHierarchy, bool nowActiveInHierarchy)
{
    if (wasActiveInHierarchy == nowActiveInHierarchy) {
        return; // 変化なし → 何もしない
    }

    if (nowActiveInHierarchy) {
        // 実効的に有効になった：有効なコンポーネントへ OnEnable
        for (auto& comp : m_Components) {
            if (comp && comp->IsEnabled()) comp->OnEnable();
        }
    }
    else {
        // 実効的に無効になった：有効なコンポーネントへ OnDisable
        for (auto& comp : m_Components) {
            if (comp && comp->IsEnabled()) comp->OnDisable();
        }
    }

    // キャッシュを最新化（子の差分適用時に prev として渡す）
    m_LastActiveInHierarchy = nowActiveInHierarchy;
}

// 自分と配下（ツリー全体）について ActiveInHierarchy の差分を適用
// @param prevOfThis : 呼び出し直前における「このノード自身」の実効状態
void GameObject::RefreshActiveInHierarchyRecursive(bool prevOfThis)
{
    const bool now = ComputeActiveInHierarchy();

    // まず自分に適用（ここで m_LastActiveInHierarchy が更新される）
    ApplyActiveInHierarchyDelta(prevOfThis, now);

    // 子へは「それぞれの直前状態（キャッシュ）」を prev として渡し、再帰適用
    for (auto& ch : m_Children) {
        if (!ch) continue;
        const bool childPrev = ch->m_LastActiveInHierarchy;
        ch->RefreshActiveInHierarchyRecursive(childPrev);
    }
}
