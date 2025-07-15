#pragma once

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h> // Microsoft::WRL::ComPtr
#include <DirectXMath.h> // DirectX::XMFLOAT4X4 �Ȃ�
#include <vector>
#include <memory> // std::shared_ptr
#include <functional> // std::function �̂��߂ɒǉ�

// �ǉ�
#include "GameObject.h" // GameObject�N���X���C���N���[�h
#include "Mesh.h"       // MeshRendererComponent�Ȃǂ��C���N���[�h

// Direct3D 12�w���p�[���C�u���� (d3dx12.h) �̓v���W�F�N�g�Ɋ܂߂Ďg�p
#include "d3dx12.h" // ���ꂪ�v���W�F�N�g�Ɋ܂܂�Ă��邱�Ƃ��m�F���Ă�������

// �萔�o�b�t�@�̍\���� (�V�F�[�_�[�Ɠ���)
struct SceneConstantBuffer
{
    DirectX::XMFLOAT4X4 mvp; // Model-View-Projection �s��
};

class D3D12Renderer
{
public:
    static const UINT FrameCount = 2; // �_�u���o�b�t�@�����O

    D3D12Renderer();
    ~D3D12Renderer();

    // ������: D3D12�f�o�C�X�A�X���b�v�`�F�C���A�R�}���h���X�g�Ȃǂ��Z�b�g�A�b�v
    bool Initialize(HWND hwnd, UINT width, UINT height);
    // �����_�����O���[�v: �R�}���h���L�^���AGPU�ɑ��M���A�\��
    void Render();
    // �N���[���A�b�v: ���\�[�X�̉��
    void Cleanup();
    // �O�̃t���[������������̂�ҋ@
    void WaitForPreviousFrame();

    // === �V�����p�u���b�N�֐� ===
    // �V�[����ݒ�
    void SetScene(std::shared_ptr<Scene> scene) { m_CurrentScene = scene; }

    // GameObject���烁�b�V�������_���[�R���|�[�l���g�����o���A����D3D12���\�[�X���쐬�E����������
    // (Public �ɂ��邱�Ƃ� main.cpp ����Ăяo����悤�ɂ���)
    bool CreateMeshRendererResources(std::shared_ptr<MeshRendererComponent> meshRenderer);

    // �A�j���[�V�����t���[���J�E���g�̃Q�b�^�[
    UINT GetFrameCount() const { return m_frameCount; }

private:
    // === D3D12�I�u�W�F�N�g ===
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    UINT rtvDescriptorSize;
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[FrameCount];

    // ���ǉ�: �[�x�o�b�t�@�֘A
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap; // �[�x�X�e���V���r���[�q�[�v
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilBuffer; // �[�x�X�e���V���o�b�t�@���\�[�X

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

    // === �����I�u�W�F�N�g ===
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    UINT64 fenceValue;
    HANDLE fenceEvent;

    // === �萔�o�b�t�@�֘A ===
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
    SceneConstantBuffer m_constantBufferData;
    UINT8* m_pCbvDataBegin; // �}�b�v���ꂽ�萔�o�b�t�@�̐擪�|�C���^

    // === �`��֘A ===
    UINT m_Width;
    UINT m_Height;
    UINT frameIndex;
    UINT m_frameCount; // �A�j���[�V�����p

    // === �v���C�x�[�g�w���p�[�֐� ===
    bool CreateDevice();
    bool CreateCommandQueue();
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height);
    bool CreateRenderTargetViews();
    bool CreateCommandAllocatorsAndList();
    bool CreatePipelineState();

    // ���ǉ�: �[�x�o�b�t�@�̍쐬�֐�
    bool CreateDepthStencilBuffer(UINT width, UINT height);

    // ���݂̃V�[����ێ�
    std::shared_ptr<Scene> m_CurrentScene;

    // �J�����̃r���[�s��ƃv���W�F�N�V�����s��i���CameraComponent�Ɉڂ��j
    DirectX::XMMATRIX m_ViewMatrix;
    DirectX::XMMATRIX m_ProjectionMatrix;
};