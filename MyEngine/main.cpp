#include <windows.h>
#include <string>
#include <memory>
#include <iostream>

// SAL�A�m�e�[�V�����̂��߂ɕK�v�ɂȂ�ꍇ������܂�
#include <sal.h> // ���ǉ�

#include "D3D12Renderer.h"
#include "GameObject.h"
#include "Mesh.h"

// �E�B���h�E�v���V�[�W��
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Cube�̒��_�ƃC���f�b�N�X�f�[�^���`
MeshData CreateCubeMeshData()
{
    MeshData meshData;
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

// �� WinMain �֐��̃V�O�l�`���� SAL �A�m�e�[�V������ǉ�
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
)
{
    // �E�B���h�E�N���X�̓o�^
    const wchar_t CLASS_NAME[] = L"D3D12WindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClass(&wc))
    {
        MessageBox(nullptr, L"�E�B���h�E�N���X�̓o�^�Ɏ��s���܂����I", L"�G���[", MB_OK | MB_ICONERROR);
        return 1;
    }

    // �E�B���h�E�̃T�C�Y�ƃX�^�C���̐ݒ�
    RECT windowRect = { 0, 0, 800, 600 };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    const UINT ClientWidth = windowRect.right - windowRect.left;
    const UINT ClientHeight = windowRect.bottom - windowRect.top;

    // �E�B���h�E�̍쐬
    HWND hWnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"DirectX 12 Engine",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        ClientWidth, ClientHeight,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (hWnd == nullptr)
    {
        MessageBox(nullptr, L"�E�B���h�E�̍쐬�Ɏ��s���܂����I", L"�G���[", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // D3D12Renderer �̏�����
    D3D12Renderer renderer;
    if (!renderer.Initialize(hWnd, ClientWidth, ClientHeight))
    {
        MessageBox(nullptr, L"D3D12Renderer �̏������Ɏ��s���܂����B", L"�G���[", MB_OK | MB_ICONERROR);
        return 1;
    }

    // --- �V�[����GameObject�̍쐬 ---
    std::shared_ptr<Scene> mainScene = std::make_shared<Scene>("Main Scene");

    // �L���[�u�̃��b�V���f�[�^���쐬
    MeshData cubeMeshData = CreateCubeMeshData();

    // �ŏ��̃L���[�uGameObject
    std::shared_ptr<GameObject> cube1 = std::make_shared<GameObject>("Cube1");
    // ���ɑ傫���ړ������āA2�̃L���[�u�������邩�m�F
    cube1->Transform->Position = DirectX::XMFLOAT3(-2.0f, 0.0f, 0.0f);
    std::shared_ptr<MeshRendererComponent> meshRenderer1 = cube1->AddComponent<MeshRendererComponent>();
    meshRenderer1->SetMesh(cubeMeshData); // CPU���̃��b�V���f�[�^��ݒ�
    // D3D12���\�[�X��GPU�ɃA�b�v���[�h
    if (!renderer.CreateMeshRendererResources(meshRenderer1)) {
        MessageBox(nullptr, L"Cube1 �̃��b�V�����\�[�X�쐬�Ɏ��s���܂����B", L"�G���[", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 2�Ԗڂ̃L���[�uGameObject (�قȂ�ʒu)
    std::shared_ptr<GameObject> cube2 = std::make_shared<GameObject>("Cube2");
    // �E�ɑ傫���ړ������āA2�̃L���[�u�������邩�m�F
    cube2->Transform->Position = DirectX::XMFLOAT3(2.0f, 0.0f, 0.0f);
    std::shared_ptr<MeshRendererComponent> meshRenderer2 = cube2->AddComponent<MeshRendererComponent>();
    meshRenderer2->SetMesh(cubeMeshData);
    if (!renderer.CreateMeshRendererResources(meshRenderer2)) {
        MessageBox(nullptr, L"Cube2 �̃��b�V�����\�[�X�쐬�Ɏ��s���܂����B", L"�G���[", MB_OK | MB_ICONERROR);
        return 1;
    }

    // �V�[����GameObject��ǉ�
    mainScene->AddGameObject(cube1);
    mainScene->AddGameObject(cube2);

    // Renderer�ɃV�[����ݒ�
    renderer.SetScene(mainScene);

    // �Q�[�����[�v
    MSG msg = { 0 };
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // ���ԍX�V (�ȈՓI)
            // ���ۂɂ͍����x�^�C�}�[���g�p���ׂ�
            float deltaTime = 1.0f / 60.0f; // 60fps�z��

            // �V�[���̍X�V
            mainScene->Update(deltaTime);

            // Cube1����]������ (��)
            cube1->Transform->Rotation.y += DirectX::XMConvertToRadians(1.0f); // ���t���[��1�x��]
            // Cube2���ړ������� (��)
            cube2->Transform->Position.z = sin(renderer.GetFrameCount() * 0.05f) * 2.0f;

            // �����_�����O
            renderer.Render();
        }
    }

    // �N���[���A�b�v
    renderer.Cleanup();

    return static_cast<int>(msg.wParam);
}