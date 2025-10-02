#include <windowsx.h> // GET_X_LPARAM, GET_Y_LPARAM ���g������
#include "Input.h"

// =============================================================
// �ÓI�����o�ϐ��̒�`
//  - �u���݂̏�ԁv�Ɓu�O�t���[���̏�ԁv��ێ����č������o
// =============================================================

// �L�[�{�[�h���͏��
bool Input::m_CurrentKeys[256] = { false };   // ���t���[���̃L�[���
bool Input::m_PreviousKeys[256] = { false };  // �O�t���[���̃L�[���

// �}�E�X���͏�ԁi��:0, �E:1, ��:2 ��3�{�^���z��j
bool Input::m_CurrentMouse[3] = { false };    // ���t���[���̃}�E�X�{�^��
bool Input::m_PreviousMouse[3] = { false };   // �O�t���[���̃}�E�X�{�^��

// �}�E�X���W�i�E�B���h�E���W�n�j
int Input::m_MouseX = 0;
int Input::m_MouseY = 0;

// �z�C�[������(1�t���[���̉��Z�l)
float Input::m_MouseWheel = 0.0f;

// -------------------------------------------------------------
// ������
//  - �S�L�[�E�}�E�X�{�^����Ԃ��N���A
// -------------------------------------------------------------
void Input::Initialize()
{
    ZeroMemory(m_CurrentKeys, sizeof(m_CurrentKeys));
    ZeroMemory(m_PreviousKeys, sizeof(m_PreviousKeys));
    ZeroMemory(m_CurrentMouse, sizeof(m_CurrentMouse));
    ZeroMemory(m_PreviousMouse, sizeof(m_PreviousMouse));

    m_MouseWheel = 0.0f;
}

// -------------------------------------------------------------
// �}�E�X�ړ��ʂ̎擾
//  - �����Łu�O��ʒu�v�� static �ɕێ����č������v�Z
//  - �߂�l: (dx, dy)
// -------------------------------------------------------------
MouseDelta Input::GetMouseDelta()
{
    static int lastX = m_MouseX;
    static int lastY = m_MouseY;

    int dx = m_MouseX - lastX;
    int dy = m_MouseY - lastY;

    lastX = m_MouseX;
    lastY = m_MouseY;

    return { dx, dy };
}

float Input::GetMouseDeltaX()
{
    return (float)GetMouseDelta().x;
}

float Input::GetMouseDeltaY()
{
    return (float)GetMouseDelta().y;
}

// -------------------------------------------------------------
// �z�C�[�������̎擾(+��/-��)
//  - 1�t���[���ŗݐς����l�����̂܂ܕԂ�
// -------------------------------------------------------------
float Input::GetMouseScrollDelta()
{
    return m_MouseWheel;
}

// -------------------------------------------------------------
// ���t���[���̍X�V
//  - �u���݂̏�ԁv���u�O�t���[����ԁv�ɃR�s�[
//  - ProcessMessage() ���������������ʂ�ۑ��������
//  - �z�C�[�������̓t���[�����Ń��Z�b�g
// -------------------------------------------------------------
void Input::Update()
{
    // ���̃t���[���̏����Ŏg���؂邽�߁A���0�N���A
    m_MouseWheel = 0.0f;

    memcpy(m_PreviousKeys, m_CurrentKeys, sizeof(m_CurrentKeys));
    memcpy(m_PreviousMouse, m_CurrentMouse, sizeof(m_CurrentMouse));
}

// -------------------------------------------------------------
// �L�[���͔���
// -------------------------------------------------------------

// ������Ă���� true
bool Input::GetKey(KeyCode key)
{
    return m_CurrentKeys[(int)key];
}

// �����ꂽ�u�� true �i���t���[��������Ă��āA�O�t���[���͉�����Ă��Ȃ��j
bool Input::GetKeyDown(KeyCode key)
{
    return m_CurrentKeys[(int)key] && !m_PreviousKeys[(int)key];
}

// �����ꂽ�u�� true �i���t���[��������Ă��Ȃ��āA�O�t���[���͉�����Ă����j
bool Input::GetKeyUp(KeyCode key)
{
    return !m_CurrentKeys[(int)key] && m_PreviousKeys[(int)key];
}

// -------------------------------------------------------------
// �}�E�X�{�^�����͔���
// -------------------------------------------------------------

bool Input::GetMouseButton(MouseButton button)
{
    return m_CurrentMouse[(int)button];
}

bool Input::GetMouseButtonDown(MouseButton button)
{
    return m_CurrentMouse[(int)button] && !m_PreviousMouse[(int)button];
}

bool Input::GetMouseButtonUp(MouseButton button)
{
    return !m_CurrentMouse[(int)button] && m_PreviousMouse[(int)button];
}

// -------------------------------------------------------------
// �}�E�X���W�̎擾
// -------------------------------------------------------------
int Input::GetMouseX()
{
    return m_MouseX;
}

int Input::GetMouseY()
{
    return m_MouseY;
}

// -------------------------------------------------------------
// Win32 ���b�Z�[�W����
//  - Win32 �E�B���h�E�v���V�[�W������Ăяo�����z��
//  - �e����̓C�x���g�������Ԃɔ��f
// -------------------------------------------------------------
void Input::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        // --- �L�[�{�[�h���� ---
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: // Alt �n�ȂǃV�X�e���L�[
    {
        int vk = (int)wParam; // ���z�L�[�R�[�h

        // ���E Ctrl �̔���i�ʏ� VK_CONTROL �ŋ�ʂ����Ȃ����ߊg���r�b�g������j
        if (vk == VK_CONTROL)
        {
            bool ext = (lParam & (1 << 24)) != 0; // 24bit �������Ă���ΉE
            vk = ext ? VK_RCONTROL : VK_LCONTROL;
        }
        // ���EAlt������
        else if(vk == VK_MENU)
        {
            bool ext = (lParam & (1 << 24)) != 0;
            vk = ext ? VK_RMENU : VK_LMENU;
        }
        // ���EShift������
        else if (vk == VK_SHIFT)
        {
            UINT sc = (UINT)((lParam >> 16) & 0xFF);
            vk = (int)MapVirtualKey(sc, MAPVK_VSC_TO_VK_EX);
        }

        m_CurrentKeys[vk] = true; // ������Ԃɂ���

        // �f�o�b�O���O�o��
        char buf[64];
        sprintf_s(buf, "KeyDown: %d\n", vk);
        OutputDebugStringA(buf);
        break;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        int vk = (int)wParam;

        if (vk == VK_CONTROL)
        {
            bool ext = (lParam & (1 << 24)) != 0;
            vk = ext ? VK_RCONTROL : VK_LCONTROL;
        }
        // ���EAlt
        else if (vk == VK_MENU)
        {
            bool ext = (lParam & (1 << 24)) != 0;
            vk = ext ? VK_RMENU : VK_LMENU;
        }
        // ���EShift
        else if (vk == VK_SHIFT)
        {
            UINT sc = (UINT)((lParam >> 16) & 0xFF);
            vk = (int)MapVirtualKey(sc, MAPVK_VSC_TO_VK_EX);
        }

        m_CurrentKeys[vk] = false; // �����ꂽ��Ԃɂ���

        char buf[64];
        sprintf_s(buf, "KeyUp: %d\n", vk);
        OutputDebugStringA(buf);
        break;
    }

        // --- �}�E�X�{�^�� ---
    case WM_LBUTTONDOWN: m_CurrentMouse[0] = true; break;
    case WM_LBUTTONUP:   m_CurrentMouse[0] = false; break;
    case WM_RBUTTONDOWN: m_CurrentMouse[1] = true; break;
    case WM_RBUTTONUP:   m_CurrentMouse[1] = false; break;
    case WM_MBUTTONDOWN: m_CurrentMouse[2] = true; break;
    case WM_MBUTTONUP:   m_CurrentMouse[2] = false; break;

        // --- �}�E�X�ړ� ---
    case WM_MOUSEMOVE:
        m_MouseX = GET_X_LPARAM(lParam); // �E�B���h�E����X���W
        m_MouseY = GET_Y_LPARAM(lParam); // �E�B���h�E����Y���W
        break;

        // --- �z�C�[�� ---
    case WM_MOUSEWHEEL:
        // 1�m�b�`=120�� +1/-1�̍����ɐ��K�����ėݐ�
        m_MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) / float(WHEEL_DELTA);
        break;

    }
}
