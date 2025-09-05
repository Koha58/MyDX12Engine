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
// デバッグ用コンポーネント
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
// Cube2 移動用コンポーネント
// =============================
class MoveComponent : public Component
{
public:
    MoveComponent(GameObject* owner) : Component(ComponentType::None), m_Owner(owner) {}

    void Update(float deltaTime) override {
        // オーナーが存在し、かつアクティブの場合のみ動作
        if (!m_Owner || !m_Owner->IsActive()) return;

        // フレームカウントに基づくサイン波移動
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
// ウィンドウプロシージャ
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
// キューブメッシュ生成
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
    // ウィンドウ作成
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
    // D3D12Renderer 初期化
    // -----------------------------
    D3D12Renderer renderer;
    renderer.Initialize(hWnd, ClientWidth, ClientHeight);

    // -----------------------------
    // SceneManager と Scene作成
    // -----------------------------
    SceneManager sceneManager;
    auto mainScene = std::make_shared<Scene>("Main Scene");
    sceneManager.AddScene("Main", mainScene);
    sceneManager.SwitchScene("Main");

    MeshData cubeMeshData = CreateCubeMeshData();

    // -----------------------------
    // Cube1 作成
    // -----------------------------
    auto cube1 = std::make_shared<GameObject>("Cube1");
    cube1->Transform->Position = { -2.0f,0.0f,0.0f };
    cube1->AddComponent<TestComponent>();
    auto meshRenderer1 = cube1->AddComponent<MeshRendererComponent>();
    meshRenderer1->SetMesh(cubeMeshData);
    renderer.CreateMeshRendererResources(meshRenderer1);

    // -----------------------------
    // Cube2 作成
    // -----------------------------
    auto cube2 = std::make_shared<GameObject>("Cube2");
    cube2->Transform->Position = { 2.0f,0.0f,0.0f };
    auto meshRenderer2 = cube2->AddComponent<MeshRendererComponent>();
    meshRenderer2->SetMesh(cubeMeshData);
    renderer.CreateMeshRendererResources(meshRenderer2);
    cube2->AddComponent<MoveComponent>(cube2.get()); // MoveComponent追加

    // シーンに追加
    mainScene->AddGameObject(cube1);
    mainScene->AddGameObject(cube2);

    // -----------------------------
    // メインループ
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

            // 2秒ごとに Cube2 の非表示/表示切替
            elapsedTime += deltaTime;
            if (elapsedTime >= 2.0f)
            {
                bool active = cube2->IsActive();
                mainScene->SetGameObjectActive(cube2, !active); // Scene と GameObject の状態を同期
                elapsedTime = 0.0f;
            }

            // アクティブシーン取得
            auto activeScene = sceneManager.GetActiveScene();
            if (activeScene)
            {
                activeScene->Update(deltaTime); // Scene内の全GameObject更新

                // Cube1: Y軸回転
                cube1->Transform->Rotation.y += DirectX::XMConvertToRadians(1.0f);

                renderer.SetScene(activeScene);
                renderer.Render();
            }
        }
    }

    // -----------------------------
    // クリーンアップ
    // -----------------------------
    if (auto activeScene = sceneManager.GetActiveScene())
    {
        activeScene->DestroyAllGameObjects();
    }
    renderer.Cleanup();

    return static_cast<int>(msg.wParam);
}
