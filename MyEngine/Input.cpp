#include <windowsx.h> // GET_X_LPARAM, GET_Y_LPARAM ‚ðŽg‚¤‚½‚ß
#include "Input.h"

bool Input::m_CurrentKeys[256] = { false };
bool Input::m_PreviousKeys[256] = { false };

bool Input::m_CurrentMouse[3] = { false };
bool Input::m_PreviousMouse[3] = { false };

int Input::m_MouseX = 0;
int Input::m_MouseY = 0;

void Input::Initialize()
{
    ZeroMemory(m_CurrentKeys, sizeof(m_CurrentKeys));
    ZeroMemory(m_PreviousKeys, sizeof(m_PreviousKeys));
    ZeroMemory(m_CurrentMouse, sizeof(m_CurrentMouse));
    ZeroMemory(m_PreviousMouse, sizeof(m_PreviousMouse));
}

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

void Input::Update()
{
    memcpy(m_PreviousKeys, m_CurrentKeys, sizeof(m_CurrentKeys));
    memcpy(m_PreviousMouse, m_CurrentMouse, sizeof(m_CurrentMouse));
}


bool Input::GetKey(KeyCode key)
{
    return m_CurrentKeys[(int)key];
}

bool Input::GetKeyDown(KeyCode key)
{
    return m_CurrentKeys[(int)key] && !m_PreviousKeys[(int)key];
}

bool Input::GetKeyUp(KeyCode key)
{
    return !m_CurrentKeys[(int)key] && m_PreviousKeys[(int)key];
}

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

int Input::GetMouseX()
{
    return m_MouseX;
}

int Input::GetMouseY()
{
    return m_MouseY;
}

void Input::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        int vk = (int)wParam;

        // Ctrl ”»’è
        if (vk == VK_CONTROL)
        {
            bool isExtended = (lParam & (1 << 24)) != 0; // 24bit‚Å‰E‚©‚Ç‚¤‚©”»’è
            vk = isExtended ? VK_RCONTROL : VK_LCONTROL;
        }

        m_CurrentKeys[vk] = true;

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
            bool isExtended = (lParam & (1 << 24)) != 0;
            vk = isExtended ? VK_RCONTROL : VK_LCONTROL;
        }

        m_CurrentKeys[vk] = false;

        char buf[64];
        sprintf_s(buf, "KeyUp: %d\n", vk);
        OutputDebugStringA(buf);
        break;
    }

    case WM_LBUTTONDOWN: m_CurrentMouse[0] = true; break;
    case WM_LBUTTONUP:   m_CurrentMouse[0] = false; break;
    case WM_RBUTTONDOWN: m_CurrentMouse[1] = true; break;
    case WM_RBUTTONUP:   m_CurrentMouse[1] = false; break;
    case WM_MBUTTONDOWN: m_CurrentMouse[2] = true; break;
    case WM_MBUTTONUP:   m_CurrentMouse[2] = false; break;

    case WM_MOUSEMOVE:
        m_MouseX = GET_X_LPARAM(lParam);
        m_MouseY = GET_Y_LPARAM(lParam);
        break;
    }
}


