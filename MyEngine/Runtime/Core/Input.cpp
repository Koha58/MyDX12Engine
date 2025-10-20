// Input.cpp
//------------------------------------------------------------------------------
// �����FWin32 ���b�Z�[�W���� �g�t���[���P�ʂ̓��͏�ԁh ��g�ݗ��ĂĒ񋟂���B
//       - �L�[/�}�E�X�{�^���̌���/�O�t���[����ԁi�����E�����n�߁E�����j
//       - �}�E�X���W�i�E�B���h�E���W�j�ƈړ��ʁi���j
//       - �}�E�X�z�C�[���i�t���[�����̍����ʁj
//
// �g�����i1�t���̗���̖ڈ��j
//   1) ���C�����[�v�擪�� Input::Update() ���Ăԁi�O�t���[�������t���[���̃X�i�b�v�V���b�g�j
//   2) WndProc ���痈�� Win32 ���b�Z�[�W�� Input::ProcessMessage(...) �ɑS���n��
//   3) �Q�[���X�V���� Input::GetKey / GetMouseButton / GetMouseDelta �Ȃǂ��Q��
//
// ���ӁF
//   - �{�����͒P��E�B���h�E/�P��X���b�h�O��i���b�Z�[�W�̓��B���ŏ�Ԃ��X�V�����j
//   - �}�E�X���͌Ăяo�����Ɂg�O��ۑ��l�����ݒl�h�̍�����Ԃ��i���� static �ŕێ��j
//   - �z�C�[���� 1.0f �P�ʁi1�m�b�`�j�ɐ��K������ �g�t���[���̍��v�h ��Ԃ�
//   - ���E Ctrl/Alt/Shift ����ʂ��鏈������iWM_* �̊g������X�L�����R�[�h���Q�Ɓj
//------------------------------------------------------------------------------

#include <windowsx.h> // GET_X_LPARAM, GET_Y_LPARAM
#include "Input.h"

// ============================================================================
// �ÓI�����o�i�g���̓f�o�C�X�̌���/�O�t���[����ԁh ��ێ��j
// ============================================================================

// �L�[�{�[�h�F���z�L�[ 0..255 �̉������
bool Input::m_CurrentKeys[256] = { false };  // ���t���[��
bool Input::m_PreviousKeys[256] = { false };  // �O�t���[��

// �}�E�X�F��0/�E1/��2 ��3�{�^����z��
bool Input::m_CurrentMouse[3] = { false };  // ���t���[��
bool Input::m_PreviousMouse[3] = { false };  // �O�t���[��

// �J�[�\�����W�i�E�B���h�E���W�n�j
int  Input::m_MouseX = 0;
int  Input::m_MouseY = 0;

// �}�E�X�z�C�[���i1�t���[���̍��v�����F+��/-���j
float Input::m_MouseWheel = 0.0f;

// ============================================================================
// Initialize�F�S��ԃN���A�i�Q�[���N�����Ɉ�x�Ăׂ�OK�j
// ============================================================================
void Input::Initialize()
{
    ZeroMemory(m_CurrentKeys, sizeof(m_CurrentKeys));
    ZeroMemory(m_PreviousKeys, sizeof(m_PreviousKeys));
    ZeroMemory(m_CurrentMouse, sizeof(m_CurrentMouse));
    ZeroMemory(m_PreviousMouse, sizeof(m_PreviousMouse));
    m_MouseWheel = 0.0f;
}

// ============================================================================
// GetMouseDelta�F���߂̌Ăяo������� (dx, dy) ��Ԃ�
//  - ���� static �� �g�O��Q�Ǝ��̍��W�h ���o���Ă����A���ݒl�Ƃ̍�����Ԃ�
//  - �Ăԕp�x�� 1 �t���[�� 1 �񂪑z��i������ĂԂƍ����������݂ɕ��������j
// ============================================================================
MouseDelta Input::GetMouseDelta()
{
    static int lastX = m_MouseX; // ����� 0
    static int lastY = m_MouseY;

    const int dx = m_MouseX - lastX;
    const int dy = m_MouseY - lastY;

    lastX = m_MouseX;
    lastY = m_MouseY;

    return { dx, dy };
}

float Input::GetMouseDeltaX() { return static_cast<float>(GetMouseDelta().x); }
float Input::GetMouseDeltaY() { return static_cast<float>(GetMouseDelta().y); }

// ============================================================================
// GetMouseScrollDelta�F�t���[�����ŗݐς����z�C�[�������i1.0=1�m�b�`�j
//  - �l�� Update() �`���Ń��Z�b�g����邽�߁u���Y�t���[���̑��ʁv���擾�ł���
// ============================================================================
float Input::GetMouseScrollDelta()
{
    return m_MouseWheel;
}

// ============================================================================
// Update�F�t���[�����E����
//  - �g���ݏ�ԁh �� �g�O�t���[����ԁh �ɑޔ�
//  - �z�C�[�������� 0 �N���A�i���t���[���̐ώZ�̂��߁j
//  - �����C�����[�v�� �g�t���[�����h �ŕK���ĂԂ���
// ============================================================================
void Input::Update()
{
    // ���̃t���[���p�ɐ�Ƀz�C�[�����N���A
    m_MouseWheel = 0.0f;

    // �O�t���[���փX�i�b�v�V���b�g
    memcpy(m_PreviousKeys, m_CurrentKeys, sizeof(m_CurrentKeys));
    memcpy(m_PreviousMouse, m_CurrentMouse, sizeof(m_CurrentMouse));
}

// ============================================================================
// �L�[���́i��������j
//  - GetKey�F������Ă���� true
//  - GetKeyDown�F���t���[���ŉ����ꂽ�u�� true
//  - GetKeyUp�F���t���[���ŗ����ꂽ�u�� true
// ============================================================================
bool Input::GetKey(KeyCode key) { return m_CurrentKeys[(int)key]; }
bool Input::GetKeyDown(KeyCode key) { return  m_CurrentKeys[(int)key] && !m_PreviousKeys[(int)key]; }
bool Input::GetKeyUp(KeyCode key) { return !m_CurrentKeys[(int)key] && m_PreviousKeys[(int)key]; }

// ============================================================================
// �}�E�X�{�^���i����=0, �E=1, ��=2�j
// ============================================================================
bool Input::GetMouseButton(MouseButton b) { return m_CurrentMouse[(int)b]; }
bool Input::GetMouseButtonDown(MouseButton b) { return  m_CurrentMouse[(int)b] && !m_PreviousMouse[(int)b]; }
bool Input::GetMouseButtonUp(MouseButton b) { return !m_CurrentMouse[(int)b] && m_PreviousMouse[(int)b]; }

// ============================================================================
// �}�E�X���W�i�E�B���h�E���W�j
// ============================================================================
int  Input::GetMouseX() { return m_MouseX; }
int  Input::GetMouseY() { return m_MouseY; }

// ============================================================================
// ProcessMessage�FWin32 WndProc ���牡��������
//  - ������ �g���ݏ�ԁh ���X�V����iUpdate �̓t���[�����E�̃X�i�b�v�V���b�g�j
//  - ���E Ctrl/Alt/Shift �̎��ʂɒ��ӁF
//       * Ctrl  �c VK_CONTROL �ł͍��E�s�� �� �g���r�b�g(24bit)�ŉE/���𔻒�
//       * Alt    �c VK_MENU    �����l�i�g���r�b�g�j
//       * Shift  �c VK_SHIFT   �̓X�L�����R�[�h���� MapVirtualKey �Ŏ��L�[�ɕϊ�
// ============================================================================
void Input::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        // -----------------------
        // �L�[�{�[�h����
        // -----------------------
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: // Alt ���̃V�X�e���L�[���܂�
    {
        int vk = static_cast<int>(wParam);

        // ���E Ctrl �����ʁi�g���L�[�Ȃ�E�j
        if (vk == VK_CONTROL)
        {
            const bool ext = (lParam & (1 << 24)) != 0;
            vk = ext ? VK_RCONTROL : VK_LCONTROL;
        }
        // ���E Alt�iVK_MENU�j������
        else if (vk == VK_MENU)
        {
            const bool ext = (lParam & (1 << 24)) != 0;
            vk = ext ? VK_RMENU : VK_LMENU;
        }
        // ���E Shift �����ʁi�X�L�����R�[�h�����z�L�[�g���j
        else if (vk == VK_SHIFT)
        {
            const UINT sc = static_cast<UINT>((lParam >> 16) & 0xFF);
            vk = static_cast<int>(MapVirtualKey(sc, MAPVK_VSC_TO_VK_EX));
        }

        m_CurrentKeys[vk] = true;

#ifdef _DEBUG
        char buf[64]; sprintf_s(buf, "KeyDown: %d\n", vk); OutputDebugStringA(buf);
#endif
        break;
    }

    // -----------------------
    // �L�[�{�[�h����
    // -----------------------
    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        int vk = static_cast<int>(wParam);

        if (vk == VK_CONTROL)
        {
            const bool ext = (lParam & (1 << 24)) != 0;
            vk = ext ? VK_RCONTROL : VK_LCONTROL;
        }
        else if (vk == VK_MENU)
        {
            const bool ext = (lParam & (1 << 24)) != 0;
            vk = ext ? VK_RMENU : VK_LMENU;
        }
        else if (vk == VK_SHIFT)
        {
            const UINT sc = static_cast<UINT>((lParam >> 16) & 0xFF);
            vk = static_cast<int>(MapVirtualKey(sc, MAPVK_VSC_TO_VK_EX));
        }

        m_CurrentKeys[vk] = false;

#ifdef _DEBUG
        char buf[64]; sprintf_s(buf, "KeyUp: %d\n", vk); OutputDebugStringA(buf);
#endif
        break;
    }

    // -----------------------
    // �}�E�X�{�^������/����
    // -----------------------
    case WM_LBUTTONDOWN: m_CurrentMouse[0] = true;  break;
    case WM_LBUTTONUP:   m_CurrentMouse[0] = false; break;
    case WM_RBUTTONDOWN: m_CurrentMouse[1] = true;  break;
    case WM_RBUTTONUP:   m_CurrentMouse[1] = false; break;
    case WM_MBUTTONDOWN: m_CurrentMouse[2] = true;  break;
    case WM_MBUTTONUP:   m_CurrentMouse[2] = false; break;

        // -----------------------
        // �}�E�X�ړ��i�E�B���h�E���W�j
        // -----------------------
    case WM_MOUSEMOVE:
        m_MouseX = GET_X_LPARAM(lParam);
        m_MouseY = GET_Y_LPARAM(lParam);
        break;

        // -----------------------
        // �}�E�X�z�C�[��
        //  - WHEEL_DELTA(=120) �� 1.0f �ɐ��K�����A�t���[���ݐ�
        // -----------------------
    case WM_MOUSEWHEEL:
        m_MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) / float(WHEEL_DELTA);
        break;

    default:
        break;
    }
}
