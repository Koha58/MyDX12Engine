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
#include "Time.h"
#include "Input.h"
#include "CameraComponent.h"
#include "CameraControllerComponent.h"

// =============================
// デバッグ用コンポーネント
// =============================
// GameObject にアタッチして、ライフサイクルイベント
// （OnEnable/OnDisable/OnDestroy）が正しく呼ばれるかを確認するためのサンプル。
class TestComponent : public Component
{
public:
    TestComponent() : Component(ComponentType::None) {}

    // コンポーネントが有効化された直後に呼ばれる
    void OnEnable() override {
        OutputDebugStringA("TestComponent: OnEnable\n"); // Visual Studio の出力ウィンドウにログ
    }

    // コンポーネントが無効化された直後に呼ばれる
    void OnDisable() override {
        OutputDebugStringA("TestComponent: OnDisable\n");
    }

    // コンポーネントが破棄される直前に呼ばれる
    void OnDestroy() override {
        // OnDestroy が呼ばれていることを分かりやすく確認するためにダイアログを表示
        MessageBoxA(nullptr, "TestComponent: OnDestroy called!", "Debug", MB_OK);
    }
};

// =============================
// Cube2 移動用コンポーネント
// =============================
// サイン波関数を使ってオブジェクトを前後に動かすコンポーネント。
// 「移動」などの挙動はコンポーネント化することで、再利用性や管理がしやすくなる。
class MoveComponent : public Component
{
public:
    MoveComponent(GameObject* owner) : Component(ComponentType::None), m_Owner(owner) {}

    // 毎フレーム呼ばれる更新処理
    void Update(float deltaTime) override {
        // GameObject が存在し、かつアクティブ状態のときだけ動作させる
        if (!m_Owner || !m_Owner->IsActive()) return;

        // sin 波を利用して z 座標を周期的に変化させる
        // m_FrameCount に応じて値が増えるので、結果として往復運動になる
        m_Owner->Transform->Position.z = sin(m_FrameCount * 0.05f) * 2.0f;

        // フレームごとにカウントを進める
        m_FrameCount++;
    }

    // 有効化された直後
    void OnEnable() override {
        OutputDebugStringA("MoveComponent: OnEnable\n");
    }

    // 無効化された直後
    void OnDisable() override {
        OutputDebugStringA("MoveComponent: OnDisable\n");
    }

private:
    GameObject* m_Owner = nullptr; // このコンポーネントを所有する GameObject への参照
    int m_FrameCount = 0;          // フレームカウンタ（動きの周期を作るために使用）
};

// =============================
// ウィンドウプロシージャ
// =============================
// Windows が投げてくるメッセージ（ウィンドウが閉じられた、入力があった等）を処理する関数。
// 今回は最低限、WM_DESTROY だけを処理している。
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Input システムに通知
    Input::ProcessMessage(message, wParam, lParam);

    switch (message)
    {
    case WM_DESTROY:
        // ウィンドウが閉じられたとき、アプリケーション終了を指示する
        PostQuitMessage(0);
        break;
    default:
        // その他のメッセージは既定処理に渡す
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// =============================
// キューブメッシュ生成
// =============================
// 頂点カラーを持つ単純な立方体を生成。
// 実際のゲームでは FBX など外部データを使うが、ここでは手書きで定義している。
MeshData CreateCubeMeshData()
{
    MeshData meshData;

    // 立方体の頂点（位置 + 法線 + RGBA色）
    meshData.Vertices = {
        // -Z 面
        {{-0.5f,  0.5f, -0.5f}, {0,0,-1}, {1,0,0,1}}, // 赤
        {{ 0.5f,  0.5f, -0.5f}, {0,0,-1}, {0,1,0,1}}, // 緑
        {{-0.5f, -0.5f, -0.5f}, {0,0,-1}, {0,0,1,1}}, // 青
        {{ 0.5f, -0.5f, -0.5f}, {0,0,-1}, {1,1,0,1}}, // 黄

        // +Z 面
        {{-0.5f,  0.5f,  0.5f}, {0,0,1}, {0,1,1,1}},  // シアン
        {{ 0.5f,  0.5f,  0.5f}, {0,0,1}, {1,0,1,1}},  // マゼンタ
        {{-0.5f, -0.5f,  0.5f}, {0,0,1}, {0,0,0,1}},  // 黒
        {{ 0.5f, -0.5f,  0.5f}, {0,0,1}, {1,1,1,1}},  // 白
    };

    // インデックス（今まで通りでOK）
    meshData.Indices = {
        0,1,2, 1,3,2,   // -Z 面
        4,6,5, 5,6,7,   // +Z 面
        4,5,0, 5,1,0,   // +Y 面
        2,3,6, 3,7,6,   // -Y 面
        1,5,3, 5,7,3,   // +X 面
        4,0,6, 0,2,6    // -X 面
    };

    return meshData;
}


// =============================
// WinMain (アプリケーションのエントリーポイント)
// =============================
// Windows アプリとして最初に呼ばれる関数。
// ウィンドウ作成 → DirectX 初期化 → ゲームループ → 終了処理 の流れ。
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int nCmdShow)
{
    // -----------------------------
    // 1. ウィンドウクラスの登録
    // -----------------------------
    const wchar_t CLASS_NAME[] = L"D3D12WindowClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;           // このウィンドウに届くメッセージを処理する関数
    wc.hInstance = hInstance;           // アプリケーションのインスタンス
    wc.lpszClassName = CLASS_NAME;      // 識別用のクラス名
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); // カーソル形状
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // 背景色
    RegisterClass(&wc);

    // -----------------------------
    // 2. ウィンドウ作成
    // -----------------------------
    RECT windowRect = { 0,0,800,600 };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE); // 枠も含めたウィンドウサイズに調整
    const UINT ClientWidth = windowRect.right - windowRect.left;
    const UINT ClientHeight = windowRect.bottom - windowRect.top;

    HWND hWnd = CreateWindowEx(
        0, CLASS_NAME, L"DirectX12 Engine",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        ClientWidth, ClientHeight,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // -----------------------------
    // 3. DirectX12Renderer 初期化
    // -----------------------------
    // デバイス作成、スワップチェイン作成、レンダーターゲット用バッファの準備などを行う
    D3D12Renderer renderer;
    renderer.Initialize(hWnd, ClientWidth, ClientHeight);

    // -----------------------------
    // 4. シーンの作成と登録
    // -----------------------------
    SceneManager sceneManager;
    auto mainScene = std::make_shared<Scene>("Main Scene");
    sceneManager.AddScene("Main", mainScene);   // "Main" というキーで管理
    sceneManager.SwitchScene("Main");           // このシーンをアクティブにする

    // -----------------------------
    // 5. キューブ用メッシュの生成
    // -----------------------------
    MeshData cubeMeshData = CreateCubeMeshData();

    // -----------------------------
    // 6. Cube1 作成
    // -----------------------------
    auto cube1 = std::make_shared<GameObject>("Cube1");
    cube1->Transform->Position = { -2.0f,0.0f,0.0f }; // 左側に配置
    cube1->AddComponent<TestComponent>();             // ライフサイクル確認用
    auto meshRenderer1 = cube1->AddComponent<MeshRendererComponent>();
    meshRenderer1->SetMesh(cubeMeshData);             // メッシュを設定
    renderer.CreateMeshRendererResources(meshRenderer1); // GPU用リソース生成

    // -----------------------------
    // 7. Cube2 作成
    // -----------------------------
    auto cube2 = std::make_shared<GameObject>("Cube2");
    cube2->Transform->Position = { 2.0f,0.0f,0.0f }; // 右側に配置
    auto meshRenderer2 = cube2->AddComponent<MeshRendererComponent>();
    meshRenderer2->SetMesh(cubeMeshData);
    renderer.CreateMeshRendererResources(meshRenderer2);
    cube2->AddComponent<MoveComponent>(cube2.get());  // 移動用コンポーネントを追加

    // シーンにオブジェクトを登録
    mainScene->AddGameObject(cube1);
    mainScene->AddGameObject(cube2);

    // -----------------------------
    // 8. カメラ用 GameObject 作成
    // -----------------------------
    auto cameraObj = std::make_shared<GameObject>("Camera");
    cameraObj->Transform->Position = { 0.0f, 2.0f, -5.0f }; // 初期位置

    // CameraComponent を追加
    auto cameraComp = cameraObj->AddComponent<CameraComponent>(cameraObj.get());
    cameraComp->SetAspect(static_cast<float>(ClientWidth) / ClientHeight);

    // CameraControllerComponent を追加してマウス + WASD で操作可能にする
    cameraObj->AddComponent<CameraControllerComponent>(cameraObj.get(), cameraComp.get()); // ← .get() を追加

    // シーンに登録
    mainScene->AddGameObject(cameraObj);


    // -----------------------------
    // 9. メインループ
    // -----------------------------
    MSG msg = { 0 };
    float elapsedTime = 0.0f; // Cube2 の表示/非表示切り替えに使う経過時間

    // メインループ
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Time::Update();

            float dt = Time::GetDeltaTime();

            // 例: Wキーで前進
            //if (Input::GetKey(KeyCode::W))
            //{
            //    cube1->Transform->Position.z += 2.0f * dt;
            //}

            // 例: 左クリックでログ
            if (Input::GetMouseButtonDown(MouseButton::Left))
            {
                OutputDebugStringA("Left Mouse Clicked!\n");
            }

            if (Input::GetKeyDown(KeyCode::Space))
            {
                bool active = cube2->IsActive();
                mainScene->SetGameObjectActive(cube2, !active);
            }

            //if (Input::GetKeyDown(KeyCode::D))
            //{
            //    cube2->Destroy(); // OnDestroy が呼ばれる
            //}

            if (Input::GetKey(KeyCode::LeftControl))
            {
                OutputDebugStringA("LeftControl Pressed!\n");
            }


            elapsedTime += dt; // 経過時間を更新する

            if (elapsedTime >= 2.0f)
            { 
                // Cube2 のアクティブ状態を反転させる 
                bool active = cube2->IsActive(); 
                mainScene->SetGameObjectActive(cube2, !active); 
                char buf[128];
                sprintf_s(buf, "dt = %f, elapsedTime = %f\n", dt, elapsedTime);
                OutputDebugStringA(buf);
                elapsedTime = 0.0f; 
            }

            auto activeScene = sceneManager.GetActiveScene();
            if (activeScene)
            {
                activeScene->Update(dt);
                renderer.SetScene(activeScene);
                renderer.SetCamera(cameraComp);
                renderer.Render();
            }

            Input::Update();
        }
    }


    // -----------------------------
    // 9. 終了処理
    // -----------------------------
    if (auto activeScene = sceneManager.GetActiveScene())
    {
        activeScene->DestroyAllGameObjects(); // オブジェクトをすべて破棄し、OnDestroy を呼ぶ
    }
    renderer.Cleanup(); // DirectX のリソース解放

    // アプリ終了コードを返す
    return static_cast<int>(msg.wParam);
}
