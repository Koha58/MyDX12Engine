#pragma once // ����w�b�_�̑��d�C���N���[�h�h�~�iMSVC/GCC/Clang�j

// ============================== �ˑ��w�b�_ ===============================
// D3D12 �� COM �x�[�X�BMicrosoft::WRL::ComPtr �ŎQ�ƃJ�E���g�������Ǘ��B
#include <windows.h>        // HWND, HANDLE, Win32 ��{�^
#include <d3d12.h>          // D3D12 �R�A API
#include <dxgi1_6.h>        // DXGI�iSwapChain/Adapter �񋓁j�� IDXGISwapChain4 �p
#include <wrl/client.h>     // Microsoft::WRL::ComPtr�iCOM �X�}�[�g�|�C���^�j
#include <DirectXMath.h>    // �s��/�x�N�g���iSIMD �x�[�X�j
#include <vector>
#include <memory>
#include <functional>

#include "GameObject.h"            // �V�[���c���[�̃m�[�h
#include "Mesh.h"                  // MeshData / Vertex �^ �Ȃ�
#include "Scene.h"                 // ���[�g GameObject �̏W��
#include "MeshRendererComponent.h" // ���b�V���`��p�R���|�[�l���g
#include "CameraComponent.h"       // View/Projection ���
#include "SceneConstantBuffer.h"   // HLSL cbuffer �ɑΉ����� CPU ���\����
#include "d3dx12.h"                // CD3DX12_* ���[�e�B���e�B�i�w���p�j
// =======================================================================

// =======================================================================
// D3D12Renderer
//  - D3D12 �̏�����/��n���A�t���[�����̃R�}���h�L�^�A�X���b�v�A������S���B
//  - �V�[���iScene�j�ƃJ�����iCameraComponent�j���󂯎��`�悷��B
//  - �|���V�[�F���̃N���X�́u�჌�x���`�搧��v�ɏW���B�Q�[�����W�b�N�͕ێ����Ȃ��B
// =======================================================================
class D3D12Renderer
{
public:
    // ----------------------------- �t���[����d�� -----------------------------
    // �o�b�t�@�����B2=�_�u���o�b�t�@�B������ 3 �ɂ���� Present �҂�������ꍇ����B
    static const UINT FrameCount = 2;

    // ----------------------------- ���C�t�T�C�N�� -----------------------------
    D3D12Renderer();
    ~D3D12Renderer(); // Cleanup() �o�R�ň��S�ɔj���iGPU �����҂����݁j

    // === Public API�i�������j=================================================
    // 1) Initialize:
    //   - �f�o�C�X/�L���[/�X���b�v�`�F�C��/RTV/DSV/�R�}���h/PSO ���ˑ����ɍ\�z�B
    //   - ����: true / ���s: false�i�ڍׂ̓f�o�b�O�o�͂��Q�Ɓj�B
    //   - HWND �̎����͌Ăяo�����Ǘ��i���L���Ȃ��j�B
    bool Initialize(HWND hwnd, UINT width, UINT height);

    // 2) Render:
    //   - �R�}���h���X�g Reset���L�^��Close��Execute��Present�B
    //   - �I�[�� WaitForPreviousFrame �ɂ�� CPU/GPU ���S�����i���S�d���j�B
    //   - �V�[��/�J�������ݒ莞�͈��S�ɃX�L�b�v�B
    void Render();

    // 3) Resize:
    //   - �X���b�v�`�F�C���č쐬�i�T�C�Y�̂݁j��RTV/DSV ��蒼���B
    //   - �g�p�����������邽�߁A���O�� WaitForGPU() �őҋ@�B
    void Resize(UINT width, UINT height) noexcept;

    // 4) Cleanup:
    //   - GPU �����҂����V�[�� GPU ���\�[�X������R�}���h/�q�[�v/CB/����/SC/Dev �̏��ŉ���B
    //   - ��d�Ăяo���ɑ΂��Ĉ��S�inullptr/���������͖��Q�j�B
    void Cleanup();

    // === �V�[�� / �J���� ======================================================
    // �`��ΏۃV�[���̐ݒ�i���L���L�j�B�ؑւ̓t���[���Ԃ̈��S�ȃ^�C�~���O�ŁB
    void SetScene(std::shared_ptr<Scene> scene) { m_CurrentScene = scene; }

    // �g�p�J�����̐ݒ�B�A�X�y�N�g���̍X�V�� Camera ���Ŏ��{���ēn���B
    void SetCamera(std::shared_ptr<CameraComponent> camera) { m_Camera = camera; }

    // === ���b�V�� GPU ���\�[�X�쐬 ==========================================
    // MeshData �� Upload �q�[�v��� VB/IB �𐶐����AVBV/IBV/IndexCount ��ݒ�B
    // ���/�ÓI�f�[�^�͏��� Default �q�[�v�]���ł֒u�������B
    bool CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> meshRenderer);

    // === �����E���[�e�B���e�B�i�K�v�ɉ����ĊO��������Ăׂ�悤���J�j ==========
    // Fence �� Signal�����B�҂��BframeIndex �� Present ��̍ŐV�l�ɍX�V�B
    void WaitForPreviousFrame();

    // ��O�𓊂��Ȃ��ȈՑҋ@�BResize ���u�Ƃ肠�����҂v�p�r�B
    void WaitForGPU() noexcept;

    // PSO/���[�g/CBV/RT �����K�؂ɐݒ�ς݂ł���O��̑����h���[�B
    void DrawMesh(MeshRendererComponent* meshRenderer);

    // �V�[���z���� GPU ���\�[�X(VB/IB ��)�����S�ɉ���iResize/Cleanup �����ł����p�j�B
    void ReleaseSceneResources();

    // ���v/�A�j������p�FPresent �����񐔁i�o�߃t���[�����j��Ԃ��B
    UINT GetFrameCount() const { return m_frameCount; }

private:
    // === �����������w���p�iInitialize ����Ă΂�� / ���s�� false ��Ԃ��j =========
    bool CreateDevice();                         // �A�_�v�^�� �� D3D12CreateDevice
    bool CreateCommandQueue();                   // Direct �R�}���h�L���[
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height); // Flip Discard �X���b�v�`�F�C��
    bool CreateRenderTargetViews();              // �e�o�b�N�o�b�t�@�� RTV �쐬
    bool CreateDepthStencilBuffer(UINT width, UINT height);    // D32F �e�N�X�`�� + DSV
    bool CreateCommandAllocatorsAndList();       // �t���[���ʃA���P�[�^ + ���L CL
    bool CreatePipelineState();                  // ���[�g�V�O�l�`�� + PSO �\�z

private:
    // ======================== D3D12 ��v�I�u�W�F�N�g�Q =======================
    // ComPtr �̓X�R�[�v�I���Ŏ��� Release�B���� Release �s�v�B
    Microsoft::WRL::ComPtr<ID3D12Device>                device;         // �_���f�o�C�X
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>          commandQueue;   // ���ڃR�}���h�L���[
    Microsoft::WRL::ComPtr<IDXGISwapChain4>             swapChain;      // �o�b�N/�t�����g�ؑցiIDXGISwapChain4�j
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        rtvHeap;        // RTV �q�[�v�iCPU ���j
    UINT                                                rtvDescriptorSize = 0; // RTV �C���N�������g��
    Microsoft::WRL::ComPtr<ID3D12Resource>              renderTargets[FrameCount]; // �o�b�N�o�b�t�@�Q

    // ---- �[�x/�X�e���V�� ----
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        dsvHeap;        // DSV �q�[�v�iCPU ���j
    Microsoft::WRL::ComPtr<ID3D12Resource>              depthStencilBuffer; // D32F �e�N�X�`��

    // ---- �R�}���h�L�^�n�i�t���[�����ƂɃA���P�[�^�����j----
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      commandAllocators[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>   commandList;    // 1 �{�^�p�iReset/Close �ōė��p�j

    // ---- ���[�g/�p�C�v���C�� ----
    Microsoft::WRL::ComPtr<ID3D12RootSignature>         rootSignature;  // ���\�[�X�������C�A�E�g
    Microsoft::WRL::ComPtr<ID3D12PipelineState>         pipelineState;  // PSO�iVS/PS/RS/DS/Blend ���j

    // ---- �����iCPU-GPU�j----
    Microsoft::WRL::ComPtr<ID3D12Fence>                 fence;          // GPU ������
    UINT64                                              fenceValue = 0; // �V�O�i���l�i���t���[��++�j
    HANDLE                                              fenceEvent = nullptr; // �ҋ@�p Win32 �C�x���g

    // ---- �萔�o�b�t�@�i�I�u�W�F�N�g���j----
    // �EUpload�iCPU ���j�ɑ傫�߂̒P��o�b�t�@���m�ۂ��A�e�I�u�W�F�N�g�� 256B �P�ʂŊ����B
    // �ECBV �� ShaderVisible �q�[�v�Ɏ��O�������Ă����A�`�掞�� GPU �n���h�����I�t�Z�b�g�w��B
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        m_cbvHeap;      // CBV/SRV/UAV �q�[�v�iGPU ���j
    Microsoft::WRL::ComPtr<ID3D12Resource>              m_constantBuffer; // Upload �q�[�v�i�i���}�b�v�j
    UINT8* m_pCbvDataBegin = nullptr; // Map �擪�A�h���X
    UINT                                                m_cbStride = 0;        // 1 �I�u�W�F�N�g���� CB �T�C�Y�i256B �A���C���j
    UINT                                                m_cbvDescriptorSize = 0; // CBV/SRV/UAV �C���N�������g��

    // ============================ ���/�T�C�Y�� ==============================
    UINT                                                m_Width = 0;   // �N���C�A���g�̈�̕��ipx�j
    UINT                                                m_Height = 0;   // �N���C�A���g�̈�̍����ipx�j
    UINT                                                frameIndex = 0; // ���o�b�N�o�b�t�@ index�iPresent ��ɍX�V�j
    UINT                                                m_frameCount = 0; // Present �񐔁i���v/�A�j������Ȃǁj

    // ============================ �����x���Q�� ===============================
    // Renderer �� Scene/Camera �́u���p�ҁv�B������ shared_ptr �ŋ��L���邾���B
    std::shared_ptr<Scene>                              m_CurrentScene; // �`��ΏۃV�[��
    std::shared_ptr<CameraComponent>                    m_Camera;       // �g�p�J�����iView/Proj �������j

    // ============================ �����̊g���i�c�u�j =========================
    // �� �݊��ێ��̂��ߎc�u�B���g�p�� m_Camera ����擾�𐄏��B
    DirectX::XMMATRIX                                   m_ViewMatrix;        // m_Camera->GetViewMatrix() �𐄏�
    DirectX::XMMATRIX                                   m_ProjectionMatrix;  // ����
};
