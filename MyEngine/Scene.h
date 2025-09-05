#pragma once
#include <string>
#include <vector>
#include <memory>

class GameObject;
class D3D12Renderer;

// --- Sceneクラス ---
// 複数のGameObjectを管理し、シーン全体の更新を行う
// ゲームの論理的な区切り（例: レベル、メニュー画面など）を表す
class Scene : public std::enable_shared_from_this<Scene>
{
public:
    // コンストラクタ
    // @param name: シーンの名前 (デフォルトは"New Scene")
    Scene(const std::string& name = "New Scene");

    // デストラクタ
    ~Scene() = default;

    void SetActive(bool active); // シーン全体を有効/無効にする
    bool IsActive() const { return m_Active; }

    bool ContainsRootGameObject(std::shared_ptr<GameObject> obj) const {
        return std::find(m_RootGameObjects.begin(), m_RootGameObjects.end(), obj) != m_RootGameObjects.end();
    }

    // GameObject のアクティブ状態を Scene 管理リストと同期させて切り替える
    void SetGameObjectActive(std::shared_ptr<GameObject> gameObject, bool active);

    void Render(D3D12Renderer* renderer);

    // シーンにルートGameObjectを追加する
    void AddGameObject(std::shared_ptr<GameObject> gameObject,
        std::shared_ptr<GameObject> parent = nullptr);

    // シーンからルートGameObjectを削除する
    void RemoveGameObject(std::shared_ptr<GameObject> gameObject);

    // UnityのDestroy相当（遅延実行）
    void DestroyGameObject(std::shared_ptr<GameObject> gameObject);

    void DestroyAllGameObjects();

    // シーン内のすべてのルートGameObjectを更新する
    void Update(float deltaTime);

    // シーンのルートGameObjectリストをconst参照で取得する
    const std::vector<std::shared_ptr<GameObject>>& GetRootGameObjects() const { return m_RootGameObjects; }

private:
    std::string m_Name;                                    // シーンの名前
    std::vector<std::shared_ptr<GameObject>> m_RootGameObjects; // シーンの最上位階層にあるGameObjectのリスト

    // Destroy予約リスト
    std::vector<std::shared_ptr<GameObject>> m_DestroyQueue;

    // Destroy実行用の内部関数（再帰）
    void ExecuteDestroy(std::shared_ptr<GameObject> gameObject);

    bool m_Active = true; // デフォルトは有効
};
