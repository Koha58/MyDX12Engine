#pragma once
#include <windows.h>        // Win32 API �̃L�[�R�[�h (VK_*) ��`
#include <unordered_map>    // (����͖��g�p���������I�ɃL�[�Ǘ��ɗ��p�ł���)

// ============================================================================
// KeyCode
//  - �v���W�F�N�g���Ŏg���L�[���͂�񋓑̂Œ�`
//  - ���̂� Win32 API �̉��z�L�[�R�[�h (VK_*)
// ============================================================================
enum class KeyCode
{
    W = 'W',          // �A���t�@�x�b�g�L�[�͂��̂܂� ASCII �R�[�h
    A = 'A',
    S = 'S',
    D = 'D',
    Q = 'Q',
    E = 'E',

    Space = VK_SPACE,    // �X�y�[�X�L�[
    Escape = VK_ESCAPE,   // Esc �L�[
    Left = VK_LEFT,     // ��
    Right = VK_RIGHT,    // ��
    Up = VK_UP,       // ��
    Down = VK_DOWN,     // ��
    LeftControl = VK_LCONTROL, // ��Ctrl
    RightControl = VK_RCONTROL, // �ECtrl
};

// ============================================================================
// MouseButton
//  - �}�E�X�{�^�������ʂ���񋓑�
//  - �z��C���f�b�N�X�ƑΉ��i0: �� / 1: �E / 2: �����j
// ============================================================================
enum class MouseButton
{
    Left = 0,
    Right = 1,
    Middle = 2,
};

// ============================================================================
// MouseDelta
//  - �}�E�X�ړ��̍������i�[����\����
//  - ���t���[���O��ʒu�Ƃ̍��� (dx, dy) ��Ԃ�
// ============================================================================
struct MouseDelta
{
    int x;
    int y;
};

// ============================================================================
// Input
//  - �L�[�{�[�h & �}�E�X���͂��Ǘ�����ÓI�N���X
//  - �d�g��
//      �EWin32 ���b�Z�[�W (WndProc) ����L�[/�}�E�X��Ԃ��X�V
//      �EUpdate() �Łu�O�t���[���̏�ԁv��ۑ�
//      �EGetKey / GetKeyDown / GetKeyUp �ŏ�Ԃ�₢���킹
//  - Unity �� Input �N���X�ɋ߂��C���^�[�t�F�C�X
// ============================================================================
class Input
{
public:
    // �������i�z����N���A�j
    static void Initialize();

    // �t���[���X�V�̍Ō�ɌĂ�
    // - ���݂̏�Ԃ� "Previous" �ɃR�s�[����
    static void Update();

    // --------------------------- �L�[�{�[�h���� ---------------------------
    // GetKey      : �������ςȂ�����i���݉�����Ă��邩�j
    // GetKeyDown  : �������u�Ԕ���i�O�t���[���͉�����Ă��Ȃ� & ���݉�����Ă���j
    // GetKeyUp    : �������u�Ԕ���i�O�t���[���͉�����Ă��� & ���݉�����Ă��Ȃ��j
    static bool GetKey(KeyCode key);
    static bool GetKeyDown(KeyCode key);
    static bool GetKeyUp(KeyCode key);

    // --------------------------- �}�E�X���� ---------------------------
    // GetMouseButton     : �{�^���������ςȂ�
    // GetMouseButtonDown : �{�^�����������u��
    // GetMouseButtonUp   : �{�^���𗣂����u��
    static bool GetMouseButton(MouseButton button);
    static bool GetMouseButtonDown(MouseButton button);
    static bool GetMouseButtonUp(MouseButton button);

    // �}�E�X���W�i�X�N���[�����W�n�j
    static int GetMouseX();
    static int GetMouseY();

    // Win32 ���b�Z�[�W����
    // - WndProc ������Ăяo���ē��͏�Ԃ𔽉f����
    // - �L�[�{�[�h�̉���/����A�}�E�X�̃N���b�N/�ړ��Ȃ�
    static void ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);

    // �O�t���[���Ƃ̍��� (dx, dy) ��Ԃ�
    // - CameraController �Ȃǂ̉�]�����ɗ��p
    static MouseDelta GetMouseDelta();

private:
    // --------------------------- �����f�[�^ ---------------------------
    // �L�[�{�[�h���͏��
    static bool m_CurrentKeys[256];   // ���݃t���[���̏��
    static bool m_PreviousKeys[256];  // �O�t���[���̏��

    // �}�E�X���͏��
    static bool m_CurrentMouse[3];    // ���݃t���[���̏�� (Left/Right/Middle)
    static bool m_PreviousMouse[3];   // �O�t���[���̏��

    // �}�E�X���W�i�X�N���[�����W�A�s�N�Z���P�ʁj
    static int m_MouseX;
    static int m_MouseY;
};
