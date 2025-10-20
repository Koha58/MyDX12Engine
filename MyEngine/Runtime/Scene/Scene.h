#pragma once
// ���d�C���N���[�h�h�~�BScene �́u�Q�[�����̘_���I�Ȃ܂Ƃ܂�i���x���A���j���[���j�v��\���B

#include <string>
#include <vector>
#include <memory>
#include <algorithm> // std::find�iContainsRootGameObject �Ŏg�p�j

class GameObject;
class D3D12Renderer;

// ============================================================================
// Scene
// ----------------------------------------------------------------------------
// �E�V�[�������i= �e�Ȃ��j�� GameObject �Q��ێ����AUpdate/Render �̋N�_�ɂȂ�B
// �EGameObject �̔j���́u�����v�ł͂Ȃ��\��iDestroyQueue�j�� �t���[���I�[�Ŏ��s�B
//   �� ���񒆂̃R���e�i�ύX�ɂ��s����������邽�߁B
// �E�݌v�̈ʒu�t���FSceneManager ������ Scene ��ؑւ����ʑw�AGameObject �͉��ʂ̌́B
// ============================================================================
class Scene : public std::enable_shared_from_this<Scene>
{
public:
    //--------------------------------------------------------------------------
    // �R���X�g���N�^
    // @param name : �V�[�����i�f�o�b�O/���ʗp�j
    //--------------------------------------------------------------------------
    explicit Scene(const std::string& name = "New Scene");
    ~Scene() = default;

    //--------------------------------------------------------------------------
    // Active ����i�V�[���S�̂� ON/OFF�j
    // �Efalse �̏ꍇ�A�z�� GameObject �� Update/Render ���~�߂�z��B
    //   ���ۂ̒�~���W�b�N�� GameObject ���� SetActive �A�g�ōs���B
    //--------------------------------------------------------------------------
    void SetActive(bool active);
    bool IsActive() const { return m_Active; }

    //--------------------------------------------------------------------------
    // ContainsRootGameObject
    // �E������ GameObject ���u���[�g�z��v�Ɋ܂܂�Ă��邩�ǂ�����Ԃ��B
    //   �i�e����̎q�I�u�W�F�N�g���ǂ����͌��Ȃ��j
    //--------------------------------------------------------------------------
    bool ContainsRootGameObject(std::shared_ptr<GameObject> obj) const {
        return std::find(m_RootGameObjects.begin(), m_RootGameObjects.end(), obj) != m_RootGameObjects.end();
    }

    //--------------------------------------------------------------------------
    // SetGameObjectActive
    // �E�w�� GameObject�i����т��̎q�j��L��/�����ɁB
    // �EScene �̃��[�g�z��� GameObject ���̏�ԁiactiveSelf/OnEnable/OnDisable�j�𓯊��B
    //--------------------------------------------------------------------------
    void SetGameObjectActive(std::shared_ptr<GameObject> gameObject, bool active);

    //--------------------------------------------------------------------------
    // Render
    // �E�S���[�g GameObject �ɑ΂��čċA�I�� Render ���ĂԁB
    // �E���`��͊e�R���|�[�l���g�i��FMeshRendererComponent�j���s���ARenderer �ɈϏ�����B
    //--------------------------------------------------------------------------
    void Render(D3D12Renderer* renderer);

    //--------------------------------------------------------------------------
    // AddGameObject
    // �EGameObject �����̃V�[���֏���������B
    //   parent == nullptr �̏ꍇ�̓��[�g�z��ցA�w�肳��Ă���ΐe�̎q�Ƃ��Đڑ��B
    // �EB�ă��C�t�T�C�N���ɏ]���A�����ł� Awake/OnEnable �͌Ă΂Ȃ��iAddComponent ���Ŏ��{�j�B
    //--------------------------------------------------------------------------
    void AddGameObject(std::shared_ptr<GameObject> gameObject,
        std::shared_ptr<GameObject> parent = nullptr);

    //--------------------------------------------------------------------------
    // RemoveGameObject
    // �E���[�g�z�񂩂�w��I�u�W�F�N�g����菜���B
    // �E�e�t���̏ꍇ�� GameObject::RemoveChild ���o�R���ĊK�w����O���O��B
    //   �i�ŏI�I�Ƀ��[�g�֖߂����ǂ����̓|���V�[����j
    //--------------------------------------------------------------------------
    void RemoveGameObject(std::shared_ptr<GameObject> gameObject);

    //--------------------------------------------------------------------------
    // DestroyGameObject�i�j���\��j
    // �E�����j���͂��� DestroyQueue �ɐςށB�t���[���I�[�ň��S�Ɏ��s�B
    //--------------------------------------------------------------------------
    void DestroyGameObject(std::shared_ptr<GameObject> gameObject);

    //--------------------------------------------------------------------------
    // DestroyAllGameObjects
    // �E�V�[���z�����ׂĂ� GameObject ��j���iOnDestroy �𔭉΂��c���[���ƕЕt����j�B
    // �E�V�[���ؑւ�A�v���I�����̈ꊇ����p�r�B
    //--------------------------------------------------------------------------
    void DestroyAllGameObjects();

    //--------------------------------------------------------------------------
    // Update
    // �E�S���[�g GameObject ���X�V�i�����Őe���q���e�R���|�[�l���g�ɓ`�d�j�B
    // �E�t���[���I�[�� DestroyQueue �̒��g�����j���i�\�񁨎��s�̓�i�K�j�B
    // @param deltaTime : �O�t���[������̌o�ߎ��ԁi�b�j
    //--------------------------------------------------------------------------
    void Update(float deltaTime);

    //--------------------------------------------------------------------------
    // GetRootGameObjects
    // �E���[�g�z��i�e�Ȃ��� GameObject�j���Q�ƕԂ��B
    //   �O������͕ҏW�ł��Ȃ��悤 const �Q�Ƃ�Ԃ��B
    //--------------------------------------------------------------------------
    const std::vector<std::shared_ptr<GameObject>>& GetRootGameObjects() const { return m_RootGameObjects; }

private:
    //================== ������� ==================
    std::string m_Name;                                    // ���ʗp�V�[����
    std::vector<std::shared_ptr<GameObject>> m_RootGameObjects; // �e�Ȃ� GameObject �̔z��

    // Destroy �\�񃊃X�g�iUpdate �I�����ɏ����j
    std::vector<std::shared_ptr<GameObject>> m_DestroyQueue;

    // Destroy ���s���[�e�B���e�B�F�q���e�̏��ŎQ�Ƃ��O���AScene �Ǘ�����؂藣���B
    // ���ۂ� GameObject::Destroy() �Ăяo���� Update ���ŏ����𓝈ꂵ�Ď��s�B
    void ExecuteDestroy(std::shared_ptr<GameObject> gameObject);

    bool m_Active = true; // �V�[���S�̗̂L���t���O�i�f�t�H���g�L���j
};
