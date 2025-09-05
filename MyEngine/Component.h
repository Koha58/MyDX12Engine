#pragma once
#include <memory>

// �O���錾�i�w�b�_�Ԉˑ�������邽�� GameObject �̊��S�Ȓ�`�͕s�v�j
class GameObject;

// ================================================
// Component ���N���X
// --------------------------------
// �E�S�ẴR���|�[�l���g�̋��ʊ��N���X
// �ETransform, MeshRenderer, ����� MoveComponent �Ȃǂ���������h������
// �EUnity �� MonoBehaviour �ɋ߂����C�t�T�C�N����z�肵�Ă���
// ================================================
class Component
{
public:
    // ----------------------------
    // �R���|�[�l���g�̎�ނ����ʂ���񋓌^
    // ----------------------------
    enum ComponentType
    {
        None = 0,          // �ėp/���w��
        Transform,         // Transform �R���|�[�l���g
        MeshRenderer,      // MeshRenderer �R���|�[�l���g
        MAX_COMPONENT_TYPES
    };

    // �R���X�g���N�^�Ń^�C�v���󂯎���ĕێ�
    Component(ComponentType type) : m_Type(type) {}
    virtual ~Component() = default; // �p���N���X�ł��f�X�g���N�^���������Ă΂��悤���z��

    // ���g�̎�ނ��擾
    ComponentType GetType() const { return m_Type; }

    // ----------------------------
    // Unity�����C�t�T�C�N���֐�
    // ----------------------------
    // ���K�v�ɉ����Ĕh���N���X�ŃI�[�o�[���C�h����
    virtual void Awake() {}                     // �R���|�[�l���g��������Ɉ�x�����Ă΂��
    virtual void OnEnable() {}                  // �L�������ꂽ���ɌĂ΂��
    virtual void Start() {}                     // �ŏ��� Update ���Ă΂�钼�O�Ɉ�x�����Ă΂��
    virtual void Update(float deltaTime) {}     // ���t���[���Ă΂��X�V����
    virtual void LateUpdate(float deltaTime) {} // Update �̌�ɌĂ΂��i��������p�j
    virtual void OnDisable() {}                 // ���������ꂽ���ɌĂ΂��
    virtual void OnDestroy() {}                 // �j������钼�O�ɌĂ΂��

    // �`��p�BMeshRenderer �ȂǕ`�揈�������R���|�[�l���g�ŃI�[�o�[���C�h
    virtual void Render(class D3D12Renderer* renderer) {}

    // ----------------------------
    // �I�[�i�[(GameObject)�Ǘ�
    // ----------------------------
    // �E�ǂ� GameObject �ɑ����Ă��邩���Ǘ�����
    // �Eshared_ptr ������� weak_ptr �ɂ��邱�Ƃŏz�Q�Ƃ�h��
    void SetOwner(std::shared_ptr<GameObject> owner) { m_Owner = owner; }
    std::shared_ptr<GameObject> GetOwner() const { return m_Owner.lock(); }

    // ----------------------------
    // �L��/�����t���O�Ǘ�
    // ----------------------------
    bool IsEnabled() const { return m_Enabled; }

    // ��Ԃ�؂�ւ���� OnEnable/OnDisable �������ŌĂяo��
    void SetEnabled(bool enabled) {
        if (m_Enabled != enabled) {
            m_Enabled = enabled;
            if (m_Enabled) OnEnable();
            else OnDisable();
        }
    }

    // ----------------------------
    // Start() �Ăяo���Ǘ�
    // ----------------------------
    // �EStart ��1�񂵂��Ă΂�Ȃ����߁A�t���O�Ő��䂷��
    bool HasStarted() const { return m_Started; }
    void MarkStarted() { m_Started = true; }

protected:
    ComponentType m_Type;           // ���̃R���|�[�l���g�̎��
    std::weak_ptr<GameObject> m_Owner; // �������� GameObject�i�z�Q�Ƃ�����邽�� weak_ptr�j

    // ��ԃt���O
    bool m_Started = false; // Start() �����ɌĂ΂ꂽ���ǂ���
    bool m_Enabled = true;  // ���ݗL�����ǂ����iOnEnable/OnDisable �̌Ăяo������p�j
};
