#pragma once // ����w�b�_�̑��d�C���N���[�h�h�~�iMSVC/GCC/Clang�Ή��̊ȈՔŁj

// ============================== �ˑ��w�b�_ ===============================
// �� D3D12 �� COM �x�[�X�BMicrosoft::WRL::ComPtr �ŎQ�ƃJ�E���g�������Ǘ�����B
#include <windows.h>      // HWND, HANDLE, Win32 ��{�^
#include <d3d12.h>        // D3D12 �R�A API
#include <dxgi1_4.h>      // �X���b�v�`�F�C�� / �A�_�v�^�񋓁iDXGI 1.4�j
#include <wrl/client.h>   // Microsoft::WRL::ComPtr�iCOM �X�}�[�g�|�C���^�j
#include <DirectXMath.h>  // �s��/�x�N�g���iSIMD �x�[�X�j
#include <vector>
#include <memory>
#include <functional>

#include "GameObject.h"            // �V�[���c���[�̃m�[�h
#include "Mesh.h"                  // MeshData / Vertex �^ �Ȃ�
#include "Scene.h"                 // ���[�g GameObject �̏W���ƍX�V/�`��̋N�_
#include "MeshRendererComponent.h" // ���b�V���`��p�R���|�[�l���g
#include "CameraComponent.h"       // View/Projection ���
#include "SceneConstantBuffer.h"   // HLSL cbuffer �ɑΉ����� CPU ���\����
#include "d3dx12.h"                // D3D12 �w���p�[�iCD3DX12_* ���[�e�B���e�B�j
// =======================================================================

// =======================================================================
// D3D12Renderer
//  - D3D12 �̏�����/��n���A�t���[�����̃R�}���h�L�^�A�X���b�v�A������S���B
//  - �V�[���iScene�j�ƃJ�����iCameraComponent�j���󂯎���ĕ`����s���B
//  - �|���V�[�F���̃N���X�́u�჌�x���̕`�搧��v�ɏW�����A�Q�[�����W�b�N�͕ێ����Ȃ��B
//    * GameObject �̏����R���|�[�l���g�P�ʂ̕`��Ăяo���� Scene / GameObject / Component ���B
//    * Renderer �� GPU ���\�[�X�̍쐬/����/�h���[�̍ŏ��� API ��񋟁B
// =======================================================================
class D3D12Renderer
{
public:
    // ----------------------------- �t���[����d�� -----------------------------
    // �o�b�t�@�̌��B2=�_�u���o�b�t�@�B�e�B�A�����O�y���� GPU/CPU �̕��s���m�ہB
    // �� �����I�� 3 (�g���v��) �ɂ���� Present �̑҂�������ꍇ������B
    static const UINT FrameCount = 2;

    // ----------------------------- ���C�t�T�C�N�� -----------------------------
    D3D12Renderer();
    ~D3D12Renderer(); // Cleanup() ���Ă�Ń��\�[�X�j���iComPtr �ɂ����S�j

    // �������F
    //  - �f�o�C�X/�L���[/�X���b�v�`�F�C��/RTV/DSV/�R�}���h�֘A/�p�C�v���C�� ���\�z�B
    //  - ������ true�B���s�� false�i�ڍׂ̓f�o�b�O�o�́j�B
    //  - HWND �͏��L���Ȃ��iWin32 �E�B���h�E�����͌Ăяo�����ŊǗ��j�B
    bool Initialize(HWND hwnd, UINT width, UINT height);

    // �t���[���`��F
    //  - �R�}���h���X�g�� Reset �� �L�^ �� Close �� Execute�B
    //  - Present �� �O�t���[���̊����҂��i�t�F���X�����j�B
    //  - �V�[����J���������ݒ�Ȃ瑁�� return�B
    void Render();

    void WaitForGPU() noexcept;

    void Resize(UINT width, UINT height) noexcept;

    // ���b�V���P�̃h���[�̃w���p�[�F
    //  - ���O�� IA�iVB/IB/�g�|���W�j���Z�b�g���ADrawIndexedInstanced �𔭍s�B
    //  - PSO/RS/DS/CBV �Ȃǂ� Render() ���̗���Őݒ肳��Ă���O��B
    void DrawMesh(MeshRendererComponent* meshRenderer);

    // �I�������i�����I�j���j�F
    //  - GPU �����҂� �� �}�b�v���� �� �n���h������iComPtr �͎��� Release�j�B
    //  - Initialize ���J��Ԃ��ꍇ�̂��߂ɂ��Ăяo���\�ɂ��Ă���B
    void Cleanup();

    // GPU/CPU �����F
    //  - ���݂̃t�F���X�l�� Signal �� ���̒l�ɓ��B����܂őҋ@�B
    //  - �t���[�����\�[�X�i�R�}���h�A���P�[�^���j�̃��Z�b�g���S���ɕK�{�B
    void WaitForPreviousFrame();

    // ----------------------------- �V�[��/�J���� ------------------------------
    // �`��Ώۂ̃V�[����ݒ�i���L���L�j�B
    // �ERenderer �� Scene �̎�����ێ����Ȃ��݌v�ł��悢���A�����ł� shared_ptr �ŎQ�ƁB
    // �EScene �̐ؑւ̓t���[���Ԃň��S�ȃ^�C�~���O�ōs�����ƁB
    void SetScene(std::shared_ptr<Scene> scene) { m_CurrentScene = scene; }

    // �g�p����J������ݒ�F
    // �EView/Projection �� CameraComponent ����擾�B
    // �E�A�X�y�N�g��Ȃǂ̕ύX�� Camera ���� SetAspect �Ȃǂ��Ă�Ŕ��f������B
    void SetCamera(std::shared_ptr<CameraComponent> camera) { m_Camera = camera; }

    // ���b�V���� GPU ���\�[�X�����F
    //  - MeshRendererComponent ������ MeshData ���� Upload �q�[�v�� VB/IB ���쐬���A�R�s�[�B
    //  - VBV/IBV �𖄂߁AIndexCount ��ݒ�B
    //  - �{�֐��́uCPU ���b�V����GPU �o�b�t�@�v�ϊ��̓�����B�e�N�X�`�����͖��Ή��B
    bool CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> meshRenderer);

    // �o�߃t���[�����iPresent �����񐔁j��Ԃ��B�A�j��/���ԃx�[�X�̊ȈՃg���K�ɗ��p�B
    UINT GetFrameCount() const { return m_frameCount; }

private:
    // ======================== D3D12 ��v�I�u�W�F�N�g�Q =======================
    // �� ComPtr �͗�O���S�B�X�R�[�v�I���� Release�B���� Release �͕s�v�B
    Microsoft::WRL::ComPtr<ID3D12Device> device;                // �_���f�o�C�X
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;    // ���ڃR�}���h�L���[
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;          // �o�b�N/�t�����g�o�b�t�@�ؑ�
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;       // RTV �p�q�[�v�iCPU ���j
    UINT rtvDescriptorSize = 0;                                  // RTV �C���N�������g��
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[FrameCount]; // �o�b�N�o�b�t�@�Q

    // ---- �[�x/�X�e���V�� ----
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;       // DSV �p�q�[�v
    Microsoft::WRL::ComPtr<ID3D12Resource>      depthStencilBuffer; // D32F �e�N�X�`��

    // ---- �R�}���h�L�^�n�i�t���[�����ƂɃA���P�[�^�j----
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList; // 1 �{�^�p

    // ---- ���[�g&�p�C�v���C�� ----
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;  // ���\�[�X�������C�A�E�g
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;  // PSO�iVS/PS/RS/DS/Blend ���j

    // ---- �����iCPU-GPU�j----
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;  // GPU ������
    UINT64 fenceValue = 0;                      // �V�O�i���l�i���t���[���C���N�������g�j
    HANDLE fenceEvent = nullptr;                // �ҋ@�p Win32 �C�x���g�i�蓮���Z�b�g�s�v�j

    // ---- �萔�o�b�t�@�i�I�u�W�F�N�g���j----
    // �E�A�b�v���[�h�iCPU ���j�ɑ傫�߂̒P��o�b�t�@���m�ۂ��A�e�I�u�W�F�N�g�ɃI�t�Z�b�g�����B
    // �ECBV �� ShaderVisible �q�[�v��ɓ��I�쐬�i�`�掞�ɎQ��/�R�s�[��p��������_�ɒ��Ӂj�B
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvHeap;  // CBV/SRV/UAV �q�[�v�iGPU ���j
    Microsoft::WRL::ComPtr<ID3D12Resource>      m_constantBuffer; // �A�b�v���[�h�o�b�t�@
    UINT8* m_pCbvDataBegin = nullptr;           // Map �����擪�A�h���X�iUnmap �Y�꒍�Ӂj

    // ============================ ���/�T�C�Y�� ==============================
    UINT m_Width = 0;                          // �N���C�A���g�̈�̕��i�s�N�Z���j
    UINT m_Height = 0;                          // �N���C�A���g�̈�̍����i�s�N�Z���j
    UINT frameIndex = 0;                        // ���݂̃o�b�N�o�b�t�@ Index�iswapChain �R���j
    UINT m_frameCount = 0;                      // Present �񐔁i���v/�A�j������Ȃǁj

    // �\���i���g�p/�g���p�j�B�ʃR�}���h���X�g��ǉ����������Ȃǂɓ]�p�B
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList;

    // ============================ �����w���p�[ ==============================
    // �჌�x���������̕����F���s���� false ��Ԃ��iLog �͓����ŏo�́j
    bool CreateDevice();                         // �A�_�v�^�� �� D3D12CreateDevice
    bool CreateCommandQueue();                   // ���ڃL���[�쐬�iCopy/Compute �͖��쐬�j
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height); // �t���b�v�n�X���b�v
    bool CreateRenderTargetViews();              // �o�b�N�o�b�t�@ �� RTV �쐬
    bool CreateCommandAllocatorsAndList();       // �t���[���ʃA���P�[�^ + ���L CL
    bool CreatePipelineState();                  // ���[�g�V�O�l�`�� + PSO �\�z

    // �[�x�X�e���V���iD32F�j�e�N�X�`���쐬�F
    //  - DSV �q�[�v�� 1 �o�^�BRender() �� OMSetRenderTargets �Ŏg�p�B
    bool CreateDepthStencilBuffer(UINT width, UINT height);

    // ============================ �����x���Q�� ===============================
    // Renderer �� Scene/Camera �́u���p�ҁv�B������ shared_ptr �ŋ��L���邾���B
    std::shared_ptr<Scene>           m_CurrentScene;  // �`��ΏۃV�[��
    std::shared_ptr<CameraComponent> m_Camera;        // �g�p�J�����iView/Proj �̋������j

    // ============================ �����̊g������ ============================
    // �� �ꎞ�I�ȕێ��i�������̖��c�j�F������ CameraComponent �ɏW�񂵂ĎQ�Ƃ݂̗̂\��B
    DirectX::XMMATRIX m_ViewMatrix;        // �g�p���� m_Camera->GetViewMatrix() �𐄏�
    DirectX::XMMATRIX m_ProjectionMatrix;  // ����iSetAspect/FOV �� Camera ���ŊǗ��j
};

// ============================ �^�p/�݌v���� ================================
// �E�X���b�h�Z�[�t�ł͂Ȃ��FInitialize/Render/Cleanup �̓��C���X���b�h�z��B
// �E�𑜓x�ύX/�E�B���h�E���T�C�Y�FResize �Ή���ǉ�����ꍇ�ADSV/RTV/SwapChain ����蒼���B
// �EDescriptor �̎����FCreate*View ��͌����\�[�X���L���ł��邱�Ƃ��K�v�iComPtr �Ǘ��j�B
// �ECBV �̔z�u�FD3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT(256) �ɍ��킹�ăA���C���K�{�B
// �EPresent �̈����F�e�B�A�����O/���������𐧌䂵�����ꍇ�͓K�؂ȃt���O�ɕύX�B
// �E�f�o�b�O���� D3D12 �f�o�b�O���C����L�����iCreateDevice ���� _DEBUG �u���b�N�j�B
// ============================================================================
