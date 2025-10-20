#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgiformat.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "imgui_internal.h"

/*
    RenderTarget.h
    ----------------------------------------------------------------------------
    �����F
      - �I�t�X�N���[���`��p�� Color/Depth �e�N�X�`���ƁA���� RTV/DSV ���ЂƂ܂Ƃ߂ɊǗ��B
      - �쐬(Create)�^���(Release)�^�T�C�Y�ύX(Resize)�^��ԑJ��(Transition*)�^
        �o�C���h/�N���A(Bind/Clear) �Ƃ�������A�̑����񋟁B
      - ImGui �֕\�����邽�߂� SRV�iImTextureID�j���A�O���q�[�v�iImGuiLayer�j�ɍ쐬�B

    �݌v�̃|�C���g�F
      - RenderTargetDesc�c�K�v�\���ȍ쐬�p�����[�^�iw/h/format/�N���A�l�j��ێ��B
      - ������ԃg���b�L���O�cColor �̒��߂� D3D12_RESOURCE_STATE ��ێ����A
        �s�v�ȃo���A��}���i�p���� TransitionToRT/TransitionToSRV�j�B
      - Detach() �c ���̃C���X�^���X�̏��L���\�[�X�� RenderTargetHandles �Ɉ�����������
        �Ԃ��i�����́g��h�Ɂj�BGPU �����҂��̒x���j���L���[�֍ڂ���p�r��z��B
      - ImGui SRV �c DX12 �� ImTextureID �� GPU �� SRV �n���h���iUINT64�����j�B
        SRV ���̂� ImGuiLayer ���̃q�[�v�ɍ�邽�߁A�{�N���X�� ID �̃L���b�V���݂̂����B
*/

class ImGuiLayer; // SRV �쐬���Ϗ����郌�C���i�O���錾�j

// ----------------------------------------------------------------------------
// RenderTargetDesc�FRT �쐬�p�����[�^
// ----------------------------------------------------------------------------
struct RenderTargetDesc
{
    UINT        width = 0;                                      // �e�N�X�`�����ipx�j
    UINT        height = 0;                                      // �e�N�X�`�������ipx�j
    DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;        // Color �� DXGI �t�H�[�}�b�g
    DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;             // Depth �� DXGI �t�H�[�}�b�g�i�s�v�Ȃ� UNKNOWN ���w��j
    float       clearColor[4] = { 0, 0, 0, 1 };                  // RTV �N���A�F�iCreate ���̍œK���l�ɂ��g�p�j
    float       clearDepth = 1.0f;                               // DSV �N���A�[�x
};

// ----------------------------------------------------------------------------
// RenderTargetHandles�FDetach() �ŊO�ցg���L���ڏ��h���鑩
//   - GpuGarbageQueue �Ƀt�F���X�ƕR�t���Ēx���j������P�[�X�Ŏg�p�B
//   - CPU �f�B�X�N���v�^�n���h���̓R�s�[�Ŏ����o���AComPtr �� move �ŏ��L����n���B
// ----------------------------------------------------------------------------
struct RenderTargetHandles {
    Microsoft::WRL::ComPtr<ID3D12Resource>       color;          // Color �e�N�X�`��
    Microsoft::WRL::ComPtr<ID3D12Resource>       depth;          // Depth �e�N�X�`���i�C�Ӂj
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;        // RTV �p�� CPU ���q�[�v�i1�G���g���j
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;        // DSV �p�� CPU ���q�[�v�i1�G���g���j
    D3D12_CPU_DESCRIPTOR_HANDLE                  rtv{};          // RTV �� CPU �n���h��
    D3D12_CPU_DESCRIPTOR_HANDLE                  dsv{};          // DSV �� CPU �n���h��
};

// ----------------------------------------------------------------------------
// RenderTarget�FColor/Depth + RTV/DSV ���܂Ƃ߂Ėʓ|�����郆�[�e�B���e�B
// ----------------------------------------------------------------------------
class RenderTarget
{
public:
    /*
        Create
        ------------------------------------------------------------------------
        ���� : RenderTargetDesc �ɏ]���� Color/Depth ���\�[�X�� RTV/DSV �𐶐��B
        �O�� : dev != nullptr, width/height > 0
        ���� : ������Ԃ� Color=RENDER_TARGET, Depth=DEPTH_WRITE�i�N���A����Ɏg�p�j�B
               sRGB ���K�v�ȏꍇ�́A�t�H�[�}�b�g������p�C�v���C�����œ��ꂷ�邱�ƁB
    */
    bool Create(ID3D12Device* dev, const RenderTargetDesc& d);

    /*
        Release
        ------------------------------------------------------------------------
        ���� : ���L���Ă��� Color/Depth �Ɗe�f�B�X�N���v�^�q�[�v��������A���Ԃɖ߂��B
        ���� : �g���L�����O�֓n���Ȃ��h�j���BGPU �Ŏg�p���̉\��������Ȃ� Detach() ���g���A
               �j���� GpuGarbageQueue �ɈϏ����邱�ƁB
    */
    void Release();

    /*
        Resize
        ------------------------------------------------------------------------
        ���� : ���݂� RT �� Release()���V�����T�C�Y�� Create() �������B
        �߂� : ����/���s
        ���� : �Ăяo�����Ńt�F���X������҂��Ă���s�����Ɓi���S�ȃ^�C�~���O�ŌĂԑO��j�B
    */
    bool Resize(ID3D12Device* dev, UINT w, UINT h);

    // ------------------------------------------------------------------------
    // �p�X�擪/�����ł̃��[�e�B���e�B�F������ԃg���b�L���O�t���̙p���o���A
    // ------------------------------------------------------------------------

    // OMSetRenderTargets / Clear �O�ɌĂԁFColor �� RENDER_TARGET
    void TransitionToRT(ID3D12GraphicsCommandList* cmd);

    // �V�F�[�_�ǂݏo���p�r�Ŏg���O�ɌĂԁFColor �� PIXEL_SHADER_RESOURCE
    void TransitionToSRV(ID3D12GraphicsCommandList* cmd);

    // ���݂� RTV/DSV �� OM �o�C���h�iDSV ������� nullptr�j
    void Bind(ID3D12GraphicsCommandList* cmd);

    // RTV/DSV ���N���A�iColor �� m_desc.clearColor�ADepth �� m_desc.clearDepth�j
    void Clear(ID3D12GraphicsCommandList* cmd);

    // ------------------------------------------------------------------------
    // ImGui �\��
    // ------------------------------------------------------------------------
    /*
        EnsureImGuiSRV
        ���� : ImGuiLayer ����� Color ���\�[�X�� SRV ���i�w��X���b�g�Ɂj�쐬/�X�V���A
               ImTextureID�i=GPU �� SRV �n���h���j��Ԃ��B
        ���� : �{�N���X�� SRV �q�[�v�������Ȃ��BSRV ���̂� ImGuiLayer ���ɍ쐬�B
               'const' ���\�b�h���� ID �L���b�V�������������邽�� m_imguiTexId �� mutable�B
    */
    ImTextureID EnsureImGuiSRV(ImGuiLayer* imgui, UINT slot) const;

    // ------------------------------------------------------------------------
    // �Q�ƌn�i���S���FColor ��������� Width/Height �� 0 ��Ԃ��̂����z����
    //         �{�����ł� m_desc ��^�Ƃ���B�g�p���� Color() �� null �𕹂��Ċm�F���j
    // ------------------------------------------------------------------------
    UINT         Width()       const { return m_desc.width; }
    UINT         Height()      const { return m_desc.height; }
    DXGI_FORMAT  ColorFormat() const { return m_desc.colorFormat; }
    ID3D12Resource* Color()    const { return m_color.Get(); }

    /*
        Detach
        ------------------------------------------------------------------------
        ���� : Color/Depth/�e�q�[�v�� CPU �n���h���� RenderTargetHandles �Ƃ��āg�����������h�Ԃ��B
               ���̃C���X�^���X���͊��S�Ɂg��h�֖߂�iSRV ID ���������j�B
        �p�r : ���T�C�Y���ȂǁA�Â� RT �� GPU ������ɔj���������ꍇ�ɁA
               GpuGarbageQueue �֐ςނ��߂̏��L���ڏ��Ɏg�p�B
    */
    RenderTargetHandles Detach();

private:
    // �쐬�p�����[�^�iResize ���ɂ��ė��p�j
    RenderTargetDesc m_desc{};

    // �{�̃��\�[�X
    Microsoft::WRL::ComPtr<ID3D12Resource>            m_color;   // �K�{�FColor �e�N�X�`��
    Microsoft::WRL::ComPtr<ID3D12Resource>            m_depth;   // �C�ӁFDepth �e�N�X�`��

    // �f�B�X�N���v�^�q�[�v�iCPU ���j
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      m_rtvHeap; // RTV 1 �����m��
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      m_dsvHeap; // DSV 1 �����m��

    // CPU �n���h���iOMSet/Clear �p�j
    D3D12_CPU_DESCRIPTOR_HANDLE                       m_rtv{};
    D3D12_CPU_DESCRIPTOR_HANDLE                       m_dsv{};

    // ImGui �p SRV �� ID�iDX12 �ł� GPU �� SRV �n���h�����i�[�����j
    mutable ImTextureID                               m_imguiTexId = 0;

    // Color ���\�[�X�́g���ݏ�ԁh���g���b�N�i�p���o���A�̂��߁j
    D3D12_RESOURCE_STATES m_colorState = D3D12_RESOURCE_STATE_COMMON;
};
