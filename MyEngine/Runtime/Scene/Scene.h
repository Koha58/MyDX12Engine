#pragma once
#include <string>
#include <vector>
#include <memory>

class GameObject;
class D3D12Renderer;

// ===============================================================
// Scene クラス
// ---------------------------------------------------------------
// ・シーン全体を管理するクラス
// ・ルートGameObjectのリストを持ち、Update/Renderを一括管理
// ・Destroy処理は即時ではなく遅延実行キューを用意（安全性のため）
// ・ゲームの論理的な区切り（例: レベル/メニュー画面/ワールド）を表す
// ===============================================================
class Scene : public std::enable_shared_from_this<Scene>
{
public:
    // -----------------------------------------------------------
    // コンストラクタ
    // @param name : シーン名（デフォルト "New Scene"）
    // -----------------------------------------------------------
    Scene(const std::string& name = "New Scene");

    // デストラクタ
    ~Scene() = default;

    // -----------------------------------------------------------
    // Active 制御
    // シーン全体を有効/無効に切り替える（GameObjectのUpdate/Renderを停止する）
    // -----------------------------------------------------------
    void SetActive(bool active);
    bool IsActive() const { return m_Active; }

    // -----------------------------------------------------------
    // ContainsRootGameObject
    // @param obj : 検索対象
    // @return    : obj がルートGameObjectリストに含まれているかどうか
    // -----------------------------------------------------------
    bool ContainsRootGameObject(std::shared_ptr<GameObject> obj) const {
        return std::find(m_RootGameObjects.begin(), m_RootGameObjects.end(), obj) != m_RootGameObjects.end();
    }

    // -----------------------------------------------------------
    // SetGameObjectActive
    // ・指定 GameObject のアクティブ状態を切り替え
    // ・Scene 内の管理リストと同期を取る
    // -----------------------------------------------------------
    void SetGameObjectActive(std::shared_ptr<GameObject> gameObject, bool active);

    // -----------------------------------------------------------
    // Render
    // ・Scene 内のすべてのルートGameObjectに対して描画処理を行う
    // ・各GameObject → 各コンポーネントのRenderを呼ぶ
    // -----------------------------------------------------------
    void Render(D3D12Renderer* renderer);

    // -----------------------------------------------------------
    // AddGameObject
    // ・GameObject をシーンに追加する
    // ・parent が nullptr の場合 → ルートリストに追加
    // ・parent が指定された場合 → その親の子リストに追加
    // -----------------------------------------------------------
    void AddGameObject(std::shared_ptr<GameObject> gameObject,
        std::shared_ptr<GameObject> parent = nullptr);

    // -----------------------------------------------------------
    // RemoveGameObject
    // ・ルートGameObjectリストから指定オブジェクトを削除
    // ・親子関係を持つ場合は GameObject::RemoveChild が使われる想定
    // -----------------------------------------------------------
    void RemoveGameObject(std::shared_ptr<GameObject> gameObject);

    // -----------------------------------------------------------
    // DestroyGameObject
    // ・即時破棄ではなく DestroyQueue に登録して遅延実行する
    //   （Update 中のリスト変更によるクラッシュを防ぐ設計）
    // -----------------------------------------------------------
    void DestroyGameObject(std::shared_ptr<GameObject> gameObject);

    // -----------------------------------------------------------
    // DestroyAllGameObjects
    // ・シーン内の全てのGameObjectを破棄
    // ・シーン切替時などに呼ばれる
    // -----------------------------------------------------------
    void DestroyAllGameObjects();

    // -----------------------------------------------------------
    // Update
    // ・シーン内の全ルートGameObjectを更新
    // ・内部で DestroyQueue の処理を行う（安全な破棄）
    // @param deltaTime : 前フレームからの経過時間
    // -----------------------------------------------------------
    void Update(float deltaTime);

    // -----------------------------------------------------------
    // GetRootGameObjects
    // ・ルートGameObjectのリストを const参照で返す
    //   → 外部から変更不可
    // -----------------------------------------------------------
    const std::vector<std::shared_ptr<GameObject>>& GetRootGameObjects() const { return m_RootGameObjects; }

private:
    // ================== 内部状態 ==================
    std::string m_Name;                                    // シーンの名前
    std::vector<std::shared_ptr<GameObject>> m_RootGameObjects; // シーン直下に属する GameObject

    // Destroy 予約リスト（Update 終了時に実行）
    std::vector<std::shared_ptr<GameObject>> m_DestroyQueue;

    // Destroy 実行用の内部関数（再帰的に子も破棄）
    void ExecuteDestroy(std::shared_ptr<GameObject> gameObject);

    bool m_Active = true; // シーン全体が有効かどうか（デフォルト true）
};
