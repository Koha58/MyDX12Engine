#pragma once
#include <string>
#include <vector>
#include <memory>

class GameObject;

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

    // シーンにルートGameObjectを追加する
    void AddGameObject(std::shared_ptr<GameObject> gameObject,
        std::shared_ptr<GameObject> parent = nullptr);

    // シーンからルートGameObjectを削除する
    void RemoveGameObject(std::shared_ptr<GameObject> gameObject);

    // シーン内のすべてのルートGameObjectを更新する
    void Update(float deltaTime);

    // シーンのルートGameObjectリストをconst参照で取得する
    const std::vector<std::shared_ptr<GameObject>>& GetRootGameObjects() const { return m_RootGameObjects; }

private:
    std::string m_Name;                                    // シーンの名前
    std::vector<std::shared_ptr<GameObject>> m_RootGameObjects; // シーンの最上位階層にあるGameObjectのリスト
};
