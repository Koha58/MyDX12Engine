#pragma once // �����w�b�_�[�t�@�C����������C���N���[�h�����̂�h���v���v���Z�b�T�f�B���N�e�B�u

#include <windows.h>      // Windows API�̊�{�@�\
#include <d3d12.h>        // Direct3D 12 API�̃R�A�w�b�_�[
#include <dxgi1_4.h>      // DXGI (DirectX Graphics Infrastructure) API�̃w�b�_�[
#include <wrl/client.h>   // Microsoft::WRL::ComPtr �̂��߂ɕK�v�BCOM�I�u�W�F�N�g�̃X�}�[�g�|�C���^
#include <DirectXMath.h>  // DirectXMath���C�u�����B�x�N�g���A�s��Ȃǂ̐��w�֐�
#include <vector>         // std::vector���g�p
#include <memory>         // std::shared_ptr���g�p
#include <functional>     // std::function ���g�p (����͒��ڎg���Ă��Ȃ����A�R�[���o�b�N�ȂǂŗL�p)

// �J�X�^���N���X�̃w�b�_�[
#include "GameObject.h"   // �Q�[�����̃I�u�W�F�N�g�\���Ɠ�����`
#include "Mesh.h"         // ���b�V���f�[�^�\����MeshRendererComponent���`
#include "Scene.h"

// Direct3D 12�w���p�[���C�u���� (d3dx12.h)
// ���̃t�@�C���͒ʏ�AMicrosoft.Direct3D.D3D12 NuGet�p�b�P�[�W�Ɋ܂܂�Ă���
#include "d3dx12.h" // D3D12�I�u�W�F�N�g�̍쐬���ȑf�����郆�[�e�B���e�B�֐����

// �萔�o�b�t�@�̍\���� (�V�F�[�_�[��C++�R�[�h�ԂŃf�[�^�𓯊�)
// �V�F�[�_�[��CBV�iConstant Buffer View�j�ɓn�����f�[�^�\���ƈ�v������K�v������
struct SceneConstantBuffer
{
    DirectX::XMFLOAT4X4 mvp; // Model-View-Projection �s��
};

// --- D3D12Renderer�N���X�̒�` ---
// Direct3D 12 API��p���������_�����O�������Ǘ�����N���X
class D3D12Renderer
{
public:
    // �t���[���o�b�t�@�̐� (�_�u���o�b�t�@�����O�̏ꍇ��2)
    static const UINT FrameCount = 2;

    // �R���X�g���N�^�ƃf�X�g���N�^
    D3D12Renderer();
    ~D3D12Renderer();

    // �������֐�:
    // D3D12�f�o�C�X�A�X���b�v�`�F�C���A�R�}���h���X�g�Ȃǂ�D3D12��{�R���|�[�l���g���Z�b�g�A�b�v
    // @param hwnd: �����_�����O�Ώۂ̃E�B���h�E�n���h��
    // @param width: �N���C�A���g�̈�̕�
    // @param height: �N���C�A���g�̈�̍���
    // @return: �����������������ꍇ��true�A���s�����ꍇ��false
    bool Initialize(HWND hwnd, UINT width, UINT height);

    // �����_�����O���[�v�̒��S:
    // �R�}���h���L�^���AGPU�ɑ��M���A���ʂ�\������
    void Render();

    // �N���[���A�b�v�֐�:
    // �쐬���ꂽD3D12���\�[�X���������
    void Cleanup();

    // �����֐�:
    // �O�̃t���[����GPU��������������̂�ҋ@����
    void WaitForPreviousFrame();

    // === �V�����p�u���b�N�֐� ===
    // �����_�����O�Ώۂ̃V�[����ݒ肷��
    // @param scene: �����_�����O����Scene�I�u�W�F�N�g��shared_ptr
    void SetScene(std::shared_ptr<Scene> scene) { m_CurrentScene = scene; }

    // GameObject����MeshRendererComponent�����o���A
    // ����Ɋ֘A����D3D12���\�[�X�i���_�o�b�t�@�A�C���f�b�N�X�o�b�t�@�Ȃǁj���쐬�E����������
    // ���̊֐���`main.cpp`����Ăяo����ACPU����MeshData��GPU�����p�ł���`���ɕϊ�����
    // @param meshRenderer: ���\�[�X���쐬����MeshRendererComponent��shared_ptr
    // @return: ���\�[�X�쐬�����������ꍇ��true�A���s�����ꍇ��false
    bool CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> meshRenderer);

    // �A�j���[�V�����t���[���J�E���g�̃Q�b�^�[
    // ���݂̃t���[�������擾���A�A�j���[�V�����⎞�ԃx�[�X�̃��W�b�N�ɗ��p�ł���
    // @return: ���݂̃t���[���J�E���g
    UINT GetFrameCount() const { return m_frameCount; }

private:
    // === D3D12��v�I�u�W�F�N�g���Ǘ�����ComPtr�����o�[ ===
    // ComPtr��COM�C���^�[�t�F�[�X�������I�ɊǗ����A���������[�N��h���X�}�[�g�|�C���^
    Microsoft::WRL::ComPtr<ID3D12Device> device;             // D3D12�f�o�C�X
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue; // GPU�ւ̃R�}���h���s�L���[
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;       // �t�����g�o�b�t�@�ƃo�b�N�o�b�t�@���Ǘ����A�\����؂�ւ���
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;    // �����_�[�^�[�Q�b�g�r���[�iRTV�j�̂��߂̃f�B�X�N���v�^�q�[�v
    UINT rtvDescriptorSize;                                  // RTV�f�B�X�N���v�^�̃T�C�Y�i�n�[�h�E�F�A�ɂ���ĈقȂ�j
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[FrameCount]; // �e�t���[���̃����_�[�^�[�Q�b�g���\�[�X

    // �[�x�o�b�t�@�֘A
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;          // �[�x�X�e���V���r���[�iDSV�j�̂��߂̃f�B�X�N���v�^�q�[�v
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilBuffer;     // �[�x�X�e���V���o�b�t�@���\�[�X

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators[FrameCount]; // �e�t���[���̃R�}���h�A���P�[�^
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList; // GPU�ɑ��M����R�}���h���L�^���郊�X�g
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;     // �V�F�[�_�[���A�N�Z�X���郊�\�[�X�̃��C�A�E�g���`
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;     // �O���t�B�b�N�X�p�C�v���C���̏�ԁi�V�F�[�_�[�A�u�����h�ݒ�Ȃǁj���`

    // === �����I�u�W�F�N�g ===
    Microsoft::WRL::ComPtr<ID3D12Fence> fence; // GPU��CPU�̓����I�u�W�F�N�g
    UINT64 fenceValue;                         // �t�F���X�̌��ݒl
    HANDLE fenceEvent;                         // �t�F���X�C�x���g�������ɒʒm�����Win32�C�x���g�n���h��

    // === �萔�o�b�t�@�֘A ===
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvHeap;       // �萔�o�b�t�@�r���[�iCBV�j�̂��߂̃f�B�X�N���v�^�q�[�v
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;      // �萔�o�b�t�@��GPU���\�[�X
    SceneConstantBuffer m_constantBufferData;                      // CPU���ŕێ�����萔�o�b�t�@�̃f�[�^
    UINT8* m_pCbvDataBegin;                                        // �}�b�v���ꂽ�萔�o�b�t�@�̐擪�|�C���^

    // === �`��֘A�̃����o�[�ϐ� ===
    UINT m_Width;                                                  // �N���C�A���g�̈�̕�
    UINT m_Height;                                                 // �N���C�A���g�̈�̍���
    UINT frameIndex;                                               // ���݂̃o�b�N�o�b�t�@�̃C���f�b�N�X
    UINT m_frameCount;                                             // �A�j���[�V�����ȂǂɎg�p����t���[���J�E���^�[

    // === �v���C�x�[�g�w���p�[�֐� ===
    // D3D12�f�o�C�X���쐬����
    bool CreateDevice();
    // �R�}���h�L���[���쐬����
    bool CreateCommandQueue();
    // �X���b�v�`�F�C�����쐬����
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height);
    // �����_�[�^�[�Q�b�g�r���[���쐬����
    bool CreateRenderTargetViews();
    // �R�}���h�A���P�[�^�ƃR�}���h���X�g���쐬����
    bool CreateCommandAllocatorsAndList();
    // �p�C�v���C���X�e�[�g�I�u�W�F�N�g���쐬����
    bool CreatePipelineState();

    // �[�x�o�b�t�@�̍쐬�֐�
    // �[�x�e�X�g�ƃX�e���V���e�X�g�Ɏg�p����[�x�X�e���V���o�b�t�@���쐬����
    // @param width: �[�x�o�b�t�@�̕�
    // @param height: �[�x�o�b�t�@�̍���
    // @return: �쐬�����������ꍇ��true�A���s�����ꍇ��false
    bool CreateDepthStencilBuffer(UINT width, UINT height);

    // ���݃����_�����O����Ă���V�[���ւ�shared_ptr
    std::shared_ptr<Scene> m_CurrentScene;

    // �J�����̃r���[�s��ƃv���W�F�N�V�����s��
    // �����_�ł�D3D12Renderer���ɒ��ڕێ�����Ă��邪�A
    // �����I�ɂ�CameraComponent�̂悤�Ȑ�p�̃R���|�[�l���g�Ɉړ�����\��
    DirectX::XMMATRIX m_ViewMatrix;
    DirectX::XMMATRIX m_ProjectionMatrix;
};