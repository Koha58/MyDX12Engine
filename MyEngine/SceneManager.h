#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "Scene.h"

class SceneManager
{
public:
    // シーンを登録
    void AddScene(const std::string& name, std::shared_ptr<Scene> scene);

    // シーンを削除
    void RemoveScene(const std::string& name);

    // シーン切り替え
    void SwitchScene(const std::string& name);

    // 現在のアクティブシーンを取得（Unity風）
    std::shared_ptr<Scene> GetActiveScene() const;

    // 毎フレーム更新
    void Update(float deltaTime);

private:
    std::unordered_map<std::string, std::shared_ptr<Scene>> m_Scenes;
    std::weak_ptr<Scene> m_CurrentScene;
};
