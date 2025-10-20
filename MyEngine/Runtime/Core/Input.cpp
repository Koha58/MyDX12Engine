// Input.cpp
//------------------------------------------------------------------------------
// 役割：Win32 メッセージから “フレーム単位の入力状態” を組み立てて提供する。
//       - キー/マウスボタンの現在/前フレーム状態（押下・押し始め・離し）
//       - マウス座標（ウィンドウ座標）と移動量（Δ）
//       - マウスホイール（フレーム中の差分量）
//
// 使い方（1フレの流れの目安）
//   1) メインループ先頭で Input::Update() を呼ぶ（前フレーム→今フレームのスナップショット）
//   2) WndProc から来る Win32 メッセージを Input::ProcessMessage(...) に全部渡す
//   3) ゲーム更新時に Input::GetKey / GetMouseButton / GetMouseDelta などを参照
//
// 注意：
//   - 本実装は単一ウィンドウ/単一スレッド前提（メッセージの到達順で状態が更新される）
//   - マウスΔは呼び出し時に“前回保存値→現在値”の差分を返す（内部 static で保持）
//   - ホイールは 1.0f 単位（1ノッチ）に正規化して “フレームの合計” を返す
//   - 左右 Ctrl/Alt/Shift を区別する処理あり（WM_* の拡張情報やスキャンコードを参照）
//------------------------------------------------------------------------------

#include <windowsx.h> // GET_X_LPARAM, GET_Y_LPARAM
#include "Input.h"

// ============================================================================
// 静的メンバ（“入力デバイスの現在/前フレーム状態” を保持）
// ============================================================================

// キーボード：仮想キー 0..255 の押下状態
bool Input::m_CurrentKeys[256] = { false };  // 今フレーム
bool Input::m_PreviousKeys[256] = { false };  // 前フレーム

// マウス：左0/右1/中2 の3ボタンを想定
bool Input::m_CurrentMouse[3] = { false };  // 今フレーム
bool Input::m_PreviousMouse[3] = { false };  // 前フレーム

// カーソル座標（ウィンドウ座標系）
int  Input::m_MouseX = 0;
int  Input::m_MouseY = 0;

// マウスホイール（1フレームの合計差分：+上/-下）
float Input::m_MouseWheel = 0.0f;

// ============================================================================
// Initialize：全状態クリア（ゲーム起動時に一度呼べばOK）
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
// GetMouseDelta：直近の呼び出しからの (dx, dy) を返す
//  - 内部 static に “前回参照時の座標” を覚えておき、現在値との差分を返す
//  - 呼ぶ頻度は 1 フレーム 1 回が想定（複数回呼ぶと差分が小刻みに分割される）
// ============================================================================
MouseDelta Input::GetMouseDelta()
{
    static int lastX = m_MouseX; // 初回は 0
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
// GetMouseScrollDelta：フレーム内で累積したホイール差分（1.0=1ノッチ）
//  - 値は Update() 冒頭でリセットされるため「当該フレームの総量」が取得できる
// ============================================================================
float Input::GetMouseScrollDelta()
{
    return m_MouseWheel;
}

// ============================================================================
// Update：フレーム境界処理
//  - “現在状態” を “前フレーム状態” に退避
//  - ホイール差分を 0 クリア（次フレームの積算のため）
//  - ※メインループの “フレーム頭” で必ず呼ぶこと
// ============================================================================
void Input::Update()
{
    // 次のフレーム用に先にホイールをクリア
    m_MouseWheel = 0.0f;

    // 前フレームへスナップショット
    memcpy(m_PreviousKeys, m_CurrentKeys, sizeof(m_CurrentKeys));
    memcpy(m_PreviousMouse, m_CurrentMouse, sizeof(m_CurrentMouse));
}

// ============================================================================
// キー入力（押下判定）
//  - GetKey：押されている間 true
//  - GetKeyDown：今フレームで押された瞬間 true
//  - GetKeyUp：今フレームで離された瞬間 true
// ============================================================================
bool Input::GetKey(KeyCode key) { return m_CurrentKeys[(int)key]; }
bool Input::GetKeyDown(KeyCode key) { return  m_CurrentKeys[(int)key] && !m_PreviousKeys[(int)key]; }
bool Input::GetKeyUp(KeyCode key) { return !m_CurrentKeys[(int)key] && m_PreviousKeys[(int)key]; }

// ============================================================================
// マウスボタン（※左=0, 右=1, 中=2）
// ============================================================================
bool Input::GetMouseButton(MouseButton b) { return m_CurrentMouse[(int)b]; }
bool Input::GetMouseButtonDown(MouseButton b) { return  m_CurrentMouse[(int)b] && !m_PreviousMouse[(int)b]; }
bool Input::GetMouseButtonUp(MouseButton b) { return !m_CurrentMouse[(int)b] && m_PreviousMouse[(int)b]; }

// ============================================================================
// マウス座標（ウィンドウ座標）
// ============================================================================
int  Input::GetMouseX() { return m_MouseX; }
int  Input::GetMouseY() { return m_MouseY; }

// ============================================================================
// ProcessMessage：Win32 WndProc から横流しする
//  - ここで “現在状態” を更新する（Update はフレーム境界のスナップショット）
//  - 左右 Ctrl/Alt/Shift の識別に注意：
//       * Ctrl  … VK_CONTROL では左右不明 → 拡張ビット(24bit)で右/左を判定
//       * Alt    … VK_MENU    も同様（拡張ビット）
//       * Shift  … VK_SHIFT   はスキャンコードから MapVirtualKey で実キーに変換
// ============================================================================
void Input::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        // -----------------------
        // キーボード押下
        // -----------------------
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: // Alt 等のシステムキーも含む
    {
        int vk = static_cast<int>(wParam);

        // 左右 Ctrl を識別（拡張キーなら右）
        if (vk == VK_CONTROL)
        {
            const bool ext = (lParam & (1 << 24)) != 0;
            vk = ext ? VK_RCONTROL : VK_LCONTROL;
        }
        // 左右 Alt（VK_MENU）を識別
        else if (vk == VK_MENU)
        {
            const bool ext = (lParam & (1 << 24)) != 0;
            vk = ext ? VK_RMENU : VK_LMENU;
        }
        // 左右 Shift を識別（スキャンコード→仮想キー拡張）
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
    // キーボード離し
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
    // マウスボタン押下/離し
    // -----------------------
    case WM_LBUTTONDOWN: m_CurrentMouse[0] = true;  break;
    case WM_LBUTTONUP:   m_CurrentMouse[0] = false; break;
    case WM_RBUTTONDOWN: m_CurrentMouse[1] = true;  break;
    case WM_RBUTTONUP:   m_CurrentMouse[1] = false; break;
    case WM_MBUTTONDOWN: m_CurrentMouse[2] = true;  break;
    case WM_MBUTTONUP:   m_CurrentMouse[2] = false; break;

        // -----------------------
        // マウス移動（ウィンドウ座標）
        // -----------------------
    case WM_MOUSEMOVE:
        m_MouseX = GET_X_LPARAM(lParam);
        m_MouseY = GET_Y_LPARAM(lParam);
        break;

        // -----------------------
        // マウスホイール
        //  - WHEEL_DELTA(=120) を 1.0f に正規化し、フレーム累積
        // -----------------------
    case WM_MOUSEWHEEL:
        m_MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) / float(WHEEL_DELTA);
        break;

    default:
        break;
    }
}
