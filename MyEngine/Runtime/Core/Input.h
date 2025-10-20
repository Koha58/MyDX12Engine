#pragma once
#include <windows.h>        // Win32 API のキーコード (VK_*) 定義
#include <unordered_map>    // （今は未使用。将来的にキー名→KeyCode 変換などで活用可）

// ============================================================================
// KeyCode
// ----------------------------------------------------------------------------
// ・プロジェクト全体で使うキー入力の列挙体（抽象化）
// ・実体値は Win32 の仮想キーコード (VK_*) に合わせているため、
//   WndProc で受け取る WPARAM（VK_*）をそのまま内部配列の添字に使える。
// ・左右の Ctrl/Shift/Alt は区別した値を持つ。
//   → 区別しない判定をしたい場合は、呼び出し側で || するか、
//      ユーティリティ関数を後日追加する想定。
// ============================================================================
enum class KeyCode
{
    W = 'W',          // アルファベットキーは ASCII コードと一致
    A = 'A',
    S = 'S',
    D = 'D',
    Q = 'Q',
    E = 'E',
    F = 'F',

    Space = VK_SPACE,     // スペース
    Escape = VK_ESCAPE,    // Esc
    Left = VK_LEFT,      // ←
    Right = VK_RIGHT,     // →
    Up = VK_UP,        // ↑
    Down = VK_DOWN,      // ↓

    // 修飾キー（左右を区別）
    LeftControl = VK_LCONTROL,
    RightControl = VK_RCONTROL,
    LeftShift = VK_LSHIFT,
    RightShift = VK_RSHIFT,
    LeftAlt = VK_LMENU,   // Alt（= VK_MENU）
    RightAlt = VK_RMENU,
};

// ============================================================================
// MouseButton
// ----------------------------------------------------------------------------
// ・マウスボタンの列挙体。内部配列の添字にそのまま使用。
//   0: 左, 1: 右, 2: 中
// ============================================================================
enum class MouseButton
{
    Left = 0,
    Right = 1,
    Middle = 2,
};

// ============================================================================
// MouseDelta
// ----------------------------------------------------------------------------
// ・マウス移動の差分を保持する小さな構造体。
// ・GetMouseDelta() は毎回「直前にこの関数を呼んだ時点」からの差分を返す
//   実装（内部 static で前回位置を保持）になっている点に注意。
//   → 毎フレーム 1 回だけ呼び出す運用を推奨（複数回呼ぶと 0 になりやすい）。
// ============================================================================
struct MouseDelta
{
    int x;
    int y;
};

// ============================================================================
// Input（静的クラス）
// ----------------------------------------------------------------------------
// 役割：
//  - Win32 メッセージ（WndProc）から押下/解放/座標/ホイールを取り込み、
//    フレーム単位で参照しやすい API（GetKey/GetKeyDown/GetKeyUp など）を提供。
// 使い方（最小フロー）：
//  1) アプリ開始時に Input::Initialize() を 1 度呼ぶ。
//  2) WndProc で届くメッセージを「最初に」Input::ProcessMessage() へ渡す。
//     * ImGui_ImplWin32_WndProcHandler を先に通す場合でも、ここへも必ず通すこと。
//  3) フレーム末尾で Input::Update() を 1 度呼ぶ（前フレーム状態の確定）。
//  4) ゲーム側はフレーム中に GetKey/Down/Up, GetMouseDelta(), GetMouseScrollDelta() を参照。
// 注意点：
//  - GetKeyDown/GetKeyUp は Update() 呼び出しで前フレーム状態が更新されることで機能する。
//    → 1 フレームにつき必ず 1 回、かつフレームの最後に Update() を呼ぶこと。
//  - GetMouseDelta() は「呼んだタイミングからの差分」なので、フレーム中に複数回呼ぶと
//    差分が分割される。フレーム中 1 回に統一するか、呼び出し箇所を一本化すること。
//  - ホイールは 1 ノッチ = +1/-1 に正規化して累積し、Update() で 0 に戻す運用。
//  - スレッドセーフではない（主スレッドで UI/入力/更新を回す前提）。
// ============================================================================
class Input
{
public:
    // 初期化：内部配列・ホイール量をクリア。
    // アプリ起動時に 1 度だけ呼ぶ。
    static void Initialize();

    // フレーム末尾で呼ぶ：現在状態を「前フレーム」へコピーし、
    // ホイール差分を 0 にリセットする。
    // これを呼ばないと GetKeyDown/GetKeyUp が正しく機能しない。
    static void Update();

    // --------------------------- キーボード入力 ---------------------------
    // GetKey      : 押しっぱなし（現在押されている）なら true
    // GetKeyDown  : 押した瞬間（今フレ true & 前フレ false）なら true
    // GetKeyUp    : 離した瞬間（今フレ false & 前フレ true）なら true
    static bool GetKey(KeyCode key);
    static bool GetKeyDown(KeyCode key);
    static bool GetKeyUp(KeyCode key);

    // --------------------------- マウスボタン ---------------------------
    static bool GetMouseButton(MouseButton button);
    static bool GetMouseButtonDown(MouseButton button);
    static bool GetMouseButtonUp(MouseButton button);

    // --------------------------- マウス位置/差分/ホイール ---------------------------
    // 位置：WndProc の WM_MOUSEMOVE で更新される「ウィンドウ座標系」の値。
    // DPI スケールとは無関係にピクセル単位。必要なら呼び出し側で補正する。
    static int   GetMouseX();
    static int   GetMouseY();

    // 差分：直前に GetMouseDelta() を呼んだ時からの移動量（dx,dy）。
    // フレーム中に何度も呼ばないこと（0 になりやすい）。
    static MouseDelta GetMouseDelta();
    static float      GetMouseDeltaX(); // X 成分だけ欲しい場合のショートカット
    static float      GetMouseDeltaY(); // Y 成分だけ欲しい場合のショートカット

    // ホイール差分：1 ノッチ = ±1 に正規化し、フレーム中の累積値を返す。
    // Update() で 0 にリセットされる（フレーム末尾に読む前提）。
    static float GetMouseScrollDelta();

    // --------------------------- Win32 メッセージ反映 ---------------------------
    // ・WndProc 内で必ず呼ぶこと。ImGui のハンドラを先に通した場合でも、ここにも通す。
    // ・処理対象（抜粋）：
    //   - WM_KEYDOWN / WM_SYSKEYDOWN   : m_CurrentKeys[vk] = true
    //     * VK_CONTROL/VK_MENU/VK_SHIFT は左右区別に変換してから格納する。
    //       - Ctrl : lParam の拡張ビット(24bit)で右判定（true=右）。
    //       - Alt  : VK_MENU として来る → 同様に拡張ビットで左右に振り分け。
    //       - Shift: スキャンコード→MapVirtualKey で左右を特定。
    //   - WM_KEYUP / WM_SYSKEYUP       : m_CurrentKeys[vk] = false
    //   - WM_LBUTTON*/WM_RBUTTON*/WM_MBUTTON* : m_CurrentMouse[] を更新
    //   - WM_MOUSEMOVE                 : m_MouseX/Y を更新（ウィンドウ座標）
    //   - WM_MOUSEWHEEL                : m_MouseWheel += delta/120（1 ノッチ→±1）
    static void ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);

private:
    // --------------------------- 内部データ ---------------------------
    // キーボード状態（VK_* をそのまま添字に使用）
    static bool m_CurrentKeys[256];   // 現在フレーム
    static bool m_PreviousKeys[256];  // 前フレーム

    // マウスボタン状態（Left/Right/Middle = 0/1/2）
    static bool m_CurrentMouse[3];    // 現在フレーム
    static bool m_PreviousMouse[3];   // 前フレーム

    // マウス座標（ウィンドウクライアント座標・ピクセル）
    static int  m_MouseX;
    static int  m_MouseY;

    // ホイール差分（フレーム中に加算、Update で 0 クリア）
    static float m_MouseWheel;
};

/* ---------------------------------------------------------------------------
【運用のヒント / よくある落とし穴】

- Update() の呼び忘れ：
  GetKeyDown/Up が常に false になったり、押しっぱなしの判定が崩れる。
  → 毎フレーム末尾で必ず Input::Update() を 1 回呼ぶ。

- GetMouseDelta() の連続呼び：
  差分が「前回呼んだ時点から」になるため、複数回呼ぶと差分が分割される。
  → 呼び出し箇所を 1 箇所にまとめる（例えばカメラ操作の直前だけ）。

- 左右 Ctrl/Alt/Shift の判定：
  WM_* の WPARAM は VK_CONTROL/VK_MENU/VK_SHIFT としか来ない場合がある。
  → lParam の拡張ビットやスキャンコードから左右を割り分けている（ProcessMessage 内）。
     区別不要なら Left/Right の両方を || すればよい。

- DPI/スケーリング：
  GetMouseX/Y は「ウィンドウのクライアント座標（ピクセル）」。
  必要なら呼び出し側で DPI に応じた変換を実施する。

- ImGui との共存：
  ImGui がマウス/キーボードをキャプチャしていても、生の入力はここで記録される。
  実際に使うかどうか（カメラ操作を受け付けるか）は、EditorInterop などの
  「Scene ビューがホバー/フォーカス中か？」のフラグで制御するのが安全。
--------------------------------------------------------------------------- */
