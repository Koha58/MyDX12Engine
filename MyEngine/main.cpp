#include <windows.h>
#include <string>
#include <memory>
#include <iostream>
#include <sal.h>
#include <cmath>

#include "D3D12Renderer.h"
#include "GameObject.h"
#include "Mesh.h"
#include "Scene.h"
#include "SceneManager.h"

// =============================
// �f�o�b�O�p�R���|�[�l���g
// =============================
class TestComponent : public Component
{
public:
    TestComponent() : Component(ComponentType::None) {}

    void OnEnable() override {
        OutputDebugStringA("TestComponent: OnEnable\n");
    }

    void OnDisable() override {
        OutputDebugStringA("TestComponent: OnDisable\n");
    }

    void OnDestroy() override {
        MessageBoxA(nullptr, "TestComponent: OnDestroy called!", "Debug", MB_OK);
    }
};

// =============================
// Cube2 �ړ��p�R���|�[�l���g
// =============================
class MoveComponent : public Component
{
public:
    MoveComponent(GameObject* owner) : Component(ComponentType::None), m_Owner(owner) {}

    void Update(float deltaTime) override {
        // �I�[�i�[�����݂��A���A�N�e�B�u�̏ꍇ�̂ݓ���
        if (!m_Owner || !m_Owner->IsActive()) return;

        // �t���[���J�E���g�Ɋ�Â��T�C���g�ړ�
        m_Owner->Transform->Position.z = sin(m_FrameCount * 0.05f) * 2.0f;
        m_FrameCount++;
    }

    void OnEnable() override {
        OutputDebugStringA("MoveComponent: OnEnable\n");
    }

    void OnDisable() override {
        OutputDebugStringA("MoveComponent: OnDisable\n");
    }

private:
    GameObject* m_Owner = nullptr;
    int m_FrameCount = 0;
};

// =============================
// �E�B���h�E�v���V�[�W��
// =============================
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

// =============================
// �L���[�u���b�V������
// =============================
MeshData CreateCubeMeshData()
{
    MeshData meshData;
    meshData.Vertices = {
        {{-0.5f, 0.5f,-0.5f},{1,0,0,1}},
        {{ 0.5f, 0.5f,-0.5f},{0,1,0,1}},
        {{-0.5f,-0.5f,-0.5f},{0,0,1,1}},
        {{ 0.5f,-0.5f,-0.5f},{1,1,0,1}},
        {{-0.5f, 0.5f, 0.5f},{0,1,1,1}},
        {{ 0.5f, 0.5f, 0.5f},{1,0,1,1}},
        {{-0.5f,-0.5f, 0.5f},{0,0,0,1}},
        {{ 0.5f,-0.5f, 0.5f},{1,1,1,1}},
    };
    meshData.Indices = {
        0,1,2,1,3,2, 4,6,5,5,6,7,
        4,5,0,5,1,0, 2,3,6,3,7,6,
        1,5,3,5,7,3, 4,0,6,0,2,6
    };
    return meshData;
}

// =============================
// WinMain
// =============================
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int nCmdShow)
{
    // -----------------------------
    // �E�B���h�E�쐬
    // -----------------------------
    const wchar_t CLASS_NAME[] = L"D3D12WindowClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    RECT windowRect = { 0,0,800,600 };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    const UINT ClientWidth = windowRect.right - windowRect.left;
    const UINT ClientHeight = windowRect.bottom - windowRect.top;

    HWND hWnd = CreateWindowEx(0, CLASS_NAME, L"DirectX12 Engine",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        ClientWidth, ClientHeight,
        nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // -----------------------------
    // D3D12Renderer ������
    // -----------------------------
    D3D12Renderer renderer;
    renderer.Initialize(hWnd, ClientWidth, ClientHeight);

    // -----------------------------
    // SceneManager �� Scene�쐬
    // -----------------------------
    SceneManager sceneManager;
    auto mainScene = std::make_shared<Scene>("Main Scene");
    sceneManager.AddScene("Main", mainScene);
    sceneManager.SwitchScene("Main");

    MeshData cubeMeshData = CreateCubeMeshData();

    // -----------------------------
    // Cube1 �쐬
    // -----------------------------
    auto cube1 = std::make_shared<GameObject>("Cube1");
    cube1->Transform->Position = { -2.0f,0.0f,0.0f };
    cube1->AddComponent<TestComponent>();
    auto meshRenderer1 = cube1->AddComponent<MeshRendererComponent>();
    meshRenderer1->SetMesh(cubeMeshData);
    renderer.CreateMeshRendererResources(meshRenderer1);

    // -----------------------------
    // Cube2 �쐬
    // -----------------------------
    auto cube2 = std::make_shared<GameObject>("Cube2");
    cube2->Transform->Position = { 2.0f,0.0f,0.0f };
    auto meshRenderer2 = cube2->AddComponent<MeshRendererComponent>();
    meshRenderer2->SetMesh(cubeMeshData);
    renderer.CreateMeshRendererResources(meshRenderer2);
    cube2->AddComponent<MoveComponent>(cube2.get()); // MoveComponent�ǉ�

    // �V�[���ɒǉ�
    mainScene->AddGameObject(cube1);
    mainScene->AddGameObject(cube2);

    // -----------------------------
    // ���C�����[�v
    // -----------------------------
    MSG msg = { 0 };
    float elapsedTime = 0.0f;

    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            float deltaTime = 1.0f / 60.0f;
            elapsedTime += deltaTime;

            // 2�b���Ƃ� Cube2 �̔�\��/�\���ؑ�
            elapsedTime += deltaTime;
            if (elapsedTime >= 2.0f)
            {
                bool active = cube2->IsActive();
                mainScene->SetGameObjectActive(cube2, !active); // Scene �� GameObject �̏�Ԃ𓯊�
                elapsedTime = 0.0f;
            }

            // �A�N�e�B�u�V�[���擾
            auto activeScene = sceneManager.GetActiveScene();
            if (activeScene)
            {
                activeScene->Update(deltaTime); // Scene���̑SGameObject�X�V

                // Cube1: Y����]
                cube1->Transform->Rotation.y += DirectX::XMConvertToRadians(1.0f);

                renderer.SetScene(activeScene);
                renderer.Render();
            }
        }
    }

    // -----------------------------
    // �N���[���A�b�v
    // -----------------------------
    if (auto activeScene = sceneManager.GetActiveScene())
    {
        activeScene->DestroyAllGameObjects();
    }
    renderer.Cleanup();

    return static_cast<int>(msg.wParam);
}
