#pragma once
#include <windows.h>
#include <unordered_map>

enum class KeyCode
{
    W = 'W',
    A = 'A',
    S = 'S',
    D = 'D',
    Q = 'Q',
    E = 'E',
    Space = VK_SPACE,
    Escape = VK_ESCAPE,
    Left = VK_LEFT,
    Right = VK_RIGHT,
    Up = VK_UP,
    Down = VK_DOWN,
    LeftControl = VK_LCONTROL,
    RightControl = VK_RCONTROL,
};

enum class MouseButton
{
    Left = 0,
    Right = 1,
    Middle = 2,
};

struct MouseDelta
{
    int x;
    int y;
};

class Input
{
public:
    static void Initialize();
    static void Update();

    // キー入力
    static bool GetKey(KeyCode key);
    static bool GetKeyDown(KeyCode key);
    static bool GetKeyUp(KeyCode key);

    // マウス入力
    static bool GetMouseButton(MouseButton button);
    static bool GetMouseButtonDown(MouseButton button);
    static bool GetMouseButtonUp(MouseButton button);

    static int GetMouseX();
    static int GetMouseY();

    // Win32 メッセージを処理（WndProcから呼ぶ）
    static void ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);

    static MouseDelta GetMouseDelta(); // 前フレームとの差分を返す

private:
    static bool m_CurrentKeys[256];
    static bool m_PreviousKeys[256];

    static bool m_CurrentMouse[3];
    static bool m_PreviousMouse[3];

    static int m_MouseX;
    static int m_MouseY;
};
