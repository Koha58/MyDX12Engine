#pragma once
#include <vector>
#include <string>
#include <DirectXMath.h>
#include <memory> // std::shared_ptr
#include <type_traits> // std::is_base_of

// �O���錾
class Component;
class MeshRendererComponent;

// =====================================================================================
// �R���|�[�l���g�̃x�[�X�N���X
// ���ׂĂ̋�̓I�ȃR���|�[�l���g�͂��̃N���X����h������
// =====================================================================================
class Component
{
public:
    enum ComponentType
    {
        None = 0,
        Transform,
        MeshRenderer,
        // �K�v�ɉ����đ��̃R���|�[�l���g�^�C�v��ǉ�
        MAX_COMPONENT_TYPES // �^�̐��𐔂��邽�߂̔ԕ�
    };

    Component(ComponentType type) : m_Type(type) {}
    virtual ~Component() = default;

    ComponentType GetType() const { return m_Type; }

    // �������A�X�V�A�`��ȂǁA�R���|�[�l���g�ŗL�̃��W�b�N
    virtual void Initialize() {}
    virtual void Update(float deltaTime) {}
    // �`��̓����_�����O�p�X���ŏ�������邱�Ƃ������̂ŁA�����ł̓V���v���ȗ�ɗ��߂�
    virtual void Render() {}

protected:
    ComponentType m_Type;
};

// =====================================================================================
// �g�����X�t�H�[���R���|�[�l���g
// Component ����h�������邱�ƂŁA���ʂ̃C���^�[�t�F�[�X���g�p�ł���悤�ɂȂ�
// =====================================================================================
class TransformComponent : public Component
{
public:
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Rotation; // �I�C���[�p (���W�A��)
    DirectX::XMFLOAT3 Scale;

    TransformComponent()
        : Component(ComponentType::Transform), // Component �̃R���X�g���N�^���Ăяo��
        Position(0.0f, 0.0f, 0.0f),
        Rotation(0.0f, 0.0f, 0.0f),
        Scale(1.0f, 1.0f, 1.0f) {
    }

    // ���[���h�s����v�Z����
    DirectX::XMMATRIX GetWorldMatrix() const
    {
        using namespace DirectX;
        XMMATRIX scaleMatrix = XMMatrixScaling(Scale.x, Scale.y, Scale.z);
        XMMATRIX rotationX = XMMatrixRotationX(Rotation.x);
        XMMATRIX rotationY = XMMatrixRotationY(Rotation.y);
        XMMATRIX rotationZ = XMMatrixRotationZ(Rotation.z);
        XMMATRIX translationMatrix = XMMatrixTranslation(Position.x, Position.y, Position.z);

        // �X�P�[�� -> ��] -> ���s�ړ� �̏��œK�p
        return scaleMatrix * rotationX * rotationY * rotationZ * translationMatrix;
    }
};

// =====================================================================================
// GameObject�N���X
// �V�[�����̂��ׂẴI�u�W�F�N�g�̊�ՂƂȂ�N���X
// =====================================================================================
class GameObject : public std::enable_shared_from_this<GameObject>
{
public:
    std::string Name;
    std::shared_ptr<TransformComponent> Transform; // ���ׂĂ�GameObject��Transform������

    GameObject(const std::string& name = "GameObject");
    ~GameObject();

    // �R���|�[�l���g��ǉ�����e���v���[�g�֐�
    template<typename T, typename... Args>
    std::shared_ptr<T> AddComponent(Args&&... args)
    {
        // T �� Component ����h�����Ă���K�v������
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");

        std::shared_ptr<T> component = std::make_shared<T>(std::forward<Args>(args)...);
        m_Components.push_back(component);
        component->Initialize(); // �R���|�[�l���g�̏��������Ăяo��
        return component;
    }

    // ����̌^�̃R���|�[�l���g���擾����
    template<typename T>
    std::shared_ptr<T> GetComponent()
    {
        // T �� Component ����h�����Ă���K�v������
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component.");

        for (const auto& comp : m_Components)
        {
            // ���I�L���X�g���g�p���Č^���`�F�b�N
            std::shared_ptr<T> specificComp = std::dynamic_pointer_cast<T>(comp);
            if (specificComp)
            {
                return specificComp;
            }
        }
        return nullptr;
    }

    // �V�[���̍X�V���[�v����Ă΂��
    void Update(float deltaTime);

    // �q�I�u�W�F�N�g�̊Ǘ�
    void AddChild(std::shared_ptr<GameObject> child);
    void RemoveChild(std::shared_ptr<GameObject> child);
    const std::vector<std::shared_ptr<GameObject>>& GetChildren() const { return m_Children; }

private:
    std::vector<std::shared_ptr<Component>> m_Components;
    std::vector<std::shared_ptr<GameObject>> m_Children;
    std::weak_ptr<GameObject> m_Parent; // �e�ւ̎Q�Ɓi�z�Q�Ƃ�h������weak_ptr�j
};

// =====================================================================================
// �V�[���N���X
// ������GameObject���Ǘ����A�V�[���S�̂̍X�V���s��
// =====================================================================================
class Scene
{
public:
    Scene(const std::string& name = "New Scene");
    ~Scene() = default;

    void AddGameObject(std::shared_ptr<GameObject> gameObject);
    void RemoveGameObject(std::shared_ptr<GameObject> gameObject);

    void Update(float deltaTime);

    const std::vector<std::shared_ptr<GameObject>>& GetRootGameObjects() const { return m_RootGameObjects; }

private:
    std::string m_Name;
    std::vector<std::shared_ptr<GameObject>> m_RootGameObjects; // ���[�g�K�w��GameObject
};