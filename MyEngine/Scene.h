#pragma once
#include <string>
#include <vector>
#include <memory>

class GameObject;
class D3D12Renderer;

// --- Scene�N���X ---
// ������GameObject���Ǘ����A�V�[���S�̂̍X�V���s��
// �Q�[���̘_���I�ȋ�؂�i��: ���x���A���j���[��ʂȂǁj��\��
class Scene : public std::enable_shared_from_this<Scene>
{
public:
    // �R���X�g���N�^
    // @param name: �V�[���̖��O (�f�t�H���g��"New Scene")
    Scene(const std::string& name = "New Scene");

    // �f�X�g���N�^
    ~Scene() = default;

    void SetActive(bool active); // �V�[���S�̂�L��/�����ɂ���
    bool IsActive() const { return m_Active; }

    void Render(D3D12Renderer* renderer);

    // �V�[���Ƀ��[�gGameObject��ǉ�����
    void AddGameObject(std::shared_ptr<GameObject> gameObject,
        std::shared_ptr<GameObject> parent = nullptr);

    // �V�[�����烋�[�gGameObject���폜����
    void RemoveGameObject(std::shared_ptr<GameObject> gameObject);

    // Unity��Destroy�����i�x�����s�j
    void DestroyGameObject(std::shared_ptr<GameObject> gameObject);

    void DestroyAllGameObjects();

    // �V�[�����̂��ׂẴ��[�gGameObject���X�V����
    void Update(float deltaTime);

    // �V�[���̃��[�gGameObject���X�g��const�Q�ƂŎ擾����
    const std::vector<std::shared_ptr<GameObject>>& GetRootGameObjects() const { return m_RootGameObjects; }

private:
    std::string m_Name;                                    // �V�[���̖��O
    std::vector<std::shared_ptr<GameObject>> m_RootGameObjects; // �V�[���̍ŏ�ʊK�w�ɂ���GameObject�̃��X�g

    // Destroy�\�񃊃X�g
    std::vector<std::shared_ptr<GameObject>> m_DestroyQueue;

    // Destroy���s�p�̓����֐��i�ċA�j
    void ExecuteDestroy(std::shared_ptr<GameObject> gameObject);

    bool m_Active = true; // �f�t�H���g�͗L��
};
