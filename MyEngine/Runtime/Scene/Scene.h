#pragma once
#include <string>
#include <vector>
#include <memory>

class GameObject;
class D3D12Renderer;

// ===============================================================
// Scene �N���X
// ---------------------------------------------------------------
// �E�V�[���S�̂��Ǘ�����N���X
// �E���[�gGameObject�̃��X�g�������AUpdate/Render���ꊇ�Ǘ�
// �EDestroy�����͑����ł͂Ȃ��x�����s�L���[��p�Ӂi���S���̂��߁j
// �E�Q�[���̘_���I�ȋ�؂�i��: ���x��/���j���[���/���[���h�j��\��
// ===============================================================
class Scene : public std::enable_shared_from_this<Scene>
{
public:
    // -----------------------------------------------------------
    // �R���X�g���N�^
    // @param name : �V�[�����i�f�t�H���g "New Scene"�j
    // -----------------------------------------------------------
    Scene(const std::string& name = "New Scene");

    // �f�X�g���N�^
    ~Scene() = default;

    // -----------------------------------------------------------
    // Active ����
    // �V�[���S�̂�L��/�����ɐ؂�ւ���iGameObject��Update/Render���~����j
    // -----------------------------------------------------------
    void SetActive(bool active);
    bool IsActive() const { return m_Active; }

    // -----------------------------------------------------------
    // ContainsRootGameObject
    // @param obj : �����Ώ�
    // @return    : obj �����[�gGameObject���X�g�Ɋ܂܂�Ă��邩�ǂ���
    // -----------------------------------------------------------
    bool ContainsRootGameObject(std::shared_ptr<GameObject> obj) const {
        return std::find(m_RootGameObjects.begin(), m_RootGameObjects.end(), obj) != m_RootGameObjects.end();
    }

    // -----------------------------------------------------------
    // SetGameObjectActive
    // �E�w�� GameObject �̃A�N�e�B�u��Ԃ�؂�ւ�
    // �EScene ���̊Ǘ����X�g�Ɠ��������
    // -----------------------------------------------------------
    void SetGameObjectActive(std::shared_ptr<GameObject> gameObject, bool active);

    // -----------------------------------------------------------
    // Render
    // �EScene ���̂��ׂẴ��[�gGameObject�ɑ΂��ĕ`�揈�����s��
    // �E�eGameObject �� �e�R���|�[�l���g��Render���Ă�
    // -----------------------------------------------------------
    void Render(D3D12Renderer* renderer);

    // -----------------------------------------------------------
    // AddGameObject
    // �EGameObject ���V�[���ɒǉ�����
    // �Eparent �� nullptr �̏ꍇ �� ���[�g���X�g�ɒǉ�
    // �Eparent ���w�肳�ꂽ�ꍇ �� ���̐e�̎q���X�g�ɒǉ�
    // -----------------------------------------------------------
    void AddGameObject(std::shared_ptr<GameObject> gameObject,
        std::shared_ptr<GameObject> parent = nullptr);

    // -----------------------------------------------------------
    // RemoveGameObject
    // �E���[�gGameObject���X�g����w��I�u�W�F�N�g���폜
    // �E�e�q�֌W�����ꍇ�� GameObject::RemoveChild ���g����z��
    // -----------------------------------------------------------
    void RemoveGameObject(std::shared_ptr<GameObject> gameObject);

    // -----------------------------------------------------------
    // DestroyGameObject
    // �E�����j���ł͂Ȃ� DestroyQueue �ɓo�^���Ēx�����s����
    //   �iUpdate ���̃��X�g�ύX�ɂ��N���b�V����h���݌v�j
    // -----------------------------------------------------------
    void DestroyGameObject(std::shared_ptr<GameObject> gameObject);

    // -----------------------------------------------------------
    // DestroyAllGameObjects
    // �E�V�[�����̑S�Ă�GameObject��j��
    // �E�V�[���ؑ֎��ȂǂɌĂ΂��
    // -----------------------------------------------------------
    void DestroyAllGameObjects();

    // -----------------------------------------------------------
    // Update
    // �E�V�[�����̑S���[�gGameObject���X�V
    // �E������ DestroyQueue �̏������s���i���S�Ȕj���j
    // @param deltaTime : �O�t���[������̌o�ߎ���
    // -----------------------------------------------------------
    void Update(float deltaTime);

    // -----------------------------------------------------------
    // GetRootGameObjects
    // �E���[�gGameObject�̃��X�g�� const�Q�ƂŕԂ�
    //   �� �O������ύX�s��
    // -----------------------------------------------------------
    const std::vector<std::shared_ptr<GameObject>>& GetRootGameObjects() const { return m_RootGameObjects; }

private:
    // ================== ������� ==================
    std::string m_Name;                                    // �V�[���̖��O
    std::vector<std::shared_ptr<GameObject>> m_RootGameObjects; // �V�[�������ɑ����� GameObject

    // Destroy �\�񃊃X�g�iUpdate �I�����Ɏ��s�j
    std::vector<std::shared_ptr<GameObject>> m_DestroyQueue;

    // Destroy ���s�p�̓����֐��i�ċA�I�Ɏq���j���j
    void ExecuteDestroy(std::shared_ptr<GameObject> gameObject);

    bool m_Active = true; // �V�[���S�̂��L�����ǂ����i�f�t�H���g true�j
};
