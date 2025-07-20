#pragma once // �����w�b�_�[�t�@�C����������C���N���[�h�����̂�h���v���v���Z�b�T�f�B���N�e�B�u

#include <vector>        // std::vector���g�p
#include <string>        // std::string���g�p
#include <DirectXMath.h> // DirectX::XMFLOAT3, DirectX::XMMATRIX �Ȃǂ��g�p
#include <memory>        // std::shared_ptr, std::weak_ptr, std::enable_shared_from_this ���g�p
#include <type_traits>   // std::is_base_of ���g�p�i�e���v���[�g���^�v���O���~���O�j

// �O���錾:
// �����̃N���X�̊��S�Ȓ�`�͂����ł͕s�v�����A�|�C���^��Q�ƌ^�Ƃ��Ďg�p���邽�߂ɐ錾
class Component;
class MeshRendererComponent; // Mesh.h �Œ�`�����

// --- Component�̃x�[�X�N���X ---
// ���ׂĂ̋�̓I�ȃR���|�[�l���g�͂��̃N���X����h������
// �R���|�[�l���g��GameObject�ɃA�^�b�`����A����̋@�\��񋟂���
class Component
{
public:
    // �R���|�[�l���g�̃^�C�v�����ʂ��邽�߂̗񋓌^
    enum ComponentType
    {
        None = 0,        // ���w��܂��̓f�t�H���g
        Transform,       // �ʒu�A��]�A�X�P�[�����Ǘ�����R���|�[�l���g
        MeshRenderer,    // ���b�V���̕`���S������R���|�[�l���g
        // �K�v�ɉ����đ��̃R���|�[�l���g�^�C�v��ǉ�����
        MAX_COMPONENT_TYPES // �^�̐��𐔂��邽�߂̔ԕ��l
    };

    // �R���X�g���N�^: �R���|�[�l���g�^�C�v���󂯎��
    Component(ComponentType type) : m_Type(type) {}

    // �f�X�g���N�^: �|�����[�t�B�b�N�ȍ폜���\�ɂ��邽�߂ɉ��z�f�X�g���N�^�Ƃ���
    virtual ~Component() = default;

    // �R���|�[�l���g�̃^�C�v���擾����Q�b�^�[
    ComponentType GetType() const { return m_Type; }

    // ���z�֐�: �e�R���|�[�l���g�ŗL�̃��W�b�N���������邽�߂ɃI�[�o�[���C�h�����
    virtual void Initialize() {} // �R���|�[�l���g���A�^�b�`���ꂽ��̏���������
    virtual void Update(float deltaTime) {} // �t���[�����Ƃ̍X�V����
    virtual void Render() {} // �`�揈�� (�ʏ�̓����_�����O�p�X���ŏ�������邱�Ƃ�����)

protected:
    ComponentType m_Type; // ���̃R���|�[�l���g�̃^�C�v
};

// --- TransformComponent�N���X ---
// Component����h�����AGameObject�̈ʒu�A��]�A�X�P�[�����Ǘ�����
class TransformComponent : public Component
{
public:
    DirectX::XMFLOAT3 Position; // �I�u�W�F�N�g��3D��Ԃɂ�����ʒu
    DirectX::XMFLOAT3 Rotation; // �I�u�W�F�N�g�̃I�C���[�p�ɂ���] (���W�A���P��)
    DirectX::XMFLOAT3 Scale;    // �I�u�W�F�N�g��3�������ւ̃X�P�[��

    // �R���X�g���N�^
    TransformComponent()
        : Component(ComponentType::Transform), // Component���N���X�̃R���X�g���N�^���Ăяo��
        Position(0.0f, 0.0f, 0.0f),            // �ʒu�����_�ŏ�����
        Rotation(0.0f, 0.0f, 0.0f),            // ��]�������� (����])
        Scale(1.0f, 1.0f, 1.0f) {              // �X�P�[����1.0�ŏ�����
    }

    // ���[���h�s����v�Z����֐�
    // �ʒu�A��]�A�X�P�[�����烂�f���̃��[���h�ϊ��s��𐶐�����
    DirectX::XMMATRIX GetWorldMatrix() const
    {
        using namespace DirectX; // DirectXMath���O��Ԃ�using�錾
        // �X�P�[���s��̐���
        XMMATRIX scaleMatrix = XMMatrixScaling(Scale.x, Scale.y, Scale.z);
        // �e������̉�]�s��̐��� (Z -> Y -> X �̏��ŏ�Z�����悤����)
        XMMATRIX rotationX = XMMatrixRotationX(Rotation.x);
        XMMATRIX rotationY = XMMatrixRotationY(Rotation.y);
        XMMATRIX rotationZ = XMMatrixRotationZ(Rotation.z);
        // ���s�ړ��s��̐���
        XMMATRIX translationMatrix = XMMatrixTranslation(Position.x, Position.y, Position.z);

        // ��ʓI�ȕϊ�����: �X�P�[�� -> ��] -> ���s�ړ�
        // �s�x�N�g�����g���ꍇ�A��Z������ (S * R * T) �ƂȂ�
        return scaleMatrix * rotationX * rotationY * rotationZ * translationMatrix;
    }
};

// --- GameObject�N���X ---
// �V�[�����̂��ׂẴI�u�W�F�N�g�̊�ՂƂȂ�N���X
// �R���|�[�l���g�̏W���̂Ƃ��āA�Q�[�����W�b�N�ƕ`��\�ȃA�Z�b�g��\������
class GameObject : public std::enable_shared_from_this<GameObject> // shared_ptr���玩�g��shared_ptr���擾�\�ɂ���
{
public:
    std::string Name;                                  // GameObject�̖��O
    std::shared_ptr<TransformComponent> Transform;     // ���ׂĂ�GameObject��TransformComponent�����i���J�A�N�Z�X�j

    // �R���X�g���N�^
    // @param name: GameObject�̖��O (�f�t�H���g��"GameObject")
    GameObject(const std::string& name = "GameObject");

    // �f�X�g���N�^
    ~GameObject();

    // �R���|�[�l���g��ǉ�����e���v���[�g�֐�
    // @typename T: �ǉ�����R���|�[�l���g�̌^ (Component����h�����Ă���K�v������)
    // @param Args&&... args: �R���|�[�l���g�̃R���X�g���N�^�ɓn������
    template<typename T, typename... Args>
    std::shared_ptr<T> AddComponent(Args&&... args)
    {
        // �ÓI�A�T�[�g: T �� Component �̊��N���X�ł��邩���R���p�C�����Ƀ`�F�b�N
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");

        // �R���|�[�l���g��shared_ptr���쐬���A�R���X�g���N�^�Ɉ�����]��
        std::shared_ptr<T> component = std::make_shared<T>(std::forward<Args>(args)...);
        m_Components.push_back(component); // �����̃R���|�[�l���g���X�g�ɒǉ�
        component->Initialize();           // �R���|�[�l���g��Initialize���\�b�h���Ăяo��
        return component;                  // �ǉ����ꂽ�R���|�[�l���g��shared_ptr��Ԃ�
    }

    // ����̌^�̃R���|�[�l���g���擾����e���v���[�g�֐�
    // @typename T: �擾�������R���|�[�l���g�̌^
    // @return: �w�肳�ꂽ�^�̃R���|�[�l���g��shared_ptr�B������Ȃ��ꍇ��nullptr
    template<typename T>
    std::shared_ptr<T> GetComponent()
    {
        // �ÓI�A�T�[�g: T �� Component �̊��N���X�ł��邩���R���p�C�����Ƀ`�F�b�N
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");

        // ���ׂẴR���|�[�l���g���C�e���[�g
        for (const auto& comp : m_Components)
        {
            // ���I�L���X�g�����݂�: ���N���X�|�C���^����h���N���X�|�C���^�ւ̈��S�ȕϊ�
            std::shared_ptr<T> specificComp = std::dynamic_pointer_cast<T>(comp);
            if (specificComp) // �L���X�g�����������ꍇ�i�܂�^����v�����ꍇ�j
            {
                return specificComp; // ���̃R���|�[�l���g��Ԃ�
            }
        }
        return nullptr; // ������Ȃ������ꍇ��nullptr��Ԃ�
    }

    // GameObject�̍X�V���W�b�N
    // �V�[���̍X�V���[�v����Ă΂�A���g�Ǝq�I�u�W�F�N�g�A�A�^�b�`���ꂽ�R���|�[�l���g���X�V����
    // @param deltaTime: �O�̃t���[������̌o�ߎ���
    void Update(float deltaTime);

    // �q�I�u�W�F�N�g�̊Ǘ�
    // @param child: �ǉ�����qGameObject��shared_ptr
    void AddChild(std::shared_ptr<GameObject> child);
    // @param child: �폜����qGameObject��shared_ptr
    void RemoveChild(std::shared_ptr<GameObject> child);
    // �q�I�u�W�F�N�g�̃��X�g��const�Q�ƂŎ擾����Q�b�^�[
    const std::vector<std::shared_ptr<GameObject>>& GetChildren() const { return m_Children; }

private:
    std::vector<std::shared_ptr<Component>> m_Components; // ����GameObject�ɃA�^�b�`���ꂽ�R���|�[�l���g�̃��X�g
    std::vector<std::shared_ptr<GameObject>> m_Children;  // ����GameObject�̎q�I�u�W�F�N�g�̃��X�g
    std::weak_ptr<GameObject> m_Parent;                   // �eGameObject�ւ̎ア�Q�Ɓi�z�Q�Ƃ�h�����߁j

    // Scene�N���X��m_Parent�ɃA�N�Z�X�ł���悤�Ƀt�����h�錾
    // Scene��GameObject�̐e�q�֌W���Ǘ�����ۂɕK�v�ƂȂ�
    friend class Scene;
};

// --- Scene�N���X ---
// ������GameObject���Ǘ����A�V�[���S�̂̍X�V���s��
// �Q�[���̘_���I�ȋ�؂�i��: ���x���A���j���[��ʂȂǁj��\��
class Scene
{
public:
    // �R���X�g���N�^
    // @param name: �V�[���̖��O (�f�t�H���g��"New Scene")
    Scene(const std::string& name = "New Scene");

    // �f�X�g���N�^
    ~Scene() = default;

    // �V�[���Ƀ��[�gGameObject��ǉ�����
    // @param gameObject: �ǉ�����GameObject��shared_ptr
    void AddGameObject(std::shared_ptr<GameObject> gameObject);
    // �V�[�����烋�[�gGameObject���폜����
    // @param gameObject: �폜����GameObject��shared_ptr
    void RemoveGameObject(std::shared_ptr<GameObject> gameObject);

    // �V�[�����̂��ׂẴ��[�gGameObject���X�V����
    // @param deltaTime: �O�̃t���[������̌o�ߎ���
    void Update(float deltaTime);

    // �V�[���̃��[�gGameObject���X�g��const�Q�ƂŎ擾����Q�b�^�[
    const std::vector<std::shared_ptr<GameObject>>& GetRootGameObjects() const { return m_RootGameObjects; }

private:
    std::string m_Name;                                    // �V�[���̖��O
    std::vector<std::shared_ptr<GameObject>> m_RootGameObjects; // �V�[���̍ŏ�ʊK�w�ɂ���GameObject�̃��X�g
};