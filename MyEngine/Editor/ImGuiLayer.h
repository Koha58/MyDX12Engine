#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <windows.h>
#include "imgui.h"

// �O���錾�iEditor ���̏�Ԃ��󂯓n�����߂̌y�� POD�j
struct EditorContext;

/*
    ImGuiLayer
    ----------------------------------------------------------------------------
    �ړI�F
      - Dear ImGui �� DirectX 12 �ŋ쓮���邽�߂̔������b�p�B
      - �������^�t���[�����E�^�`��R�}���h�L�^�^�I������ ����{���B
      - �C�ӂ̃e�N�X�`���iID3D12Resource�j�ɑ΂��� SRV ���u�w��X���b�g�v�ɍ��A
        ImGui::Image �Ŏg���� ImTextureID ��Ԃ����[�e�B���e�B��񋟁B

    �݌v�����F
      - SRV �́uGPU ���� CBV/SRV/UAV �q�[�v�iShader Visible�j�v�ɍ��B
      - ImTextureID �� imgui_impl_dx12 �̎d�l�ɂ�� �gGPU �f�B�X�N���v�^�n���h���h �𐮐����������́B
      - �t���[�����̃_�u��/�g���v���o�b�t�@�ƏՓ˂��Ȃ��悤�A�Ăяo������
          baseSlot + frameIndex
        �̌`�ŃX���b�g���Ǘ�����i��FScene �p�� 1000 �ԑ�AGame �p�� 2000 �ԑ�Ȃǁj�B
      - CreateOrUpdateTextureSRV() �͓���X���b�g�̍Ĕ��s�����e�i�㏑���j����B
        �e�N�X�`�����̎����Ǘ��iGPU �g�p���ɉ�����Ȃ��j�͌Ăяo�����̐Ӗ��B
      - �X���b�h�Z�[�t�ł͂Ȃ��iUI �X���b�h/�`��X���b�h����̂݌ĂԑO��j�B
*/
class ImGuiLayer
{
public:
    /*
        Initialize
        ------------------------------------------------------------------------
        - Win32 + DX12 �p�� ImGui ���������������A���� SRV �q�[�v���m�ۂ���B
        - numFramesInFlight �� imgui_impl_dx12 �̃t���[�����i�ʏ�� SwapChain �̐��j�B
        - rtvFormat/dsvFormat �� ImGui �̃����_�[�^�[�Q�b�g�p�ݒ�i�p�C�v���C�������̂��߁j�B
        �߂�l�F������ true
    */
    bool Initialize(HWND hwnd,
        ID3D12Device* device,
        ID3D12CommandQueue* queue,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat,
        UINT numFramesInFlight);

    /*
        NewFrame
        ------------------------------------------------------------------------
        - 1�t���[���� ImGui �L�^���J�n�iImGui::NewFrame �̃��b�p�j�B
        - ImGui �E�B�W�F�b�g�\�z�iBuildDockAndWindows�j���O�ɌĂԁB
    */
    void NewFrame();

    /*
        BuildDockAndWindows
        ------------------------------------------------------------------------
        - �h�b�L���O�X�y�[�X���\�z���A�e��p�l���iHierarchy / Inspector / Viewport ���j
          �̃E�B���h�E�𐶐�����B
        - EditorContext ����āA�e�N�X�`��ID��\���T�C�Y�AHover/Focus ��ԂȂǂ�
          �󂯎��/�����߂��B
    */
    void BuildDockAndWindows(EditorContext& ctx);

    /*
        Render
        ------------------------------------------------------------------------
        - ImGui �̕`��R�}���h���󂯎��Acmd�iID3D12GraphicsCommandList�j�֋L�^�B
        - ���O�� OM�i�o�b�N�o�b�t�@�� RTV �ݒ�Ȃǁj���ς�ł���z��B
        - ���̊֐��́u1�t���[����1��v�ĂԁB
    */
    void Render(ID3D12GraphicsCommandList* cmd);

    /*
        Shutdown
        ------------------------------------------------------------------------
        - ImGui �����̃��\�[�X����B�A�v���I�����ɌĂԁB
    */
    void Shutdown();

    /*
        CreateOrUpdateTextureSRV
        ------------------------------------------------------------------------
        �ړI�F
          - �w��� D3D12 �e�N�X�`���iResource�j�ɑΉ����� SRV ���A
            �gCPU/GPU �f�B�X�N���v�^�q�[�v�̎w��X���b�g�h �ɍ쐬/�X�V�B
          - ImGui::Image �ɓn���� ImTextureID ��Ԃ��i��GPU �f�B�X�N���v�^�n���h���𐮐����j�B

        �����F
          tex  : SRV ���쐬������ ID3D12Resource�iTexture2D �Ȃǁj�B
          fmt  : SRV �̃t�H�[�}�b�g�iDXGI_FORMAT_R8G8B8A8_UNORM ���j�B
          slot : �q�[�v��̃C���f�b�N�X�B�t���[���ʂŏՓ˂��Ȃ��悤�Ăяo�����ŊǗ��B

        �d�v�F
          - ���̊֐��͓����q�[�v�͈̔̓`�F�b�N���s���z��i�������� assert / �g���j�B
          - ���� slot �֕�����ĂԂƏ㏑���iUpdate�j�ɂȂ�B
          - tex �̎����͌Ăяo�������Ǘ�����iGPU �g�p�� Release �h�~�͕ʑw�Łj�B
    */
    ImTextureID CreateOrUpdateTextureSRV(ID3D12Resource* tex, DXGI_FORMAT fmt, UINT slot);

private:
    // GPU �� SRV �q�[�v�iimgui_impl_dx12 �݊��j
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;

    // �������ς݃t���O�i���d������/�I��������邽�߁j
    bool m_initialized = false;

    // �֋X�I�ɕێ��FSRV �쐬�ɕK�v�i�C���N�������g�T�C�Y���_�n���h���j
    ID3D12Device* m_device = nullptr;
    UINT m_srvIncSize = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvCpuStart{}; // �q�[�v�擪�� CPU �n���h��
    D3D12_GPU_DESCRIPTOR_HANDLE m_srvGpuStart{}; // �q�[�v�擪�� GPU �n���h��
};
