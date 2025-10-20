#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <vector>

/*
    DeviceResources
    ----------------------------------------------------------------------------
    �����F
      - DXGI / D3D12 �́u�y��v�ɂȂ郊�\�[�X���ЂƂ܂Ƃ߂ɊǗ����郆�[�e�B���e�B�B
        * �f�o�C�X�iID3D12Device�j
        * �R�}���h�L���[�iID3D12CommandQueue�j
        * �X���b�v�`�F�C���iIDXGISwapChain4�j
        * �o�b�N�o�b�t�@�iID3D12Resource[]�j�{ RTV �q�[�v
        * �[�x�o�b�t�@�iID3D12Resource�j�{ DSV �q�[�v
      - �E�B���h�E�̃��T�C�Y���ɕK�v�ȁu�T�C�Y�ˑ����\�[�X�i�o�b�N�o�b�t�@/�[�x�j�v��
        ��蒼���������Ŋ���������B

    �|���V�[�F
      - Initialize() �� Resize() �� Present() �̒���g�p��z��i�}���`�X���b�h��Ή��j�B
      - �T�C�Y�Ɉˑ�������̂� ReleaseSizeDependentResources() �ň�U������Ă���č쐬�B
      - RTV/DSV �̃t�H�[�}�b�g�͌Œ�i�K�v�Ȃ�Z�b�^��ǉ����ăv���W�F�N�g���j�ɍ��킹�ĕύX�j�B
      - �G���[�n���h�����O�� bool �߂�l�B�ڍׂȃ��O�͏�ʑw�܂��� dxdbg �Ȃǂōs���B
*/
class DeviceResources
{
public:
    DeviceResources();               // �� �ǉ��F�R���X�g���N�^�錾�i�����l�̓����o�������q�ɂāj
    ~DeviceResources();

    /*
        Initialize
        ------------------------------------------------------------------------
        - D3D12 �f�o�C�X�A�R�}���h�L���[�A�X���b�v�`�F�C���ARTV/DSV �𐶐��B
        - �ŏ��̃E�B���h�E�T�C�Y�iwidth/height�j�ƃt���[�����iframeCount�j���󂯎��B
        �߂�l�F������ true
    */
    bool Initialize(HWND hwnd, UINT width, UINT height, UINT frameCount);

    /*
        Resize
        ------------------------------------------------------------------------
        - �E�B���h�E�N���C�A���g�T�C�Y�̕ύX�ɔ����A�T�C�Y�ˑ����\�[�X����蒼���B
        - ���O�� GPU �����҂��i�O���j���s���Ă���ĂԂ̂����S�B
    */
    void Resize(UINT width, UINT height);

    /*
        Present
        ------------------------------------------------------------------------
        - �X���b�v�`�F�C�� Present�BsyncInterval=1 �Ő��������҂��̈�ʓI�Ȑݒ�B
    */
    void Present(UINT syncInterval);

    // ------------------------- Getters�i�y�ʃA�N�Z�T�Q�j -------------------------
    ID3D12Device* GetDevice() const { return m_device.Get(); }
    ID3D12CommandQueue* GetQueue()  const { return m_queue.Get(); }
    ID3D12Resource* GetBackBuffer(UINT index) const { return m_backBuffers[index].Get(); }

    // �o�b�N�o�b�t�@ i �Ԃ� RTV �n���h��
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(UINT index) const;

    // �[�x�X�e���V���� DSV �n���h��
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const;

    // �p�C�v���C���݊��p�� RTV/DSV �̃t�H�[�}�b�g�����J
    DXGI_FORMAT GetRTVFormat() const { return m_rtvFormat; }
    DXGI_FORMAT GetDSVFormat() const { return m_dsvFormat; }

    // ���݂̃o�b�N�o�b�t�@�C���f�b�N�X�i�X���b�v�`�F�C�����璼�擾�j
    UINT GetCurrentBackBufferIndex() const { return m_swapChain->GetCurrentBackBufferIndex(); }

    // ���s�̃N���C�A���g��/�����i�o�b�N�o�b�t�@�ƈ�v�j
    UINT GetWidth()  const { return m_width; }
    UINT GetHeight() const { return m_height; }

private:
    // ------------------------- �����w���p�i�����菇�𕪊��j -------------------------
    bool CreateDevice();                                                  // �A�_�v�^�I�� �� ID3D12Device ����
    bool CreateCommandQueue();                                            // DIRECT �^�C�v�̃L���[
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height, UINT frameCount); // �X���b�v�`�F�C������
    bool CreateRTVs(UINT frameCount);                                     // RTV �q�[�v�{�e�o�b�N�o�b�t�@ RTV
    bool CreateDSV(UINT width, UINT height);                              // DSV �q�[�v�{�[�x�o�b�t�@
    void ReleaseSizeDependentResources();                                 // �o�b�N�o�b�t�@/�[�x�����

private:
    // ------------------------- Core -------------------------
    Microsoft::WRL::ComPtr<ID3D12Device>       m_device;     // ���X�̃t�@�N�g��
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue;      // ��o��iDIRECT�j
    Microsoft::WRL::ComPtr<IDXGISwapChain4>    m_swapChain;  // �\���ʂ̓�d/�O�d�o�b�t�@

    // ------------------------- RTV / DSV -------------------------
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;  // �o�b�N�o�b�t�@�p RTV �q�[�v
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;  // �[�x�p DSV �q�[�v
    UINT        m_rtvStride = 0;                             // RTV �n���h���ԃC���N�������g
    DXGI_FORMAT m_rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;    // ����̃o�b�N�o�b�t�@�t�H�[�}�b�g
    DXGI_FORMAT m_dsvFormat = DXGI_FORMAT_D32_FLOAT;         // ����̐[�x�t�H�[�}�b�g

    // ------------------------- Back Buffers -------------------------
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_backBuffers; // SwapChain �̊e�摜

    // ------------------------- Depth Buffer -------------------------
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depth;          // �[�x�X�e���V���e�N�X�`��

    // ------------------------- Size -------------------------
    UINT m_width = 0;   // ���݂̃N���C�A���g���i�o�b�N�o�b�t�@���j
    UINT m_height = 0;   // ���݂̃N���C�A���g���i�o�b�N�o�b�t�@���j
};
