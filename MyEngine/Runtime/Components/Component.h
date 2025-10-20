#pragma once
#include <memory>

/*
===============================================================================
 Component ���N���X
-------------------------------------------------------------------------------
����
- ���ׂẴQ�[���p�R���|�[�l���g�iTransform, MeshRenderer, Camera �Ȃǁj�̋��ʓy��B
- Unity �� MonoBehaviour ���C�N�ȃ��C�t�T�C�N���iAwake/Start/Update�c�j��񋟁B
- �u�ǂ� GameObject �ɑ����邩�v�̏��L�֌W��ێ��i�z�Q�Ƃ͉���j�B

�݌v�|���V�[
- ���L�֌W�FGameObject �� Component �� shared_ptr�AComponent �� GameObject �� weak_ptr�B
  �� �z�Q�Ƃ�����A�j���t���[��P�����B
- ���C�t�T�C�N���Ăяo���҂̈�ѐ��F
  * Awake/OnEnable �c�c AddComponent ���i�܂��� Scene �ւ̑g�ݍ��ݎ��j�Ɏ��s������j����ʓI�B
  * Start �c�c�c�c�c�c �ŏ��� Update �̒��O�� 1 �x�����B
- �L��/�����FSetEnabled() �� OnEnable/OnDisable ���������΁i�d���Ăяo���h�~�j�B
- �X���b�h�F��{�̓��C���X���b�h�O��i�����_������͂Ƌ������Ȃ��悤�Ɂj�B
- ���z�f�X�g���N�^�F�h���N���X�̔j�����ɐ������f�X�g���N�^���Ă΂��悤�ɕK�{�B

�g�p�K�C�h
- �V�����@�\�́u�h���N���X�v������ĕK�v�ȃ��C�t�T�C�N������ override ����B
- ���t���[���̏����� Update / LateUpdate �ɕ�����i�ˑ������𒲐����₷���j�B
- �`�悪����ꍇ�� Render(D3D12Renderer*) �� override�B
- �ꎞ�I�ɖ������������ꍇ�� SetEnabled(false) ���g���iOnDisable ������j�B
===============================================================================
*/

// �O���錾�i�w�b�_�ˑ����ŏ����j
class GameObject;

class Component
{
public:
    //-------------------------------------------------------------------------
    // �R���|�[�l���g�̎�ނ����ʁi�f�o�b�O/�G�f�B�^�p�r�j
    // �� �K�v�ɉ����Ēǉ��BMAX_COMPONENT_TYPES �͔z��T�C�Y�m�ۂȂǂɎg�p�B
    //-------------------------------------------------------------------------
    enum ComponentType
    {
        None = 0,          // �ėp/���w��
        Transform,         // Transform �R���|�[�l���g
        MeshRenderer,      // MeshRenderer �R���|�[�l���g
        Camera,            // Camera �R���|�[�l���g
        MAX_COMPONENT_TYPES
    };

    //-------------------------------------------------------------------------
    // Ctor / Dtor
    //  - type �͔h���N���X����Œ�œn���i��FComponent(ComponentType::Camera)�j
    //  - ���z�f�X�g���N�^�F�h���j���𐳂����s�����߂ɕK�{
    //-------------------------------------------------------------------------
    explicit Component(ComponentType type) : m_Type(type) {}
    virtual ~Component() = default;

    // ���g�̎�ނ��擾�i�G�f�B�^�\����^����Ɂj
    ComponentType GetType() const { return m_Type; }

    //-------------------------------------------------------------------------
    // ���C�t�T�C�N���i�K�v�Ȃ��̂��� override�j
    //  - Awake      : ��������Ɉ�x�����i�Q�Ƃ̉����E�������j
    //  - OnEnable   : �L�������ɓs�x�i�T�u�V�X�e���o�^�Ȃǁj
    //  - Start      : �ŏ��� Update ���O�Ɉ�x�����i�x���������j
    //  - Update     : ���t���[���i�Q�[�����W�b�N�j
    //  - LateUpdate : Update ��i�Ǐ]/�J�����ȂǏ����ˑ��ɕ֗��j
    //  - OnDisable  : ���������ɓs�x�i�T�u�V�X�e���o�^�����Ȃǁj
    //  - OnDestroy  : �j�����O�Ɉ�x�����iGPU/OS ���\�[�X����Ȃǁj
    //-------------------------------------------------------------------------
    virtual void Awake() {}
    virtual void OnEnable() {}
    virtual void Start() {}
    virtual void Update(float /*deltaTime*/) {}
    virtual void LateUpdate(float /*deltaTime*/) {}
    virtual void OnDisable() {}
    virtual void OnDestroy() {}

    //-------------------------------------------------------------------------
    // Render
    //  - �`�揈�������R���|�[�l���g�� override�iMeshRenderer �Ȃǁj
    //  - Renderer �ւ̃R�}���h�L�^�������ōs���݌v
    //-------------------------------------------------------------------------
    virtual void Render(class D3D12Renderer* /*renderer*/) {}

    //-------------------------------------------------------------------------
    // Owner�i�������� GameObject�j
    //  - �z�Q�Ɖ���̂��� weak_ptr�B�K�v���� lock() �� shared_ptr ���擾�B
    //  - AddComponent ���� Scene �ւ̑g�ݍ��ݎ��ɃG���W�����ŃZ�b�g����z��B
    //-------------------------------------------------------------------------
    void SetOwner(std::shared_ptr<GameObject> owner) { m_Owner = owner; }
    std::shared_ptr<GameObject> GetOwner() const { return m_Owner.lock(); }

    //-------------------------------------------------------------------------
    // �L��/�����؂�ւ�
    //  - �ύX���̂� OnEnable / OnDisable �𔭉΁i�d���Ăяo����h�~�j
    //  - �Q�[�����̈ꎞ��~���/�s���̐ؑւɗ��p
    //-------------------------------------------------------------------------
    bool IsEnabled() const { return m_Enabled; }
    void SetEnabled(bool enabled)
    {
        if (m_Enabled != enabled)
        {
            m_Enabled = enabled;
            if (m_Enabled) OnEnable();
            else           OnDisable();
        }
    }

    //-------------------------------------------------------------------------
    // Start() �Ăяo���Ǘ�
    //  - Start �͈�x����̂��߁A�t���O�Ő���i�G���W������ Update �����Ŏg�p�j
    //-------------------------------------------------------------------------
    bool HasStarted() const { return m_Started; }
    void MarkStarted() { m_Started = true; }

protected:
    // �^���i�f�o�b�O/�G�f�B�^�p�j
    ComponentType m_Type = ComponentType::None;

    // ���� GameObject�i�z�Q�Ɩh�~�̂��� weak�j
    std::weak_ptr<GameObject> m_Owner;

    // ��ԃt���O
    bool m_Started = false; // Start() �ς݂�
    bool m_Enabled = true;  // ���ݗL�����iOnEnable/OnDisable �̔��ΐ���j
};
