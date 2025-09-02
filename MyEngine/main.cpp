#include <windows.h> // Windows APIの基本機能
#include <string>    // std::stringを使用
#include <memory>    // std::shared_ptrを使用
#include <iostream>  // 入出力ストリーム用（デバッグ目的など）

// SAL (Source Code Annotation Language) アノテーションのために必要
// コンパイラによるコード解析を強化し、潜在的なバグを特定するのに役立つ
#include <sal.h> 

#include "D3D12Renderer.h" // D3D12描画処理を管理するクラスのヘッダー
#include "GameObject.h"    // ゲーム内のオブジェクトを表現するクラスのヘッダー
#include "Mesh.h"          // メッシュデータ構造を定義するヘッダー
#include "Scene.h" 

// ウィンドウプロシージャ
// Windowsメッセージを処理するためのコールバック関数
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY: // ウィンドウが破棄されるときに発生
        PostQuitMessage(0); // アプリケーション終了メッセージを投稿
        break;
    default: // その他のメッセージはデフォルトのウィンドウプロシージャに任せる
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0; // メッセージを処理したことを示す
}

// キューブの頂点とインデックスデータを生成する関数
MeshData CreateCubeMeshData()
{
    MeshData meshData;
    // 頂点データの設定: 位置 (x, y, z) と色 (r, g, b, a)
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

    // インデックスデータの設定
    // 各面を2つの三角形で構成し、各頂点インデックスを指定
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

// WinMain 関数: Windowsアプリケーションのエントリーポイント
// SALアノテーションが引数の使用方法を記述し、コード品質を向上させる
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,       // アプリケーションの現在のインスタンスへのハンドル
    _In_opt_ HINSTANCE hPrevInstance, // アプリケーションの前のインスタンスへのハンドル（常にNULL）
    _In_ LPSTR lpCmdLine,           // コマンドライン文字列
    _In_ int nCmdShow               // ウィンドウの表示方法を示すフラグ
)
{
    // ウィンドウクラス名の定義
    const wchar_t CLASS_NAME[] = L"D3D12WindowClass";

    // ウィンドウクラス構造体の設定
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;         // ウィンドウプロシージャを設定
    wc.hInstance = hInstance;         // インスタンスハンドル
    wc.lpszClassName = CLASS_NAME;    // クラス名
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); // デフォルトカーソル
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // デフォルト背景色

    // ウィンドウクラスの登録
    if (!RegisterClass(&wc))
    {
        MessageBox(nullptr, L"ウィンドウクラスの登録に失敗しました！", L"エラー", MB_OK | MB_ICONERROR);
        return 1; // 失敗時はエラーコードを返す
    }

    // ウィンドウの初期サイズとスタイルの設定
    RECT windowRect = { 0, 0, 800, 600 };
    // クライアント領域のサイズに基づいてウィンドウの実際のサイズを計算
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // 計算されたクライアント領域の幅と高さを取得
    const UINT ClientWidth = windowRect.right - windowRect.left;
    const UINT ClientHeight = windowRect.bottom - windowRect.top;

    // ウィンドウの作成
    HWND hWnd = CreateWindowEx(
        0,                            // 拡張スタイル
        CLASS_NAME,                   // 登録済みのウィンドウクラス名
        L"DirectX 12 Engine",         // ウィンドウのタイトルバーに表示されるテキスト
        WS_OVERLAPPEDWINDOW,          // ウィンドウのスタイル
        CW_USEDEFAULT, CW_USEDEFAULT, // 初期位置 (システムに任せる)
        ClientWidth, ClientHeight,    // 初期サイズ
        nullptr,                      // 親ウィンドウのハンドル
        nullptr,                      // メニューハンドルまたは子ウィンドウID
        hInstance,                    // アプリケーションインスタンスのハンドル
        nullptr                       // アプリケーション定義データへのポインタ
    );

    // ウィンドウ作成の失敗チェック
    if (hWnd == nullptr)
    {
        MessageBox(nullptr, L"ウィンドウの作成に失敗しました！", L"エラー", MB_OK | MB_ICONERROR);
        return 1; // 失敗時はエラーコードを返す
    }

    // ウィンドウを表示し、更新
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // D3D12Renderer オブジェクトの初期化
    D3D12Renderer renderer;
    if (!renderer.Initialize(hWnd, ClientWidth, ClientHeight))
    {
        MessageBox(nullptr, L"D3D12Renderer の初期化に失敗しました。", L"エラー", MB_OK | MB_ICONERROR);
        return 1; // 失敗時はエラーコードを返す
    }

    // --- シーンとGameObjectの作成 ---
    // メインシーンを作成
    std::shared_ptr<Scene> mainScene = std::make_shared<Scene>("Main Scene");

    // キューブのメッシュデータを生成
    MeshData cubeMeshData = CreateCubeMeshData();

    // 最初のキューブGameObjectを作成
    std::shared_ptr<GameObject> cube1 = std::make_shared<GameObject>("Cube1");
    // 左に大きく移動させて、2つのキューブが見えるか確認
    cube1->Transform->Position = DirectX::XMFLOAT3(-2.0f, 0.0f, 0.0f);
    // メッシュレンダラーコンポーネントを追加
    std::shared_ptr<MeshRendererComponent> meshRenderer1 = cube1->AddComponent<MeshRendererComponent>();
    meshRenderer1->SetMesh(cubeMeshData); // CPU側のメッシュデータを設定
    // D3D12リソースをGPUにアップロード
    if (!renderer.CreateMeshRendererResources(meshRenderer1)) {
        MessageBox(nullptr, L"Cube1 のメッシュリソース作成に失敗しました。", L"エラー", MB_OK | MB_ICONERROR);
        return 1; // 失敗時はエラーコードを返す
    }

    // 2番目のキューブGameObjectを作成 (異なる位置)
    std::shared_ptr<GameObject> cube2 = std::make_shared<GameObject>("Cube2");
    // 右に大きく移動させて、2つのキューブが見えるか確認
    cube2->Transform->Position = DirectX::XMFLOAT3(2.0f, 0.0f, 0.0f);
    // メッシュレンダラーコンポーネントを追加
    std::shared_ptr<MeshRendererComponent> meshRenderer2 = cube2->AddComponent<MeshRendererComponent>();
    meshRenderer2->SetMesh(cubeMeshData);
    // D3D12リソースをGPUにアップロード
    if (!renderer.CreateMeshRendererResources(meshRenderer2)) {
        MessageBox(nullptr, L"Cube2 のメッシュリソース作成に失敗しました。", L"エラー", MB_OK | MB_ICONERROR);
        return 1; // 失敗時はエラーコードを返す
    }

    // シーンにGameObjectを追加
    mainScene->AddGameObject(cube1);
    mainScene->AddGameObject(cube2);

    // レンダラーにメインシーンを設定
    renderer.SetScene(mainScene);

    // ゲームループ
    MSG msg = { 0 }; // メッセージ構造体を初期化
    while (WM_QUIT != msg.message) // WM_QUITメッセージが来るまでループを続ける
    {
        // メッセージキューにメッセージがあるかチェックし、あれば取り出す
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg); // キーボードメッセージを変換
            DispatchMessage(&msg);  // ウィンドウプロシージャにメッセージをディスパッチ
        }
        else // メッセージがない場合、ゲームロジックとレンダリングを実行
        {
            // 時間更新 (簡易的なdeltaTime、実際には高精度タイマーを使用すべき)
            float deltaTime = 1.0f / 60.0f; // 60fpsを想定した固定デルタタイム

            // シーンの更新ロジック
            mainScene->Update(deltaTime);

            // Cube1を回転させる例
            cube1->Transform->Rotation.y += DirectX::XMConvertToRadians(1.0f); // 毎フレームY軸周りに1度回転

            // Cube2をZ軸上でサイン波状に移動させる例
            cube2->Transform->Position.z = sin(renderer.GetFrameCount() * 0.05f) * 2.0f;

            // シーンのレンダリング
            renderer.Render();
        }
    }

    // アプリケーション終了時のクリーンアップ処理
    renderer.Cleanup();

    // アプリケーションの終了コードを返す
    return static_cast<int>(msg.wParam);
}