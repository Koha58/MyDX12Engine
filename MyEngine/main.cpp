#include <windows.h> // Windows API�̊�{�@�\
#include <string>    // std::string���g�p
#include <memory>    // std::shared_ptr���g�p
#include <iostream>  // ���o�̓X�g���[���p�i�f�o�b�O�ړI�Ȃǁj

// SAL (Source Code Annotation Language) �A�m�e�[�V�����̂��߂ɕK�v
// �R���p�C���ɂ��R�[�h��͂��������A���ݓI�ȃo�O����肷��̂ɖ𗧂�
#include <sal.h> 

#include "D3D12Renderer.h" // D3D12�`�揈�����Ǘ�����N���X�̃w�b�_�[
#include "GameObject.h"    // �Q�[�����̃I�u�W�F�N�g��\������N���X�̃w�b�_�[
#include "Mesh.h"          // ���b�V���f�[�^�\�����`����w�b�_�[
#include "Scene.h" 

// �E�B���h�E�v���V�[�W��
// Windows���b�Z�[�W���������邽�߂̃R�[���o�b�N�֐�
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY: // �E�B���h�E���j�������Ƃ��ɔ���
        PostQuitMessage(0); // �A�v���P�[�V�����I�����b�Z�[�W�𓊍e
        break;
    default: // ���̑��̃��b�Z�[�W�̓f�t�H���g�̃E�B���h�E�v���V�[�W���ɔC����
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0; // ���b�Z�[�W�������������Ƃ�����
}

// �L���[�u�̒��_�ƃC���f�b�N�X�f�[�^�𐶐�����֐�
MeshData CreateCubeMeshData()
{
    MeshData meshData;
    // ���_�f�[�^�̐ݒ�: �ʒu (x, y, z) �ƐF (r, g, b, a)
    meshData.Vertices = {
        // Front face (Red Green Blue Yellow)
        { {-0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f, 1.0f} }, // 0: Front Top Left
        { { 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.0f} }, // 1: Front Top Right
        { {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f, 1.0f} }, // 2: Front Bottom Left
        { { 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f, 1.0f} }, // 3: Front Bottom Right

        // Back face (Cyan Magenta Black White)
        { {-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 1.0f, 1.0f} }, // 4: Back Top Left
        { { 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 1.0f, 1.0f} }, // 5: Back Top Right
        { {-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f, 1.0f} }, // 6: Back Bottom Left
        { { 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 1.0f, 1.0f} }, // 7: Back Bottom Right
    };

    // �C���f�b�N�X�f�[�^�̐ݒ�
    // �e�ʂ�2�̎O�p�`�ō\�����A�e���_�C���f�b�N�X���w��
    meshData.Indices = {
        // Front face (0,1,2,3)
        0, 1, 2,
        1, 3, 2,

        // Back face (4,5,6,7)
        4, 6, 5,
        5, 6, 7,

        // Top face (0,1,4,5)
        4, 5, 0,
        5, 1, 0,

        // Bottom face (2,3,6,7)
        2, 3, 6,
        3, 7, 6,

        // Right face (1,3,5,7)
        1, 5, 3,
        5, 7, 3,

        // Left face (0,2,4,6)
        4, 0, 6,
        0, 2, 6,
    };
    return meshData;
}

// WinMain �֐�: Windows�A�v���P�[�V�����̃G���g���[�|�C���g
// SAL�A�m�e�[�V�����������̎g�p���@���L�q���A�R�[�h�i�������コ����
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,       // �A�v���P�[�V�����̌��݂̃C���X�^���X�ւ̃n���h��
    _In_opt_ HINSTANCE hPrevInstance, // �A�v���P�[�V�����̑O�̃C���X�^���X�ւ̃n���h���i���NULL�j
    _In_ LPSTR lpCmdLine,           // �R�}���h���C��������
    _In_ int nCmdShow               // �E�B���h�E�̕\�����@�������t���O
)
{
    // �E�B���h�E�N���X���̒�`
    const wchar_t CLASS_NAME[] = L"D3D12WindowClass";

    // �E�B���h�E�N���X�\���̂̐ݒ�
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;         // �E�B���h�E�v���V�[�W����ݒ�
    wc.hInstance = hInstance;         // �C���X�^���X�n���h��
    wc.lpszClassName = CLASS_NAME;    // �N���X��
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); // �f�t�H���g�J�[�\��
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // �f�t�H���g�w�i�F

    // �E�B���h�E�N���X�̓o�^
    if (!RegisterClass(&wc))
    {
        MessageBox(nullptr, L"�E�B���h�E�N���X�̓o�^�Ɏ��s���܂����I", L"�G���[", MB_OK | MB_ICONERROR);
        return 1; // ���s���̓G���[�R�[�h��Ԃ�
    }

    // �E�B���h�E�̏����T�C�Y�ƃX�^�C���̐ݒ�
    RECT windowRect = { 0, 0, 800, 600 };
    // �N���C�A���g�̈�̃T�C�Y�Ɋ�Â��ăE�B���h�E�̎��ۂ̃T�C�Y���v�Z
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // �v�Z���ꂽ�N���C�A���g�̈�̕��ƍ������擾
    const UINT ClientWidth = windowRect.right - windowRect.left;
    const UINT ClientHeight = windowRect.bottom - windowRect.top;

    // �E�B���h�E�̍쐬
    HWND hWnd = CreateWindowEx(
        0,                            // �g���X�^�C��
        CLASS_NAME,                   // �o�^�ς݂̃E�B���h�E�N���X��
        L"DirectX 12 Engine",         // �E�B���h�E�̃^�C�g���o�[�ɕ\�������e�L�X�g
        WS_OVERLAPPEDWINDOW,          // �E�B���h�E�̃X�^�C��
        CW_USEDEFAULT, CW_USEDEFAULT, // �����ʒu (�V�X�e���ɔC����)
        ClientWidth, ClientHeight,    // �����T�C�Y
        nullptr,                      // �e�E�B���h�E�̃n���h��
        nullptr,                      // ���j���[�n���h���܂��͎q�E�B���h�EID
        hInstance,                    // �A�v���P�[�V�����C���X�^���X�̃n���h��
        nullptr                       // �A�v���P�[�V������`�f�[�^�ւ̃|�C���^
    );

    // �E�B���h�E�쐬�̎��s�`�F�b�N
    if (hWnd == nullptr)
    {
        MessageBox(nullptr, L"�E�B���h�E�̍쐬�Ɏ��s���܂����I", L"�G���[", MB_OK | MB_ICONERROR);
        return 1; // ���s���̓G���[�R�[�h��Ԃ�
    }

    // �E�B���h�E��\�����A�X�V
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // D3D12Renderer �I�u�W�F�N�g�̏�����
    D3D12Renderer renderer;
    if (!renderer.Initialize(hWnd, ClientWidth, ClientHeight))
    {
        MessageBox(nullptr, L"D3D12Renderer �̏������Ɏ��s���܂����B", L"�G���[", MB_OK | MB_ICONERROR);
        return 1; // ���s���̓G���[�R�[�h��Ԃ�
    }

    // --- �V�[����GameObject�̍쐬 ---
    // ���C���V�[�����쐬
    std::shared_ptr<Scene> mainScene = std::make_shared<Scene>("Main Scene");

    // �L���[�u�̃��b�V���f�[�^�𐶐�
    MeshData cubeMeshData = CreateCubeMeshData();

    // �ŏ��̃L���[�uGameObject���쐬
    std::shared_ptr<GameObject> cube1 = std::make_shared<GameObject>("Cube1");
    // ���ɑ傫���ړ������āA2�̃L���[�u�������邩�m�F
    cube1->Transform->Position = DirectX::XMFLOAT3(-2.0f, 0.0f, 0.0f);
    // ���b�V�������_���[�R���|�[�l���g��ǉ�
    std::shared_ptr<MeshRendererComponent> meshRenderer1 = cube1->AddComponent<MeshRendererComponent>();
    meshRenderer1->SetMesh(cubeMeshData); // CPU���̃��b�V���f�[�^��ݒ�
    // D3D12���\�[�X��GPU�ɃA�b�v���[�h
    if (!renderer.CreateMeshRendererResources(meshRenderer1)) {
        MessageBox(nullptr, L"Cube1 �̃��b�V�����\�[�X�쐬�Ɏ��s���܂����B", L"�G���[", MB_OK | MB_ICONERROR);
        return 1; // ���s���̓G���[�R�[�h��Ԃ�
    }

    // 2�Ԗڂ̃L���[�uGameObject���쐬 (�قȂ�ʒu)
    std::shared_ptr<GameObject> cube2 = std::make_shared<GameObject>("Cube2");
    // �E�ɑ傫���ړ������āA2�̃L���[�u�������邩�m�F
    cube2->Transform->Position = DirectX::XMFLOAT3(2.0f, 0.0f, 0.0f);
    // ���b�V�������_���[�R���|�[�l���g��ǉ�
    std::shared_ptr<MeshRendererComponent> meshRenderer2 = cube2->AddComponent<MeshRendererComponent>();
    meshRenderer2->SetMesh(cubeMeshData);
    // D3D12���\�[�X��GPU�ɃA�b�v���[�h
    if (!renderer.CreateMeshRendererResources(meshRenderer2)) {
        MessageBox(nullptr, L"Cube2 �̃��b�V�����\�[�X�쐬�Ɏ��s���܂����B", L"�G���[", MB_OK | MB_ICONERROR);
        return 1; // ���s���̓G���[�R�[�h��Ԃ�
    }

    // �V�[����GameObject��ǉ�
    mainScene->AddGameObject(cube1);
    mainScene->AddGameObject(cube2);

    // �����_���[�Ƀ��C���V�[����ݒ�
    renderer.SetScene(mainScene);

    // �Q�[�����[�v
    MSG msg = { 0 }; // ���b�Z�[�W�\���̂�������
    while (WM_QUIT != msg.message) // WM_QUIT���b�Z�[�W������܂Ń��[�v�𑱂���
    {
        // ���b�Z�[�W�L���[�Ƀ��b�Z�[�W�����邩�`�F�b�N���A����Ύ��o��
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg); // �L�[�{�[�h���b�Z�[�W��ϊ�
            DispatchMessage(&msg);  // �E�B���h�E�v���V�[�W���Ƀ��b�Z�[�W���f�B�X�p�b�`
        }
        else // ���b�Z�[�W���Ȃ��ꍇ�A�Q�[�����W�b�N�ƃ����_�����O�����s
        {
            // ���ԍX�V (�ȈՓI��deltaTime�A���ۂɂ͍����x�^�C�}�[���g�p���ׂ�)
            float deltaTime = 1.0f / 60.0f; // 60fps��z�肵���Œ�f���^�^�C��

            // �V�[���̍X�V���W�b�N
            mainScene->Update(deltaTime);

            // Cube1����]�������
            cube1->Transform->Rotation.y += DirectX::XMConvertToRadians(1.0f); // ���t���[��Y�������1�x��]

            // Cube2��Z����ŃT�C���g��Ɉړ��������
            cube2->Transform->Position.z = sin(renderer.GetFrameCount() * 0.05f) * 2.0f;

            // �V�[���̃����_�����O
            renderer.Render();
        }
    }

    // �A�v���P�[�V�����I�����̃N���[���A�b�v����
    renderer.Cleanup();

    // �A�v���P�[�V�����̏I���R�[�h��Ԃ�
    return static_cast<int>(msg.wParam);
}