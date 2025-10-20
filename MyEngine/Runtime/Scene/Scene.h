#pragma once
// 多重インクルード防止。Scene は「ゲーム内の論理的なまとまり（レベル、メニュー等）」を表す。

#include <string>
#include <vector>
#include <memory>
#include <algorithm> // std::find（ContainsRootGameObject で使用）

class GameObject;
class D3D12Renderer;

// ============================================================================
// Scene
// ----------------------------------------------------------------------------
// ・シーン直下（= 親なし）の GameObject 群を保持し、Update/Render の起点になる。
// ・GameObject の破棄は「即時」ではなく予約（DestroyQueue）→ フレーム終端で実行。
//   → 巡回中のコンテナ変更による不整合を避けるため。
// ・設計の位置付け：SceneManager が複数 Scene を切替える上位層、GameObject は下位の個体。
// ============================================================================
class Scene : public std::enable_shared_from_this<Scene>
{
public:
    //--------------------------------------------------------------------------
    // コンストラクタ
    // @param name : シーン名（デバッグ/識別用）
    //--------------------------------------------------------------------------
    explicit Scene(const std::string& name = "New Scene");
    ~Scene() = default;

    //--------------------------------------------------------------------------
    // Active 制御（シーン全体の ON/OFF）
    // ・false の場合、配下 GameObject の Update/Render を止める想定。
    //   実際の停止ロジックは GameObject 側の SetActive 連携で行う。
    //--------------------------------------------------------------------------
    void SetActive(bool active);
    bool IsActive() const { return m_Active; }

    //--------------------------------------------------------------------------
    // ContainsRootGameObject
    // ・引数の GameObject が「ルート配列」に含まれているかどうかを返す。
    //   （親ありの子オブジェクトかどうかは見ない）
    //--------------------------------------------------------------------------
    bool ContainsRootGameObject(std::shared_ptr<GameObject> obj) const {
        return std::find(m_RootGameObjects.begin(), m_RootGameObjects.end(), obj) != m_RootGameObjects.end();
    }

    //--------------------------------------------------------------------------
    // SetGameObjectActive
    // ・指定 GameObject（およびその子）を有効/無効に。
    // ・Scene のルート配列と GameObject 側の状態（activeSelf/OnEnable/OnDisable）を同期。
    //--------------------------------------------------------------------------
    void SetGameObjectActive(std::shared_ptr<GameObject> gameObject, bool active);

    //--------------------------------------------------------------------------
    // Render
    // ・全ルート GameObject に対して再帰的に Render を呼ぶ。
    // ・実描画は各コンポーネント（例：MeshRendererComponent）が行い、Renderer に委譲する。
    //--------------------------------------------------------------------------
    void Render(D3D12Renderer* renderer);

    //--------------------------------------------------------------------------
    // AddGameObject
    // ・GameObject をこのシーンへ所属させる。
    //   parent == nullptr の場合はルート配列へ、指定されていれば親の子として接続。
    // ・B案ライフサイクルに従い、ここでは Awake/OnEnable は呼ばない（AddComponent 側で実施）。
    //--------------------------------------------------------------------------
    void AddGameObject(std::shared_ptr<GameObject> gameObject,
        std::shared_ptr<GameObject> parent = nullptr);

    //--------------------------------------------------------------------------
    // RemoveGameObject
    // ・ルート配列から指定オブジェクトを取り除く。
    // ・親付きの場合は GameObject::RemoveChild を経由して階層から外れる前提。
    //   （最終的にルートへ戻すかどうかはポリシー次第）
    //--------------------------------------------------------------------------
    void RemoveGameObject(std::shared_ptr<GameObject> gameObject);

    //--------------------------------------------------------------------------
    // DestroyGameObject（破棄予約）
    // ・即時破棄はせず DestroyQueue に積む。フレーム終端で安全に実行。
    //--------------------------------------------------------------------------
    void DestroyGameObject(std::shared_ptr<GameObject> gameObject);

    //--------------------------------------------------------------------------
    // DestroyAllGameObjects
    // ・シーン配下すべての GameObject を破棄（OnDestroy を発火しつつツリーごと片付ける）。
    // ・シーン切替やアプリ終了時の一括解放用途。
    //--------------------------------------------------------------------------
    void DestroyAllGameObjects();

    //--------------------------------------------------------------------------
    // Update
    // ・全ルート GameObject を更新（内部で親→子→各コンポーネントに伝播）。
    // ・フレーム終端で DestroyQueue の中身を実破棄（予約→実行の二段階）。
    // @param deltaTime : 前フレームからの経過時間（秒）
    //--------------------------------------------------------------------------
    void Update(float deltaTime);

    //--------------------------------------------------------------------------
    // GetRootGameObjects
    // ・ルート配列（親なしの GameObject）を参照返し。
    //   外部からは編集できないよう const 参照を返す。
    //--------------------------------------------------------------------------
    const std::vector<std::shared_ptr<GameObject>>& GetRootGameObjects() const { return m_RootGameObjects; }

private:
    //================== 内部状態 ==================
    std::string m_Name;                                    // 識別用シーン名
    std::vector<std::shared_ptr<GameObject>> m_RootGameObjects; // 親なし GameObject の配列

    // Destroy 予約リスト（Update 終了時に処理）
    std::vector<std::shared_ptr<GameObject>> m_DestroyQueue;

    // Destroy 実行ユーティリティ：子→親の順で参照を外しつつ、Scene 管理から切り離す。
    // 実際の GameObject::Destroy() 呼び出しは Update 側で順序を統一して実行。
    void ExecuteDestroy(std::shared_ptr<GameObject> gameObject);

    bool m_Active = true; // シーン全体の有効フラグ（デフォルト有効）
};
