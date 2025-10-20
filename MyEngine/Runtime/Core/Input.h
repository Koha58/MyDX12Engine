#pragma once
#include <windows.h>        // Win32 API �̃L�[�R�[�h (VK_*) ��`
#include <unordered_map>    // �i���͖��g�p�B�����I�ɃL�[����KeyCode �ϊ��ȂǂŊ��p�j

// ============================================================================
// KeyCode
// ----------------------------------------------------------------------------
// �E�v���W�F�N�g�S�̂Ŏg���L�[���̗͂񋓑́i���ۉ��j
// �E���̒l�� Win32 �̉��z�L�[�R�[�h (VK_*) �ɍ��킹�Ă��邽�߁A
//   WndProc �Ŏ󂯎�� WPARAM�iVK_*�j�����̂܂ܓ����z��̓Y���Ɏg����B
// �E���E�� Ctrl/Shift/Alt �͋�ʂ����l�����B
//   �� ��ʂ��Ȃ�������������ꍇ�́A�Ăяo������ || ���邩�A
//      ���[�e�B���e�B�֐�������ǉ�����z��B
// ============================================================================
enum class KeyCode
{
    W = 'W',          // �A���t�@�x�b�g�L�[�� ASCII �R�[�h�ƈ�v
    A = 'A',
    S = 'S',
    D = 'D',
    Q = 'Q',
    E = 'E',
    F = 'F',

    Space = VK_SPACE,     // �X�y�[�X
    Escape = VK_ESCAPE,    // Esc
    Left = VK_LEFT,      // ��
    Right = VK_RIGHT,     // ��
    Up = VK_UP,        // ��
    Down = VK_DOWN,      // ��

    // �C���L�[�i���E����ʁj
    LeftControl = VK_LCONTROL,
    RightControl = VK_RCONTROL,
    LeftShift = VK_LSHIFT,
    RightShift = VK_RSHIFT,
    LeftAlt = VK_LMENU,   // Alt�i= VK_MENU�j
    RightAlt = VK_RMENU,
};

// ============================================================================
// MouseButton
// ----------------------------------------------------------------------------
// �E�}�E�X�{�^���̗񋓑́B�����z��̓Y���ɂ��̂܂܎g�p�B
//   0: ��, 1: �E, 2: ��
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
// �E�}�E�X�ړ��̍�����ێ����鏬���ȍ\���́B
// �EGetMouseDelta() �͖���u���O�ɂ��̊֐����Ă񂾎��_�v����̍�����Ԃ�
//   �����i���� static �őO��ʒu��ێ��j�ɂȂ��Ă���_�ɒ��ӁB
//   �� ���t���[�� 1 �񂾂��Ăяo���^�p�𐄏��i������ĂԂ� 0 �ɂȂ�₷���j�B
// ============================================================================
struct MouseDelta
{
    int x;
    int y;
};

// ============================================================================
// Input�i�ÓI�N���X�j
// ----------------------------------------------------------------------------
// �����F
//  - Win32 ���b�Z�[�W�iWndProc�j���牟��/���/���W/�z�C�[������荞�݁A
//    �t���[���P�ʂŎQ�Ƃ��₷�� API�iGetKey/GetKeyDown/GetKeyUp �Ȃǁj��񋟁B
// �g�����i�ŏ��t���[�j�F
//  1) �A�v���J�n���� Input::Initialize() �� 1 �x�ĂԁB
//  2) WndProc �œ͂����b�Z�[�W���u�ŏ��ɁvInput::ProcessMessage() �֓n���B
//     * ImGui_ImplWin32_WndProcHandler ���ɒʂ��ꍇ�ł��A�����ւ��K���ʂ����ƁB
//  3) �t���[�������� Input::Update() �� 1 �x�Ăԁi�O�t���[����Ԃ̊m��j�B
//  4) �Q�[�����̓t���[������ GetKey/Down/Up, GetMouseDelta(), GetMouseScrollDelta() ���Q�ƁB
// ���ӓ_�F
//  - GetKeyDown/GetKeyUp �� Update() �Ăяo���őO�t���[����Ԃ��X�V����邱�Ƃŋ@�\����B
//    �� 1 �t���[���ɂ��K�� 1 ��A���t���[���̍Ō�� Update() ���ĂԂ��ƁB
//  - GetMouseDelta() �́u�Ă񂾃^�C�~���O����̍����v�Ȃ̂ŁA�t���[�����ɕ�����ĂԂ�
//    ���������������B�t���[���� 1 ��ɓ��ꂷ�邩�A�Ăяo���ӏ�����{�����邱�ƁB
//  - �z�C�[���� 1 �m�b�` = +1/-1 �ɐ��K�����ėݐς��AUpdate() �� 0 �ɖ߂��^�p�B
//  - �X���b�h�Z�[�t�ł͂Ȃ��i��X���b�h�� UI/����/�X�V���񂷑O��j�B
// ============================================================================
class Input
{
public:
    // �������F�����z��E�z�C�[���ʂ��N���A�B
    // �A�v���N������ 1 �x�����ĂԁB
    static void Initialize();

    // �t���[�������ŌĂԁF���ݏ�Ԃ��u�O�t���[���v�փR�s�[���A
    // �z�C�[�������� 0 �Ƀ��Z�b�g����B
    // ������Ă΂Ȃ��� GetKeyDown/GetKeyUp ���������@�\���Ȃ��B
    static void Update();

    // --------------------------- �L�[�{�[�h���� ---------------------------
    // GetKey      : �������ςȂ��i���݉�����Ă���j�Ȃ� true
    // GetKeyDown  : �������u�ԁi���t�� true & �O�t�� false�j�Ȃ� true
    // GetKeyUp    : �������u�ԁi���t�� false & �O�t�� true�j�Ȃ� true
    static bool GetKey(KeyCode key);
    static bool GetKeyDown(KeyCode key);
    static bool GetKeyUp(KeyCode key);

    // --------------------------- �}�E�X�{�^�� ---------------------------
    static bool GetMouseButton(MouseButton button);
    static bool GetMouseButtonDown(MouseButton button);
    static bool GetMouseButtonUp(MouseButton button);

    // --------------------------- �}�E�X�ʒu/����/�z�C�[�� ---------------------------
    // �ʒu�FWndProc �� WM_MOUSEMOVE �ōX�V�����u�E�B���h�E���W�n�v�̒l�B
    // DPI �X�P�[���Ƃ͖��֌W�Ƀs�N�Z���P�ʁB�K�v�Ȃ�Ăяo�����ŕ␳����B
    static int   GetMouseX();
    static int   GetMouseY();

    // �����F���O�� GetMouseDelta() ���Ă񂾎�����̈ړ��ʁidx,dy�j�B
    // �t���[�����ɉ��x���Ă΂Ȃ����Ɓi0 �ɂȂ�₷���j�B
    static MouseDelta GetMouseDelta();
    static float      GetMouseDeltaX(); // X ���������~�����ꍇ�̃V���[�g�J�b�g
    static float      GetMouseDeltaY(); // Y ���������~�����ꍇ�̃V���[�g�J�b�g

    // �z�C�[�������F1 �m�b�` = �}1 �ɐ��K�����A�t���[�����̗ݐϒl��Ԃ��B
    // Update() �� 0 �Ƀ��Z�b�g�����i�t���[�������ɓǂޑO��j�B
    static float GetMouseScrollDelta();

    // --------------------------- Win32 ���b�Z�[�W���f ---------------------------
    // �EWndProc ���ŕK���ĂԂ��ƁBImGui �̃n���h�����ɒʂ����ꍇ�ł��A�����ɂ��ʂ��B
    // �E�����Ώہi�����j�F
    //   - WM_KEYDOWN / WM_SYSKEYDOWN   : m_CurrentKeys[vk] = true
    //     * VK_CONTROL/VK_MENU/VK_SHIFT �͍��E��ʂɕϊ����Ă���i�[����B
    //       - Ctrl : lParam �̊g���r�b�g(24bit)�ŉE����itrue=�E�j�B
    //       - Alt  : VK_MENU �Ƃ��ė��� �� ���l�Ɋg���r�b�g�ō��E�ɐU�蕪���B
    //       - Shift: �X�L�����R�[�h��MapVirtualKey �ō��E�����B
    //   - WM_KEYUP / WM_SYSKEYUP       : m_CurrentKeys[vk] = false
    //   - WM_LBUTTON*/WM_RBUTTON*/WM_MBUTTON* : m_CurrentMouse[] ���X�V
    //   - WM_MOUSEMOVE                 : m_MouseX/Y ���X�V�i�E�B���h�E���W�j
    //   - WM_MOUSEWHEEL                : m_MouseWheel += delta/120�i1 �m�b�`���}1�j
    static void ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);

private:
    // --------------------------- �����f�[�^ ---------------------------
    // �L�[�{�[�h��ԁiVK_* �����̂܂ܓY���Ɏg�p�j
    static bool m_CurrentKeys[256];   // ���݃t���[��
    static bool m_PreviousKeys[256];  // �O�t���[��

    // �}�E�X�{�^����ԁiLeft/Right/Middle = 0/1/2�j
    static bool m_CurrentMouse[3];    // ���݃t���[��
    static bool m_PreviousMouse[3];   // �O�t���[��

    // �}�E�X���W�i�E�B���h�E�N���C�A���g���W�E�s�N�Z���j
    static int  m_MouseX;
    static int  m_MouseY;

    // �z�C�[�������i�t���[�����ɉ��Z�AUpdate �� 0 �N���A�j
    static float m_MouseWheel;
};

/* ---------------------------------------------------------------------------
�y�^�p�̃q���g / �悭���闎�Ƃ����z

- Update() �̌ĂіY��F
  GetKeyDown/Up ����� false �ɂȂ�����A�������ςȂ��̔��肪�����B
  �� ���t���[�������ŕK�� Input::Update() �� 1 ��ĂԁB

- GetMouseDelta() �̘A���ĂсF
  �������u�O��Ă񂾎��_����v�ɂȂ邽�߁A������ĂԂƍ��������������B
  �� �Ăяo���ӏ��� 1 �ӏ��ɂ܂Ƃ߂�i�Ⴆ�΃J��������̒��O�����j�B

- ���E Ctrl/Alt/Shift �̔���F
  WM_* �� WPARAM �� VK_CONTROL/VK_MENU/VK_SHIFT �Ƃ������Ȃ��ꍇ������B
  �� lParam �̊g���r�b�g��X�L�����R�[�h���獶�E�����蕪���Ă���iProcessMessage ���j�B
     ��ʕs�v�Ȃ� Left/Right �̗����� || ����΂悢�B

- DPI/�X�P�[�����O�F
  GetMouseX/Y �́u�E�B���h�E�̃N���C�A���g���W�i�s�N�Z���j�v�B
  �K�v�Ȃ�Ăяo������ DPI �ɉ������ϊ������{����B

- ImGui �Ƃ̋����F
  ImGui ���}�E�X/�L�[�{�[�h���L���v�`�����Ă��Ă��A���̓��͂͂����ŋL�^�����B
  ���ۂɎg�����ǂ����i�J����������󂯕t���邩�j�́AEditorInterop �Ȃǂ�
  �uScene �r���[���z�o�[/�t�H�[�J�X�����H�v�̃t���O�Ő��䂷��̂����S�B
--------------------------------------------------------------------------- */
