#pragma once
#include <windows.h>        // Win32 API のキーコード (VK_*) 定義
#include <unordered_map>    // (今回は未使用だが将来的にキー管理に利用できる)

// ============================================================================
// KeyCode
//  - プロジェクト内で使うキー入力を列挙体で定義
//  - 実体は Win32 API の仮想キーコード (VK_*)
// ============================================================================
enum class KeyCode
{
    W = 'W',          // アルファベットキーはそのまま ASCII コード
    A = 'A',
    S = 'S',
    D = 'D',
    Q = 'Q',
    E = 'E',

    Space = VK_SPACE,    // スペースキー
    Escape = VK_ESCAPE,   // Esc キー
    Left = VK_LEFT,     // ←
    Right = VK_RIGHT,    // →
    Up = VK_UP,       // ↑
    Down = VK_DOWN,     // ↓
    LeftControl = VK_LCONTROL, // 左Ctrl
    RightControl = VK_RCONTROL, // 右Ctrl
};

// ============================================================================
// MouseButton
//  - マウスボタンを識別する列挙体
//  - 配列インデックスと対応（0: 左 / 1: 右 / 2: 中央）
// ============================================================================
enum class MouseButton
{
    Left = 0,
    Right = 1,
    Middle = 2,
};

// ============================================================================
// MouseDelta
//  - マウス移動の差分を格納する構造体
//  - 毎フレーム前回位置との差分 (dx, dy) を返す
// ============================================================================
struct MouseDelta
{
    int x;
    int y;
};

// ============================================================================
// Input
//  - キーボード & マウス入力を管理する静的クラス
//  - 仕組み
//      ・Win32 メッセージ (WndProc) からキー/マウス状態を更新
//      ・Update() で「前フレームの状態」を保存
//      ・GetKey / GetKeyDown / GetKeyUp で状態を問い合わせ
//  - Unity の Input クラスに近いインターフェイス
// ============================================================================
class Input
{
public:
    // 初期化（配列をクリア）
    static void Initialize();

    // フレーム更新の最後に呼ぶ
    // - 現在の状態を "Previous" にコピーする
    static void Update();

    // --------------------------- キーボード入力 ---------------------------
    // GetKey      : 押しっぱなし判定（現在押されているか）
    // GetKeyDown  : 押した瞬間判定（前フレームは押されていない & 現在押されている）
    // GetKeyUp    : 離した瞬間判定（前フレームは押されている & 現在押されていない）
    static bool GetKey(KeyCode key);
    static bool GetKeyDown(KeyCode key);
    static bool GetKeyUp(KeyCode key);

    // --------------------------- マウス入力 ---------------------------
    // GetMouseButton     : ボタン押しっぱなし
    // GetMouseButtonDown : ボタンを押した瞬間
    // GetMouseButtonUp   : ボタンを離した瞬間
    static bool GetMouseButton(MouseButton button);
    static bool GetMouseButtonDown(MouseButton button);
    static bool GetMouseButtonUp(MouseButton button);

    // マウス座標（スクリーン座標系）
    static int GetMouseX();
    static int GetMouseY();

    // Win32 メッセージ処理
    // - WndProc 内から呼び出して入力状態を反映する
    // - キーボードの押下/解放、マウスのクリック/移動など
    static void ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);

    // 前フレームとの差分 (dx, dy) を返す
    // - CameraController などの回転処理に利用
    static MouseDelta GetMouseDelta();

private:
    // --------------------------- 内部データ ---------------------------
    // キーボード入力状態
    static bool m_CurrentKeys[256];   // 現在フレームの状態
    static bool m_PreviousKeys[256];  // 前フレームの状態

    // マウス入力状態
    static bool m_CurrentMouse[3];    // 現在フレームの状態 (Left/Right/Middle)
    static bool m_PreviousMouse[3];   // 前フレームの状態

    // マウス座標（スクリーン座標、ピクセル単位）
    static int m_MouseX;
    static int m_MouseY;
};
