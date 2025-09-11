// ============================================================================
// main.cpp
// �ړI:
//   - �ŏ����� D3D12 �����_�����O���ŁA�V�[��/�Q�[���I�u�W�F�N�g/�R���|�[�l���g��
//     �ǂ̏��ŏ������E�X�V�E�`�悷�邩����]�ł���悤�ɂ���B
// ����:
//   - �|���V�[: AddComponent() ���� Awake/OnEnable ���ĂԁiScene ���ł� Awake ���Ă΂Ȃ��j
//   - GameObject �� GameObject::Create() �t�@�N�g���Ő����ishared_from_this ���S���j
//   - �E�B���h�E�́u�O�g�T�C�Y�v�Ɓu�N���C�A���g�T�C�Y�v�𖾊m�ɕ���
//   - ���N���C�A���g�T�C�Y����X���b�v�`�F�C��/�J�����A�X�y�N�g���\�z�i�Y���h�~�j
//   - �T���v��: �L�[/�}�E�X���́A2�b���Ƃ� Active �ؑցA�ȒP�Ȉړ��R���|�[�l���g
//
// �悭���闎�Ƃ���:
//   - std::make_shared ���Ăт� GameObject �����ƁA�R���X�g���N�^���� shared_from_this() ��
//     �g���� std::bad_weak_ptr �ɂȂ邱�Ƃ����� �� �t�@�N�g���֐��ŉ���B
//   - AdjustWindowRect �̖߂�́u�O�g���݁v�̃T�C�Y�BDirectX �̃r���[�|�[�g�Ⓤ�e�s���
//     �A�X�y�N�g�v�Z�ɂ� GetClientRect �́u�N���C�A���g�T�C�Y�v���g���B
//   - ���C�t�T�C�N���𕡐��ӏ��ŌĂԂƓ�d������iAwake/Start/OnEnable�j�B�Ăю����{������B
// ============================================================================

#include <windows.h>
#include <string>
#include <memory>
#include <cmath>
#include <sal.h> // VS �̐ÓI��͗p�A�m�e�[�V�����i_In_ �Ȃǁj

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
// �f�o�b�O�p�R���|�[�l���g
// ����:
//   - �R���|�[�l���g�̃��C�t�T�C�N���iOnEnable/OnDisable/OnDestroy�j��
//     �z��^�C�~���O�ŌĂ΂�Ă��邩��ڎ��m�F���邽�߂̍ŏ������B
// ����:
//   - �{�Ԃł̓��b�Z�[�W�{�b�N�X�͎ז��ɂȂ�̂Ń��O�݂̂ɂ���Ȃǒ����B
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
// ����:
//   - ���L GameObject �� z �ʒu���T�C���g�ŉ���������V���v���ȋ����B
// �݌v�m�[�g:
//   - �u�������R���|�[�l���g���v���čė��p���₷��������j�̗�B
// ============================================================================
class MoveComponent : public Component
{
public:
    explicit MoveComponent(GameObject* owner) : Component(ComponentType::None), m_Owner(owner) {}

    // Update �� Scene �� GameObject �� Component �̏��ɓ`�d����
    void Update(float /*dt*/) override {
        if (!m_Owner || !m_Owner->IsActive()) return; // ������/�j���ς݂Ȃ牽�����Ȃ�
        m_Owner->Transform->Position.z = std::sin(m_Frame * 0.05f) * 2.0f; // ���x0.05, �U��2.0
        ++m_Frame;
    }

    void OnEnable()  override { OutputDebugStringA("MoveComponent: OnEnable\n"); }
    void OnDisable() override { OutputDebugStringA("MoveComponent: OnDisable\n"); }

private:
    GameObject* m_Owner = nullptr; // Transform �փA�N�Z�X���邽�߂̏��L�ҎQ��
    int m_Frame = 0;               // �o�߃t���[���i���ԑ�ցj
};

// ============================================================================
// WndProc: Win32 �E�B���h�E�v���V�[�W��
// ����:
//   - OS ����͂�����/�E�B���h�E�C�x���g���ŏ��Ɏ󂯎~�߂�B
//   - ����͍Œ���i���͓]��/WM_DESTROY�j���������B
// �d�v:
//   - Input::ProcessMessage() �������ŕK���Ăԁi�����E����E���W�Ȃǂ�����ɔ��f�j�B
// ============================================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Input::ProcessMessage(message, wParam, lParam); // ���͂̔��f

    AppContext* app = GetApp(hWnd); // �J����/�����_���փA�N�Z�X���邽��

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
        PostQuitMessage(0); // ���C�����[�v�E�o�w��
        return 0;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

// ============================================================================
// ���[�e�B���e�B: �P���ȃJ���t�������̃��b�V������g�݂Ő���
// ����:
//   - �{�i�I�ɂ� GLTF/FBX ���烍�[�h���ă^���W�F���g������������B
//   - ���̗�ł� 8���_�{�C���f�b�N�X�Ŋȑf���i�ʂ��Ƃ̌ŗL�F�������ɏo���Ȃ�ʂ��ƒ��_�d�������z�j�B
// ============================================================================
MeshData CreateCubeMeshData()
{
    MeshData m;

    // ���_�i�ʒu/�@��/�F�j
    m.Vertices = {
        // -Z ��
        {{-0.5f,  0.5f, -0.5f}, {0,0,-1}, {1,0,0,1}},
        {{ 0.5f,  0.5f, -0.5f}, {0,0,-1}, {0,1,0,1}},
        {{-0.5f, -0.5f, -0.5f}, {0,0,-1}, {0,0,1,1}},
        {{ 0.5f, -0.5f, -0.5f}, {0,0,-1}, {1,1,0,1}},
        // +Z ��
        {{-0.5f,  0.5f,  0.5f}, {0,0, 1}, {0,1,1,1}},
        {{ 0.5f,  0.5f,  0.5f}, {0,0, 1}, {1,0,1,1}},
        {{-0.5f, -0.5f,  0.5f}, {0,0, 1}, {0,0,0,1}},
        {{ 0.5f, -0.5f,  0.5f}, {0,0, 1}, {1,1,1,1}},
    };

    // �C���f�b�N�X�i�O�p�`���X�g�j�B������W�n�E�\�ʂ���O�� CW �z��iPSO �̃J�����O�ɍ��킹��j
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
// WinMain: �A�v���G���g��
// �t���[:
//  1) �E�B���h�E�N���X�o�^ �� 2) �E�B���h�E���� �� 3) ���́E���N���C�A���g�擾
//  4) D3D ������ �� 5) �V�[���\�z �� 6) ���C�����[�v�i�X�V�E�`��j �� 7) �I������
// ============================================================================
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int nCmdShow)
{
    // ---------------- 1) �E�B���h�E�N���X�o�^ ----------------
    // �� ������G��̂̓E�B���h�E�̌�����/������ς��������i�A�C�R��/�J�[�\��/�w�i ���j
    const wchar_t CLASS_NAME[] = L"D3D12WindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc); // ���s���� GetLastError()

    // ---------------- 2) �E�B���h�E���� ----------------
    // �\���������N���C�A���g�̈�� 800x600�B�O�g�Ԃ�� AdjustWindowRect �ŉ��Z���č쐬����B
    RECT wr = { 0, 0, 800, 600 };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);  // ���j���[����
    const int outerW = wr.right - wr.left;             // �O�g���ݕ�
    const int outerH = wr.bottom - wr.top;              // �O�g���ݍ�

    HWND hWnd = CreateWindowEx(
        0, CLASS_NAME, L"DirectX12 Engine",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, outerW, outerH,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // ---------------- 3) ���͏����� & ���N���C�A���g�擾 ----------------
    // �� GetClientRect �͕`��Ώۃs�N�Z���ʂ̃T�C�Y�B������r���[�|�[�g/�A�X�y�N�g�Ɏg���B
    Input::Initialize();

    RECT rc{};
    GetClientRect(hWnd, &rc);
    const UINT clientW = rc.right - rc.left;
    const UINT clientH = rc.bottom - rc.top;

    // ---------------- 4) D3D12 ������ ----------------
    // �X���b�v�`�F�C���^RTV/DSV �ȂǓ������\�[�X�� renderer ���ō\�z�����B
    D3D12Renderer renderer;
    renderer.Initialize(hWnd, clientW, clientH);

    static AppContext ctx;
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&ctx));

    // �����_����������ɖ��߂�
    ctx.renderer = &renderer;

    // �����A�X�y�N�g(�N������̃N���C�A���g�T�C�Y)��ۑ����Ă���
    ctx.clientW = clientW;
    ctx.clientH = clientH;

    // ---------------- 5) �V�[���\�z ----------------
    // SceneManager �͕����V�[���̐ؑ�/�ێ���S���B�����ł� "Main" ������ėL�����B
    SceneManager sceneManager;
    auto mainScene = std::make_shared<Scene>("Main Scene");
    sceneManager.AddScene("Main", mainScene);
    sceneManager.SwitchScene("Main");

    // �����̃��b�V���i��g�݁j
    MeshData cube = CreateCubeMeshData();

    // --- Cube1�i���j: ���C�t�T�C�N�����O�t�� ---
    // �d�v: GameObject::Create() ���g���ishared_from_this ���S���j�B
    auto cube1 = GameObject::Create("Cube1");
    cube1->Transform->Position = { -2.0f, 0.0f, 0.0f };
    cube1->AddComponent<TestComponent>(); // OnEnable/Disable/Destroy �̃��O
    auto mr1 = cube1->AddComponent<MeshRendererComponent>();
    mr1->SetMesh(cube);
    renderer.CreateMeshRendererResources(mr1); // VB/IB �� GPU ���\�[�X����

    // --- Cube2�i�E�j: �T�C���g�ŉ��� ---
    auto cube2 = GameObject::Create("Cube2");
    cube2->Transform->Position = { 2.0f, 0.0f, 0.0f };
    auto mr2 = cube2->AddComponent<MeshRendererComponent>();
    mr2->SetMesh(cube);
    renderer.CreateMeshRendererResources(mr2);
    cube2->AddComponent<MoveComponent>(cube2.get()); // ���L�҂�n��

    // �V�[���ɓo�^�i�e�Ȃ������[�g�j
    mainScene->AddGameObject(cube1);
    mainScene->AddGameObject(cube2);

    // --- �J�����iWASD + �}�E�X�ňړ�/��]�ł���j ---
    auto camObj = GameObject::Create("Camera");
    camObj->Transform->Position = { 0.0f, 2.0f, -5.0f }; // ���Ղ���
    auto cameraComp = camObj->AddComponent<CameraComponent>(camObj.get());
    cameraComp->SetAspect(static_cast<float>(clientW) / static_cast<float>(clientH)); // ���N���C�A���g��
    camObj->AddComponent<CameraControllerComponent>(camObj.get(), cameraComp.get());
    mainScene->AddGameObject(camObj);

    // �J�������g�����g��h�Ŗ��߂�
    ctx.camera = cameraComp;

    // ---------------- 6) ���C�����[�v ----------------
    // ���[�v�\��:
    //  - �\�Ȃ� OS ���b�Z�[�W�������i��u���b�L���O�j
    //  - ����ȊO�̎��Ԃ� 1�t���[���i�߂�i���ԍX�V�����͎Q�Ɓ��V�[���X�V���`�恨���̓X�i�b�v�V���b�g�j
    MSG msg = {};
    float elapsed = 0.0f; // �f���p: 2�b���Ƃ� Cube2 ���g�O������

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue; // ���̃t���[���� OS ���b�Z�[�W�����݂̂ŏI��
        }

        // ---- 1) ���ԍX�V ----
        Time::Update();
        const float dt = Time::GetDeltaTime();

        // ---- 2) ���͗�i�K�v�ɉ����Ď����̏����ɒu���j----
        if (Input::GetMouseButtonDown(MouseButton::Left)) {
            OutputDebugStringA("Left Mouse Clicked!\n");
        }
        if (Input::GetKeyDown(KeyCode::Space)) {
            mainScene->SetGameObjectActive(cube2, !cube2->IsActive()); // Space �Ńg�O��
        }

        // �f��: 2�b���ƂɎ����Ńg�O���iSpace �ƕ��p����Ƒ����؂�ւ��\������j
        elapsed += dt;
        if (elapsed >= 2.0f) {
            mainScene->SetGameObjectActive(cube2, !cube2->IsActive());
            elapsed = 0.0f;
        }

        // ---- 3) �X�V���`�� ----
        if (auto scene = sceneManager.GetActiveScene()) {
            scene->Update(dt);                 // �Q�[�����W�b�N�i�e���q���e�R���|�[�l���g�j
            renderer.SetScene(scene);          // ����`�悷��V�[��
            renderer.SetCamera(cameraComp);           // �g�p�J����
            renderer.Render();                 // D3D12 �R�}���h�L�^�����s��Present
        }

        // ---- 4) ���̓X�i�b�v�V���b�g�X�V ----
        // ���t���[���� GetKeyDown/GetKeyUp ����̂��߁A�����Łu�O�t���[����ԁv���m�肳����B
        Input::Update();
    }

    // ---------------- 7) �I������ ----------------
    // �����܂ŗ���ƃE�B���h�E�͕��Ă���BGPU �������c���Ă���\��������̂ŁA
    // renderer.Cleanup() ���� GPU ���������\�[�X���������B
    if (auto scene = sceneManager.GetActiveScene()) {
        scene->DestroyAllGameObjects(); // OnDestroy �𐳂����ĂтȂ���j��
    }
    renderer.Cleanup();

#ifdef _DEBUG
    // �����I�u�W�F�N�g�̃T�}����\��
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
