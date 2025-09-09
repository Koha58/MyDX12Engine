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
// �f�o�b�O�p�R���|�[�l���g
// =============================
// GameObject �ɃA�^�b�`���āA���C�t�T�C�N���C�x���g
// �iOnEnable/OnDisable/OnDestroy�j���������Ă΂�邩���m�F���邽�߂̃T���v���B
class TestComponent : public Component
{
public:
    TestComponent() : Component(ComponentType::None) {}

    // �R���|�[�l���g���L�������ꂽ����ɌĂ΂��
    void OnEnable() override {
        OutputDebugStringA("TestComponent: OnEnable\n"); // Visual Studio �̏o�̓E�B���h�E�Ƀ��O
    }

    // �R���|�[�l���g�����������ꂽ����ɌĂ΂��
    void OnDisable() override {
        OutputDebugStringA("TestComponent: OnDisable\n");
    }

    // �R���|�[�l���g���j������钼�O�ɌĂ΂��
    void OnDestroy() override {
        // OnDestroy ���Ă΂�Ă��邱�Ƃ𕪂���₷���m�F���邽�߂Ƀ_�C�A���O��\��
        MessageBoxA(nullptr, "TestComponent: OnDestroy called!", "Debug", MB_OK);
    }
};

// =============================
// Cube2 �ړ��p�R���|�[�l���g
// =============================
// �T�C���g�֐����g���ăI�u�W�F�N�g��O��ɓ������R���|�[�l���g�B
// �u�ړ��v�Ȃǂ̋����̓R���|�[�l���g�����邱�ƂŁA�ė��p����Ǘ������₷���Ȃ�B
class MoveComponent : public Component
{
public:
    MoveComponent(GameObject* owner) : Component(ComponentType::None), m_Owner(owner) {}

    // ���t���[���Ă΂��X�V����
    void Update(float deltaTime) override {
        // GameObject �����݂��A���A�N�e�B�u��Ԃ̂Ƃ��������삳����
        if (!m_Owner || !m_Owner->IsActive()) return;

        // sin �g�𗘗p���� z ���W�������I�ɕω�������
        // m_FrameCount �ɉ����Ēl��������̂ŁA���ʂƂ��ĉ����^���ɂȂ�
        m_Owner->Transform->Position.z = sin(m_FrameCount * 0.05f) * 2.0f;

        // �t���[�����ƂɃJ�E���g��i�߂�
        m_FrameCount++;
    }

    // �L�������ꂽ����
    void OnEnable() override {
        OutputDebugStringA("MoveComponent: OnEnable\n");
    }

    // ���������ꂽ����
    void OnDisable() override {
        OutputDebugStringA("MoveComponent: OnDisable\n");
    }

private:
    GameObject* m_Owner = nullptr; // ���̃R���|�[�l���g�����L���� GameObject �ւ̎Q��
    int m_FrameCount = 0;          // �t���[���J�E���^�i�����̎�������邽�߂Ɏg�p�j
};

// =============================
// �E�B���h�E�v���V�[�W��
// =============================
// Windows �������Ă��郁�b�Z�[�W�i�E�B���h�E������ꂽ�A���͂����������j����������֐��B
// ����͍Œ���AWM_DESTROY �������������Ă���B
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Input �V�X�e���ɒʒm
    Input::ProcessMessage(message, wParam, lParam);

    switch (message)
    {
    case WM_DESTROY:
        // �E�B���h�E������ꂽ�Ƃ��A�A�v���P�[�V�����I�����w������
        PostQuitMessage(0);
        break;
    default:
        // ���̑��̃��b�Z�[�W�͊��菈���ɓn��
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// =============================
// �L���[�u���b�V������
// =============================
// ���_�J���[�����P���ȗ����̂𐶐��B
// ���ۂ̃Q�[���ł� FBX �ȂǊO���f�[�^���g�����A�����ł͎菑���Œ�`���Ă���B
MeshData CreateCubeMeshData()
{
    MeshData meshData;

    // �����̂̒��_�i�ʒu + �@�� + RGBA�F�j
    meshData.Vertices = {
        // -Z ��
        {{-0.5f,  0.5f, -0.5f}, {0,0,-1}, {1,0,0,1}}, // ��
        {{ 0.5f,  0.5f, -0.5f}, {0,0,-1}, {0,1,0,1}}, // ��
        {{-0.5f, -0.5f, -0.5f}, {0,0,-1}, {0,0,1,1}}, // ��
        {{ 0.5f, -0.5f, -0.5f}, {0,0,-1}, {1,1,0,1}}, // ��

        // +Z ��
        {{-0.5f,  0.5f,  0.5f}, {0,0,1}, {0,1,1,1}},  // �V�A��
        {{ 0.5f,  0.5f,  0.5f}, {0,0,1}, {1,0,1,1}},  // �}�[���^
        {{-0.5f, -0.5f,  0.5f}, {0,0,1}, {0,0,0,1}},  // ��
        {{ 0.5f, -0.5f,  0.5f}, {0,0,1}, {1,1,1,1}},  // ��
    };

    // �C���f�b�N�X�i���܂Œʂ��OK�j
    meshData.Indices = {
        0,1,2, 1,3,2,   // -Z ��
        4,6,5, 5,6,7,   // +Z ��
        4,5,0, 5,1,0,   // +Y ��
        2,3,6, 3,7,6,   // -Y ��
        1,5,3, 5,7,3,   // +X ��
        4,0,6, 0,2,6    // -X ��
    };

    return meshData;
}


// =============================
// WinMain (�A�v���P�[�V�����̃G���g���[�|�C���g)
// =============================
// Windows �A�v���Ƃ��čŏ��ɌĂ΂��֐��B
// �E�B���h�E�쐬 �� DirectX ������ �� �Q�[�����[�v �� �I������ �̗���B
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int nCmdShow)
{
    // -----------------------------
    // 1. �E�B���h�E�N���X�̓o�^
    // -----------------------------
    const wchar_t CLASS_NAME[] = L"D3D12WindowClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;           // ���̃E�B���h�E�ɓ͂����b�Z�[�W����������֐�
    wc.hInstance = hInstance;           // �A�v���P�[�V�����̃C���X�^���X
    wc.lpszClassName = CLASS_NAME;      // ���ʗp�̃N���X��
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); // �J�[�\���`��
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // �w�i�F
    RegisterClass(&wc);

    // -----------------------------
    // 2. �E�B���h�E�쐬
    // -----------------------------
    RECT windowRect = { 0,0,800,600 };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE); // �g���܂߂��E�B���h�E�T�C�Y�ɒ���
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
    // 3. DirectX12Renderer ������
    // -----------------------------
    // �f�o�C�X�쐬�A�X���b�v�`�F�C���쐬�A�����_�[�^�[�Q�b�g�p�o�b�t�@�̏����Ȃǂ��s��
    D3D12Renderer renderer;
    renderer.Initialize(hWnd, ClientWidth, ClientHeight);

    // -----------------------------
    // 4. �V�[���̍쐬�Ɠo�^
    // -----------------------------
    SceneManager sceneManager;
    auto mainScene = std::make_shared<Scene>("Main Scene");
    sceneManager.AddScene("Main", mainScene);   // "Main" �Ƃ����L�[�ŊǗ�
    sceneManager.SwitchScene("Main");           // ���̃V�[�����A�N�e�B�u�ɂ���

    // -----------------------------
    // 5. �L���[�u�p���b�V���̐���
    // -----------------------------
    MeshData cubeMeshData = CreateCubeMeshData();

    // -----------------------------
    // 6. Cube1 �쐬
    // -----------------------------
    auto cube1 = std::make_shared<GameObject>("Cube1");
    cube1->Transform->Position = { -2.0f,0.0f,0.0f }; // �����ɔz�u
    cube1->AddComponent<TestComponent>();             // ���C�t�T�C�N���m�F�p
    auto meshRenderer1 = cube1->AddComponent<MeshRendererComponent>();
    meshRenderer1->SetMesh(cubeMeshData);             // ���b�V����ݒ�
    renderer.CreateMeshRendererResources(meshRenderer1); // GPU�p���\�[�X����

    // -----------------------------
    // 7. Cube2 �쐬
    // -----------------------------
    auto cube2 = std::make_shared<GameObject>("Cube2");
    cube2->Transform->Position = { 2.0f,0.0f,0.0f }; // �E���ɔz�u
    auto meshRenderer2 = cube2->AddComponent<MeshRendererComponent>();
    meshRenderer2->SetMesh(cubeMeshData);
    renderer.CreateMeshRendererResources(meshRenderer2);
    cube2->AddComponent<MoveComponent>(cube2.get());  // �ړ��p�R���|�[�l���g��ǉ�

    // �V�[���ɃI�u�W�F�N�g��o�^
    mainScene->AddGameObject(cube1);
    mainScene->AddGameObject(cube2);

    // -----------------------------
    // 8. �J�����p GameObject �쐬
    // -----------------------------
    auto cameraObj = std::make_shared<GameObject>("Camera");
    cameraObj->Transform->Position = { 0.0f, 2.0f, -5.0f }; // �����ʒu

    // CameraComponent ��ǉ�
    auto cameraComp = cameraObj->AddComponent<CameraComponent>(cameraObj.get());
    cameraComp->SetAspect(static_cast<float>(ClientWidth) / ClientHeight);

    // CameraControllerComponent ��ǉ����ă}�E�X + WASD �ő���\�ɂ���
    cameraObj->AddComponent<CameraControllerComponent>(cameraObj.get(), cameraComp.get()); // �� .get() ��ǉ�

    // �V�[���ɓo�^
    mainScene->AddGameObject(cameraObj);


    // -----------------------------
    // 9. ���C�����[�v
    // -----------------------------
    MSG msg = { 0 };
    float elapsedTime = 0.0f; // Cube2 �̕\��/��\���؂�ւ��Ɏg���o�ߎ���

    // ���C�����[�v
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

            // ��: W�L�[�őO�i
            //if (Input::GetKey(KeyCode::W))
            //{
            //    cube1->Transform->Position.z += 2.0f * dt;
            //}

            // ��: ���N���b�N�Ń��O
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
            //    cube2->Destroy(); // OnDestroy ���Ă΂��
            //}

            if (Input::GetKey(KeyCode::LeftControl))
            {
                OutputDebugStringA("LeftControl Pressed!\n");
            }


            elapsedTime += dt; // �o�ߎ��Ԃ��X�V����

            if (elapsedTime >= 2.0f)
            { 
                // Cube2 �̃A�N�e�B�u��Ԃ𔽓]������ 
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
    // 9. �I������
    // -----------------------------
    if (auto activeScene = sceneManager.GetActiveScene())
    {
        activeScene->DestroyAllGameObjects(); // �I�u�W�F�N�g�����ׂĔj�����AOnDestroy ���Ă�
    }
    renderer.Cleanup(); // DirectX �̃��\�[�X���

    // �A�v���I���R�[�h��Ԃ�
    return static_cast<int>(msg.wParam);
}
