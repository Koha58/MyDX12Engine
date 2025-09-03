#pragma once // �����w�b�_�[�t�@�C����������C���N���[�h�����̂�h���v���v���Z�b�T�f�B���N�e�B�u

#include <vector>        // std::vector���g�p
#include <string>        // std::string���g�p
#include <DirectXMath.h> // DirectX::XMFLOAT3, DirectX::XMMATRIX �Ȃǂ��g�p
#include <memory>        // std::shared_ptr, std::weak_ptr, std::enable_shared_from_this ���g�p
#include <type_traits>   // std::is_base_of ���g�p�i�e���v���[�g���^�v���O���~���O�j

#include "TransformComponent.h" // �����ǉ�
#include "Component.h"

// �O���錾:
// �����̃N���X�̊��S�Ȓ�`�͂����ł͕s�v�����A�|�C���^��Q�ƌ^�Ƃ��Ďg�p���邽�߂ɐ錾
class Component;
class MeshRendererComponent; // Mesh.h �Œ�`�����
class Scene;


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

    std::shared_ptr<Scene> GetScene() const { return m_Scene.lock(); }
    void SetScene(std::shared_ptr<Scene> scene) { m_Scene = scene; }

    // �e�����S�Ɏ擾����w���p�[�iScene.cpp ����g���j
    std::shared_ptr<GameObject> GetParent() const { return m_Parent.lock(); }

    // �j���t���O���́u��������v
    explicit operator bool() const {
        return !m_Destroyed;
    }

    bool IsDestroyed() const { return m_Destroyed; }

    // Destroy �\�񂳂ꂽ�� Scene ���瑦�t���O�𗧂Ă�
    void MarkAsDestroyed() { m_Destroyed = true; }

    // GameObject���j�������Ƃ��ɌĂԊ֐�
    void Destroy();

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
    std::weak_ptr<Scene> m_Scene; // ��������������V�[��

    bool m_Destroyed = false;

    // Scene�N���X��m_Parent�ɃA�N�Z�X�ł���悤�Ƀt�����h�錾
    // Scene��GameObject�̐e�q�֌W���Ǘ�����ۂɕK�v�ƂȂ�
    friend class Scene;
};