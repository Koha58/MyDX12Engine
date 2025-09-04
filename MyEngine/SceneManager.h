#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "Scene.h"

class SceneManager
{
public:
    // �V�[����o�^
    void AddScene(const std::string& name, std::shared_ptr<Scene> scene);

    // �V�[�����폜
    void RemoveScene(const std::string& name);

    // �V�[���؂�ւ�
    void SwitchScene(const std::string& name);

    // ���݂̃A�N�e�B�u�V�[�����擾�iUnity���j
    std::shared_ptr<Scene> GetActiveScene() const;

    // ���t���[���X�V
    void Update(float deltaTime);

private:
    std::unordered_map<std::string, std::shared_ptr<Scene>> m_Scenes;
    std::weak_ptr<Scene> m_CurrentScene;
};
