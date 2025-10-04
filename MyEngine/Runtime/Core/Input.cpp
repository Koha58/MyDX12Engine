#include <windowsx.h> // GET_X_LPARAM, GET_Y_LPARAM を使うため
#include "Input.h"

// =============================================================
// 静的メンバ変数の定義
//  - 「現在の状態」と「前フレームの状態」を保持して差分検出
// =============================================================

// キーボード入力状態
bool Input::m_CurrentKeys[256] = { false };   // 今フレームのキー状態
bool Input::m_PreviousKeys[256] = { false };  // 前フレームのキー状態

// マウス入力状態（左:0, 右:1, 中:2 の3ボタン想定）
bool Input::m_CurrentMouse[3] = { false };    // 今フレームのマウスボタン
bool Input::m_PreviousMouse[3] = { false };   // 前フレームのマウスボタン

// マウス座標（ウィンドウ座標系）
int Input::m_MouseX = 0;
int Input::m_MouseY = 0;

// ホイール差分(1フレームの加算値)
float Input::m_MouseWheel = 0.0f;

// -------------------------------------------------------------
// 初期化
//  - 全キー・マウスボタン状態をクリア
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
// マウス移動量の取得
//  - 内部で「前回位置」を static に保持して差分を計算
//  - 戻り値: (dx, dy)
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
// ホイール差分の取得(+上/-下)
//  - 1フレームで累積した値をそのまま返す
// -------------------------------------------------------------
float Input::GetMouseScrollDelta()
{
    return m_MouseWheel;
}

// -------------------------------------------------------------
// 毎フレームの更新
//  - 「現在の状態」を「前フレーム状態」にコピー
//  - ProcessMessage() が書き換えた結果を保存する役割
//  - ホイール差分はフレーム頭でリセット
// -------------------------------------------------------------
void Input::Update()
{
    // このフレームの処理で使い切るため、先に0クリア
    m_MouseWheel = 0.0f;

    memcpy(m_PreviousKeys, m_CurrentKeys, sizeof(m_CurrentKeys));
    memcpy(m_PreviousMouse, m_CurrentMouse, sizeof(m_CurrentMouse));
}

// -------------------------------------------------------------
// キー入力判定
// -------------------------------------------------------------

// 押されている間 true
bool Input::GetKey(KeyCode key)
{
    return m_CurrentKeys[(int)key];
}

// 押された瞬間 true （今フレーム押されていて、前フレームは押されていない）
bool Input::GetKeyDown(KeyCode key)
{
    return m_CurrentKeys[(int)key] && !m_PreviousKeys[(int)key];
}

// 離された瞬間 true （今フレーム押されていなくて、前フレームは押されていた）
bool Input::GetKeyUp(KeyCode key)
{
    return !m_CurrentKeys[(int)key] && m_PreviousKeys[(int)key];
}

// -------------------------------------------------------------
// マウスボタン入力判定
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
// マウス座標の取得
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
// Win32 メッセージ処理
//  - Win32 ウィンドウプロシージャから呼び出される想定
//  - 各種入力イベントを内部状態に反映
// -------------------------------------------------------------
void Input::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        // --- キーボード入力 ---
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: // Alt 系などシステムキー
    {
        int vk = (int)wParam; // 仮想キーコード

        // 左右 Ctrl の判定（通常 VK_CONTROL で区別がつかないため拡張ビットを見る）
        if (vk == VK_CONTROL)
        {
            bool ext = (lParam & (1 << 24)) != 0; // 24bit が立っていれば右
            vk = ext ? VK_RCONTROL : VK_LCONTROL;
        }
        // 左右Altを識別
        else if(vk == VK_MENU)
        {
            bool ext = (lParam & (1 << 24)) != 0;
            vk = ext ? VK_RMENU : VK_LMENU;
        }
        // 左右Shiftを識別
        else if (vk == VK_SHIFT)
        {
            UINT sc = (UINT)((lParam >> 16) & 0xFF);
            vk = (int)MapVirtualKey(sc, MAPVK_VSC_TO_VK_EX);
        }

        m_CurrentKeys[vk] = true; // 押下状態にする

        // デバッグログ出力
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
        // 左右Alt
        else if (vk == VK_MENU)
        {
            bool ext = (lParam & (1 << 24)) != 0;
            vk = ext ? VK_RMENU : VK_LMENU;
        }
        // 左右Shift
        else if (vk == VK_SHIFT)
        {
            UINT sc = (UINT)((lParam >> 16) & 0xFF);
            vk = (int)MapVirtualKey(sc, MAPVK_VSC_TO_VK_EX);
        }

        m_CurrentKeys[vk] = false; // 離された状態にする

        char buf[64];
        sprintf_s(buf, "KeyUp: %d\n", vk);
        OutputDebugStringA(buf);
        break;
    }

        // --- マウスボタン ---
    case WM_LBUTTONDOWN: m_CurrentMouse[0] = true; break;
    case WM_LBUTTONUP:   m_CurrentMouse[0] = false; break;
    case WM_RBUTTONDOWN: m_CurrentMouse[1] = true; break;
    case WM_RBUTTONUP:   m_CurrentMouse[1] = false; break;
    case WM_MBUTTONDOWN: m_CurrentMouse[2] = true; break;
    case WM_MBUTTONUP:   m_CurrentMouse[2] = false; break;

        // --- マウス移動 ---
    case WM_MOUSEMOVE:
        m_MouseX = GET_X_LPARAM(lParam); // ウィンドウ内のX座標
        m_MouseY = GET_Y_LPARAM(lParam); // ウィンドウ内のY座標
        break;

        // --- ホイール ---
    case WM_MOUSEWHEEL:
        // 1ノッチ=120→ +1/-1の差分に正規化して累積
        m_MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) / float(WHEEL_DELTA);
        break;

    }
}
