// ============================================================================
// main.cpp
// 目的:
//   - 最小限の D3D12 レンダリング環境で、シーン/ゲームオブジェクト/コンポーネントを
//     どの順で初期化・更新・描画するかを一望できるようにする。
// 特徴:
//   - ポリシー: AddComponent() 側で Awake/OnEnable を呼ぶ（Scene 側では Awake を呼ばない）
//   - GameObject は GameObject::Create() ファクトリで生成（shared_from_this 安全化）
//   - ウィンドウの「外枠サイズ」と「クライアントサイズ」を明確に分離
//   - 実クライアントサイズからスワップチェイン/カメラアスペクトを構築（ズレ防止）
//   - サンプル: キー/マウス入力、2秒ごとの Active 切替、簡単な移動コンポーネント
//
// よくある落とし穴:
//   - std::make_shared 直呼びで GameObject を作ると、コンストラクタ内の shared_from_this() が
//     使えず std::bad_weak_ptr になることがある → ファクトリ関数で回避。
//   - AdjustWindowRect の戻りは「外枠込み」のサイズ。DirectX のビューポートや投影行列の
//     アスペクト計算には GetClientRect の「クライアントサイズ」を使う。
//   - ライフサイクルを複数箇所で呼ぶと二重化する（Awake/Start/OnEnable）。呼び主を一本化する。
// ============================================================================

#include <windows.h>
#include <string>
#include <memory>
#include <cmath>
#include <sal.h> // VS の静的解析用アノテーション（_In_ など）

#include "D3D12Renderer.h"
#include "GameObject.h"
#include "Mesh.h"
#include "Scene.h"
#include "SceneManager.h"
#include "Time.h"
#include "Input.h"
#include "CameraComponent.h"
#include "CameraControllerComponent.h"

#ifdef  _DEBUG
#include<wrl/client.h>
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
using Microsoft::WRL::ComPtr;
#endif //  _DEBUG


struct AppContext {
    D3D12Renderer* renderer = nullptr;
    std::shared_ptr<CameraComponent> camera;
    UINT clientW = 0;
    UINT clientH = 0;
};

static AppContext* GetApp(HWND hWnd)
{
    return reinterpret_cast<AppContext*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
}

// ============================================================================
// デバッグ用コンポーネント
// 役割:
//   - コンポーネントのライフサイクル（OnEnable/OnDisable/OnDestroy）が
//     想定タイミングで呼ばれているかを目視確認するための最小実装。
// 注意:
//   - 本番ではメッセージボックスは邪魔になるのでログのみにするなど調整。
// ============================================================================
class TestComponent : public Component
{
public:
    TestComponent() : Component(ComponentType::None) {}
    void OnEnable()  override { OutputDebugStringA("TestComponent: OnEnable\n"); }
    void OnDisable() override { OutputDebugStringA("TestComponent: OnDisable\n"); }
    void OnDestroy() override { MessageBoxA(nullptr, "TestComponent: OnDestroy called!", "Debug", MB_OK); }
};

// ============================================================================
// MoveComponent
// 役割:
//   - 所有 GameObject の z 位置をサイン波で往復させるシンプルな挙動。
// 設計ノート:
//   - 「挙動をコンポーネント化」して再利用しやすくする方針の例。
// ============================================================================
class MoveComponent : public Component
{
public:
    explicit MoveComponent(GameObject* owner) : Component(ComponentType::None), m_Owner(owner) {}

    // Update は Scene → GameObject → Component の順に伝播する
    void Update(float /*dt*/) override {
        if (!m_Owner || !m_Owner->IsActive()) return; // 無効化/破棄済みなら何もしない
        m_Owner->Transform->Position.z = std::sin(m_Frame * 0.05f) * 2.0f; // 速度0.05, 振幅2.0
        ++m_Frame;
    }

    void OnEnable()  override { OutputDebugStringA("MoveComponent: OnEnable\n"); }
    void OnDisable() override { OutputDebugStringA("MoveComponent: OnDisable\n"); }

private:
    GameObject* m_Owner = nullptr; // Transform へアクセスするための所有者参照
    int m_Frame = 0;               // 経過フレーム（時間代替）
};

// ============================================================================
// WndProc: Win32 ウィンドウプロシージャ
// 役割:
//   - OS から届く入力/ウィンドウイベントを最初に受け止める。
//   - 今回は最低限（入力転送/WM_DESTROY）だけ処理。
// 重要:
//   - Input::ProcessMessage() をここで必ず呼ぶ（押下・離上・座標などを内部に反映）。
// ============================================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Input::ProcessMessage(message, wParam, lParam); // 入力の反映

    AppContext* app = GetApp(hWnd); // カメラ/レンダラへアクセスするため

    switch (message) {
    case WM_SIZE: {
        if (!app || wParam == SIZE_MINIMIZED) return 0;

        auto newW = LOWORD(lParam);
        auto newH = HIWORD(lParam);
        if (newW == 0 || newH == 0) return 0;

        app->clientW = newW;
        app->clientH = newH;

        if (app->renderer) {

            app->renderer->Resize(newW, newH);
        }

        if (app->camera) {
            
            app->camera->SetAspect(static_cast<float>(newW) /
                static_cast<float>(newH));
        }

        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0); // メインループ脱出指示
        return 0;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

// ============================================================================
// ユーティリティ: 単純なカラフル立方体メッシュを手組みで生成
// 注意:
//   - 本格的には GLTF/FBX からロードしてタンジェント等も持たせる。
//   - この例では 8頂点＋インデックスで簡素化（面ごとの固有色を厳密に出すなら面ごと頂点重複が理想）。
// ============================================================================
MeshData CreateCubeMeshData()
{
    MeshData m;

    // 頂点（位置/法線/色）
    m.Vertices = {
        // -Z 面
        {{-0.5f,  0.5f, -0.5f}, {0,0,-1}, {1,0,0,1}},
        {{ 0.5f,  0.5f, -0.5f}, {0,0,-1}, {0,1,0,1}},
        {{-0.5f, -0.5f, -0.5f}, {0,0,-1}, {0,0,1,1}},
        {{ 0.5f, -0.5f, -0.5f}, {0,0,-1}, {1,1,0,1}},
        // +Z 面
        {{-0.5f,  0.5f,  0.5f}, {0,0, 1}, {0,1,1,1}},
        {{ 0.5f,  0.5f,  0.5f}, {0,0, 1}, {1,0,1,1}},
        {{-0.5f, -0.5f,  0.5f}, {0,0, 1}, {0,0,0,1}},
        {{ 0.5f, -0.5f,  0.5f}, {0,0, 1}, {1,1,1,1}},
    };

    // インデックス（三角形リスト）。左手座標系・表面が手前で CW 想定（PSO のカリングに合わせる）
    m.Indices = {
        0,1,2, 1,3,2,  // -Z
        4,6,5, 5,6,7,  // +Z
        4,5,0, 5,1,0,  // +Y
        2,3,6, 3,7,6,  // -Y
        1,5,3, 5,7,3,  // +X
        4,0,6, 0,2,6   // -X
    };

    return m;
}

// ============================================================================
// WinMain: アプリエントリ
// フロー:
//  1) ウィンドウクラス登録 → 2) ウィンドウ生成 → 3) 入力・実クライアント取得
//  4) D3D 初期化 → 5) シーン構築 → 6) メインループ（更新・描画） → 7) 終了処理
// ============================================================================
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int nCmdShow)
{
    // ---------------- 1) ウィンドウクラス登録 ----------------
    // ※ ここを触るのはウィンドウの見た目/挙動を変えたい時（アイコン/カーソル/背景 等）
    const wchar_t CLASS_NAME[] = L"D3D12WindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc); // 失敗時は GetLastError()

    // ---------------- 2) ウィンドウ生成 ----------------
    // 表示したいクライアント領域は 800x600。外枠ぶんは AdjustWindowRect で加算して作成する。
    RECT wr = { 0, 0, 800, 600 };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);  // メニュー無し
    const int outerW = wr.right - wr.left;             // 外枠込み幅
    const int outerH = wr.bottom - wr.top;              // 外枠込み高

    HWND hWnd = CreateWindowEx(
        0, CLASS_NAME, L"DirectX12 Engine",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, outerW, outerH,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // ---------------- 3) 入力初期化 & 実クライアント取得 ----------------
    // ※ GetClientRect は描画対象ピクセル面のサイズ。これをビューポート/アスペクトに使う。
    Input::Initialize();

    RECT rc{};
    GetClientRect(hWnd, &rc);
    const UINT clientW = rc.right - rc.left;
    const UINT clientH = rc.bottom - rc.top;

    // ---------------- 4) D3D12 初期化 ----------------
    // スワップチェイン／RTV/DSV など内部リソースは renderer 側で構築される。
    D3D12Renderer renderer;
    renderer.Initialize(hWnd, clientW, clientH);

    static AppContext ctx;
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&ctx));

    // レンダラ初期化後に埋める
    ctx.renderer = &renderer;

    // 初期アスペクト(起動直後のクライアントサイズ)を保存しておく
    ctx.clientW = clientW;
    ctx.clientH = clientH;

    // ---------------- 5) シーン構築 ----------------
    // SceneManager は複数シーンの切替/保持を担う。ここでは "Main" を作って有効化。
    SceneManager sceneManager;
    auto mainScene = std::make_shared<Scene>("Main Scene");
    sceneManager.AddScene("Main", mainScene);
    sceneManager.SwitchScene("Main");

    // 立方体メッシュ（手組み）
    MeshData cube = CreateCubeMeshData();

    // --- Cube1（左）: ライフサイクルログ付き ---
    // 重要: GameObject::Create() を使う（shared_from_this 安全化）。
    auto cube1 = GameObject::Create("Cube1");
    cube1->Transform->Position = { -2.0f, 0.0f, 0.0f };
    cube1->AddComponent<TestComponent>(); // OnEnable/Disable/Destroy のログ
    auto mr1 = cube1->AddComponent<MeshRendererComponent>();
    mr1->SetMesh(cube);
    renderer.CreateMeshRendererResources(mr1); // VB/IB の GPU リソース準備

    // --- Cube2（右）: サイン波で往復 ---
    auto cube2 = GameObject::Create("Cube2");
    cube2->Transform->Position = { 2.0f, 0.0f, 0.0f };
    auto mr2 = cube2->AddComponent<MeshRendererComponent>();
    mr2->SetMesh(cube);
    renderer.CreateMeshRendererResources(mr2);
    cube2->AddComponent<MoveComponent>(cube2.get()); // 所有者を渡す

    // シーンに登録（親なし＝ルート）
    mainScene->AddGameObject(cube1);
    mainScene->AddGameObject(cube2);

    // --- カメラ（WASD + マウスで移動/回転できる） ---
    auto camObj = GameObject::Create("Camera");
    camObj->Transform->Position = { 0.0f, 2.0f, -5.0f }; // 俯瞰ぎみ
    auto cameraComp = camObj->AddComponent<CameraComponent>(camObj.get());
    cameraComp->SetAspect(static_cast<float>(clientW) / static_cast<float>(clientH)); // 実クライアント比
    camObj->AddComponent<CameraControllerComponent>(camObj.get(), cameraComp.get());
    mainScene->AddGameObject(camObj);

    // カメラを使った“後”で埋める
    ctx.camera = cameraComp;

    // ---------------- 6) メインループ ----------------
    // ループ構造:
    //  - 可能なら OS メッセージを処理（非ブロッキング）
    //  - それ以外の時間は 1フレーム進める（時間更新→入力参照→シーン更新→描画→入力スナップショット）
    MSG msg = {};
    float elapsed = 0.0f; // デモ用: 2秒ごとに Cube2 をトグルする

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue; // このフレームは OS メッセージ処理のみで終了
        }

        // ---- 1) 時間更新 ----
        Time::Update();
        const float dt = Time::GetDeltaTime();

        // ---- 2) 入力例（必要に応じて自分の処理に置換）----
        if (Input::GetMouseButtonDown(MouseButton::Left)) {
            OutputDebugStringA("Left Mouse Clicked!\n");
        }
        if (Input::GetKeyDown(KeyCode::Space)) {
            mainScene->SetGameObjectActive(cube2, !cube2->IsActive()); // Space でトグル
        }

        // デモ: 2秒ごとに自動でトグル（Space と併用すると早く切り替わる可能性あり）
        elapsed += dt;
        if (elapsed >= 2.0f) {
            mainScene->SetGameObjectActive(cube2, !cube2->IsActive());
            elapsed = 0.0f;
        }

        // ---- 3) 更新＆描画 ----
        if (auto scene = sceneManager.GetActiveScene()) {
            scene->Update(dt);                 // ゲームロジック（親→子→各コンポーネント）
            renderer.SetScene(scene);          // 今回描画するシーン
            renderer.SetCamera(cameraComp);           // 使用カメラ
            renderer.Render();                 // D3D12 コマンド記録→実行→Present
        }

        // ---- 4) 入力スナップショット更新 ----
        // 次フレームの GetKeyDown/GetKeyUp 判定のため、ここで「前フレーム状態」を確定させる。
        Input::Update();
    }

    // ---------------- 7) 終了処理 ----------------
    // ここまで来るとウィンドウは閉じている。GPU 処理が残っている可能性があるので、
    // renderer.Cleanup() 内で GPU 同期→リソース解放をする。
    if (auto scene = sceneManager.GetActiveScene()) {
        scene->DestroyAllGameObjects(); // OnDestroy を正しく呼びながら破棄
    }
    renderer.Cleanup();

#ifdef _DEBUG
    // 生存オブジェクトのサマリを表示
    Microsoft::WRL::ComPtr<IDXGIDebug1> dxgiDebug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
    {
        const DXGI_DEBUG_RLO_FLAGS flags =
            static_cast<DXGI_DEBUG_RLO_FLAGS>(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL);
        dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, flags);
    }
#endif

    return static_cast<int>(msg.wParam);
}
