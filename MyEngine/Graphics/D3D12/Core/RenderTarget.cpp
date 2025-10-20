#include "RenderTarget.h"
#include "d3dx12.h"
#include "Editor/ImGuiLayer.h" 

// ----------------------------------------------------------------------------
// �ړI�F���ƂŃN���b�V�����/�����ǐՂ��₷���悤�ART���ǂ̏u��/��Ԃɂ��邩���o���B
//       �i�y�ʃ��O�B�K�v�Ȃ�{�i���K�[�ɒu�������j
// ----------------------------------------------------------------------------
#include <sstream>
static void RT_LogPtr(const char* tag, ID3D12Resource* color, ID3D12Resource* depth)
{
    std::ostringstream oss;
    oss << "[RT][" << tag << "] color=" << color << " depth=" << depth << "\n";
    OutputDebugStringA(oss.str().c_str());
}


// ----------------------------------------------------------------------------
// DX_CALL�FHRESULT ���`�F�b�N���đ� return false�BRenderTarget.* �� bool ��Ԃ��݌v�B
//           ���{�ԃR�[�h�ł͗�O or ���O�t�G���[�R�[�h�ԋp�Ȃǂɒu��������OK�B
// ----------------------------------------------------------------------------
#ifndef DX_CALL
#define DX_CALL(x) do {                                           \
    HRESULT _hr = (x);                                            \
    if (FAILED(_hr)) {                                            \
        char _buf[256];                                           \
        sprintf_s(_buf, "%s failed (hr=0x%08X)\n", #x, (unsigned)_hr); \
        OutputDebugStringA(_buf);                                  \
        return false;                                             \
    }                                                             \
} while (0)
#endif


// ============================================================================
// Create
// �����FRenderTargetDesc(d) �ɏ]���� Color/Depth ���\�[�X�� RTV/DSV �q�[�v�𐶐��B
// �O��Fdev != nullptr, d.width/height > 0
// ���ӁF������Ԃ� COLOR=RENDER_TARGET, DEPTH=DEPTH_WRITE �ɂ��Ă����iClear����Ɏg����j�B
//       sRGB ���K�v�Ȃ�t�H�[�}�b�g��p�C�v���C�����œ��ꂷ��iBackBuffer�ƍ������Ӂj�B
// ============================================================================
bool RenderTarget::Create(ID3D12Device* dev, const RenderTargetDesc& d)
{
    if (!dev) return false;
    if (d.width == 0 || d.height == 0) return false; // ��0�T�C�Y�쐬�֎~�i�s���A�N�Z�X�h�~�j

    // �ݒ��ێ��iResize �Ŏg���񂷁j
    m_desc = d;

    // --- RTV heap�iColor�p�j ---
    {
        D3D12_DESCRIPTOR_HEAP_DESC r{};
        r.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        r.NumDescriptors = 1;                                 // �P��RTV
        r.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;            // CPU��p
        DX_CALL(dev->CreateDescriptorHeap(&r, IID_PPV_ARGS(&m_rtvHeap)));
        m_rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    }

    // �� Color/Depth �̗����Ŏg���u���ʁv�q�[�v�v���p�e�B���O���ɏo��
    CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);      // GPU���[�J��

    // --- Color �e�N�X�`���{�� ---
    {
        // Tex2D(desc, width, height, arraySize=1, mipLevels=1)
        auto rd = CD3DX12_RESOURCE_DESC::Tex2D(d.colorFormat, d.width, d.height);
        rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;   // RTV �Ƃ��Ďg�p

        // Clear���̍œK���p�l�i�����_�[�^�[�Q�b�g��Color���Z�b�g�j
        D3D12_CLEAR_VALUE c{};
        c.Format = d.colorFormat;
        memcpy(c.Color, d.clearColor, sizeof(float) * 4);

        // ������Ԃ� RENDER_TARGET �ɂ��Ă����ƁA���̂܂� Clear/Bind �ł���
        DX_CALL(dev->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_RENDER_TARGET, &c, IID_PPV_ARGS(&m_color)));

        // RTV �쐬�i�f�t�H���g�L�q�q�j
        dev->CreateRenderTargetView(m_color.Get(), nullptr, m_rtv);
        m_colorState = D3D12_RESOURCE_STATE_RENDER_TARGET; // ������Ԃ����킹��i�o���A�v��̋N�_�j
    }

    // --- Depth�i�K�v�ȏꍇ�̂݁j---
    if (d.depthFormat != DXGI_FORMAT_UNKNOWN) {
        // DSV heap�iCPU��p�j
        D3D12_DESCRIPTOR_HEAP_DESC dh{};
        dh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dh.NumDescriptors = 1;
        DX_CALL(dev->CreateDescriptorHeap(&dh, IID_PPV_ARGS(&m_dsvHeap)));
        m_dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

        // Depth �e�N�X�`���{��
        auto dd = CD3DX12_RESOURCE_DESC::Tex2D(d.depthFormat, d.width, d.height);
        dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE dc{};
        dc.Format = d.depthFormat;
        dc.DepthStencil.Depth = d.clearDepth;    // ���� 1.0
        // Stencil ���g���ꍇ�� .Stencil ���K�؂�

        DX_CALL(dev->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &dd,     // �� ���ʂ� hp �����̂܂ܗ��p
            D3D12_RESOURCE_STATE_DEPTH_WRITE, &dc, IID_PPV_ARGS(&m_depth)));

        // DSV �쐬
        dev->CreateDepthStencilView(m_depth.Get(), nullptr, m_dsv);
    }

#ifdef _DEBUG
    // �������̃q���g���iVS/PIX ���ŒH��₷���j
    if (m_color) m_color->SetName(L"RenderTarget.Color");
    if (m_depth) m_depth->SetName(L"RenderTarget.Depth");
#endif

    RT_LogPtr("Create", m_color.Get(), m_depth.Get());

    // ImGui SRV �̒x���쐬�ɔ�����ID���N���A
    m_imguiTexId = 0; // ���w�b�_���� 'mutable' �w�肪����O��iconst���\�b�h����G�邽�߁j
    return true;
}


// ============================================================================
// Resize
// �����F���݂�RT����U Release() ���A���� desc ��V���� w/h �� Create() �������B
// ���ӁFGPU���Ŏg�p���̉\��������ꍇ�́A�Ăяo�����Ńt�F���X�����҂����ς܂��Ă���B
//       �i���̃��\�b�h���̂́u���S�ȏu�ԂɌĂ΂��v�O��j
// ============================================================================
bool RenderTarget::Resize(ID3D12Device* dev, UINT w, UINT h)
{
    if (w == 0 || h == 0) return false;                          // 0�T�C�Y�͖���
    if (w == m_desc.width && h == m_desc.height) return false;   // ���� no-op

    RT_LogPtr("Resize.Begin", m_color.Get(), m_depth.Get());      // �����\�[�X�̐����m�F�ɖ𗧂�
    Release();                                                    // �Q�Ƃ�؂�iResizeBuffers ���l�̍�@�j
    m_desc.width = w; m_desc.height = h;
    const bool ok = Create(dev, m_desc);                          // �č쐬�i�������/�N���A�l�������j
    RT_LogPtr(ok ? "Resize.End.OK" : "Resize.End.FAIL", m_color.Get(), m_depth.Get());
    return ok;
}


// ============================================================================
// Release
// �����F���L���Ă��� Color/Depth �Ɗe�q�[�v���J���B������Ԃ�����l�ɖ߂��B
// ���ӁFDetach() �ƈႢ�A�g�n���h���̏��L�����O�֓n���Ȃ��h�j���B
//       �x���j�����K�v�Ȃ� Detach() �� GpuGarbageQueue �ցB
// ============================================================================
void RenderTarget::Release()
{
    // ImGui ������Q�Ƃ���Ȃ��悤 SRV ID ���N���A�iSRV���̂� ImGuiLayer ���̃q�[�v�j
    m_imguiTexId = 0;

    // �[�x �� RTV �̏��ŉ���i�����͌����ł͂Ȃ����A�ǐՂ��₷���j
    m_depth.Reset();
    m_dsvHeap.Reset();

    m_color.Reset();
    m_rtvHeap.Reset();

    // CPU �n���h�������������Ă����iBind/Clear �̌�p�΍�j
    m_rtv = {};
    m_dsv = {};

    // �ȍ~�̃o���A�v��̋N�_�� COMMON �ɖ߂��i���S�ɖ��g�p��ԁj
    m_colorState = D3D12_RESOURCE_STATE_COMMON;

    // ���O�F�����͏�� null �ɂȂ��Ă���͂�
    RT_LogPtr("Release", m_color.Get(), m_depth.Get());
}


// ============================================================================
// TransitionToRT
// �����FColor ���\�[�X�� RENDER_TARGET ��ԂցiOMSetRenderTargets/Clear �O�ɌĂԁj
// ���ӁF�����ŏ�Ԃ��g���b�L���O�B�A���J�ڂ͗}�����ĕs�v�ȃo���A�������B
// ============================================================================
void RenderTarget::TransitionToRT(ID3D12GraphicsCommandList* cmd)
{
    if (m_colorState == D3D12_RESOURCE_STATE_RENDER_TARGET) {
        return; // ���ł�RT��ԂȂ牽�����Ȃ��i�p���j
    }
    RT_LogPtr("TransitionToRT", m_color.Get(), m_depth.Get());
    auto b = CD3DX12_RESOURCE_BARRIER::Transition(
        m_color.Get(), m_colorState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmd->ResourceBarrier(1, &b);
    m_colorState = D3D12_RESOURCE_STATE_RENDER_TARGET; // ������Ԃ��X�V
}


// ============================================================================
// TransitionToSRV
// �����FColor ���\�[�X�� PIXEL_SHADER_RESOURCE ��Ԃցi�V�F�[�_�ǂݏo���p�j
// ���ӁFSRV �����̂� ImGuiLayer ���i�{�N���X�́g��ԁh�����ڍs�j�B
// ============================================================================
void RenderTarget::TransitionToSRV(ID3D12GraphicsCommandList* cmd)
{
    if (m_colorState == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
        return; // ����PSR�Ȃ牽�����Ȃ�
    }
    RT_LogPtr("TransitionToSRV", m_color.Get(), m_depth.Get());
    auto b = CD3DX12_RESOURCE_BARRIER::Transition(
        m_color.Get(), m_colorState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); // ��from �͎����
    cmd->ResourceBarrier(1, &b);
    m_colorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}


// ============================================================================
// Bind
// �����F���݂� RTV/DSV �� OM �Ƀo�C���h�B
// �O��FRenderTargetView/DepthStencilView �͍쐬�ς݁B���\�[�X�� RT ��Ԃɂ��Ă���g���B
// ============================================================================
void RenderTarget::Bind(ID3D12GraphicsCommandList* cmd)
{
    RT_LogPtr("Bind", m_color.Get(), m_depth.Get());
    // DSV �������ꍇ�� nullptr ��n���iColor �̂ݕ`��j
    cmd->OMSetRenderTargets(1, &m_rtv, FALSE, m_depth ? &m_dsv : nullptr);
}


// ============================================================================
// Clear
// �����FRTV/DSV ���N���A�i�������j�B�`��O�ɌĂԂ��Ƃ������B
// �O��FColor �� RENDER_TARGET�ADepth �� DEPTH_WRITE ��Ԃ��]�܂����i�����͑J�ڂ��������Ȃ��j�B
// ============================================================================
void RenderTarget::Clear(ID3D12GraphicsCommandList* cmd)
{
    RT_LogPtr("Clear", m_color.Get(), m_depth.Get());
    cmd->ClearRenderTargetView(m_rtv, m_desc.clearColor, 0, nullptr);
    if (m_depth) {
        cmd->ClearDepthStencilView(
            m_dsv, D3D12_CLEAR_FLAG_DEPTH, m_desc.clearDepth, 0, 0, nullptr);
    }
}


// ============================================================================
// EnsureImGuiSRV
// �����FImGuiLayer ����� Color ���\�[�X�� SRV ���i�w��X���b�g�Łj�쐬/�X�V�B
// �߂�FImTextureID�iDX12 �ł� GPU �� SRV �f�B�X�N���v�^�n���h�����l�߂������j
// ���ӁF���̃��\�b�h�� 'const' ���� m_imguiTexId ���X�V���邽�߁A�w�b�_���ł� 'mutable' ��z��B
//       slot: �t���[���C���f�b�N�X�ŏՓ˂��Ȃ� SRV �X���b�g�i�t�H���g�͒ʏ� slot=0 �\��j�B 
// ============================================================================
ImTextureID RenderTarget::EnsureImGuiSRV(ImGuiLayer* imgui, UINT slot) const
{
    if (!m_imguiTexId) {
        // ImGuiLayer ���� SRV ���쐬�iDX12 �� ImTextureID=GPU�n���h���j
        m_imguiTexId = imgui->CreateOrUpdateTextureSRV(m_color.Get(), m_desc.colorFormat, slot);
        RT_LogPtr("ImGuiSRV.Create", m_color.Get(), m_depth.Get());
    }
    // ���ɍ쐬�ς݂Ȃ炻�̂܂ܕԂ��iImGui ���ł͓���ID���g���񂷁j
    return m_imguiTexId;
}


// ============================================================================
// Detach
// �����F���L���Ă��� Color/Depth/�e�q�[�v�� CPU�n���h���� RenderTargetHandles �Ƃ��āg�����������h�Ԃ��B
//       �� �Ăяo�����i��FViewports/Renderer�j�� GpuGarbageQueue �ɐς�ŁuGPU������ɔj���v�ł���B
// ���ӁF���̃C���X�^���X���́g��h��ԂɂȂ�i�T�C�Y=0, state=COMMON, SRV ID ���Z�b�g�j�B
// ============================================================================
RenderTargetHandles RenderTarget::Detach()
{
    // ���O�Fdetach ����g��RT�h�̃A�h���X��f���i�N���b�V�����̒ǐՂɗL���j
    RT_LogPtr("Detach.Before", m_color.Get(), m_depth.Get());

    RenderTargetHandles out{};
    // ComPtr �̏��L���� out �Ɉڂ��i����RenderTarget����͐؂藣�����j
    out.color = std::move(m_color);
    out.depth = std::move(m_depth);
    out.rtvHeap = std::move(m_rtvHeap);
    out.dsvHeap = std::move(m_dsvHeap);
    // CPU �n���h���̓R�s�[�i�n���h�����̂͌y�ʁB�O����OMSet���Ɏg����j
    out.rtv = m_rtv;
    out.dsv = m_dsv;

    // ���g�͊��S�Ɂg��h�֖߂�
    m_rtv = {};
    m_dsv = {};
    m_imguiTexId = 0;                              // ImGui ������Q�Ƃ���Ȃ��悤��
    m_colorState = D3D12_RESOURCE_STATE_COMMON;    // �o���A�v��̋N�_��
    m_desc.width = m_desc.height = 0;              // 0x0 �Ƃ��Ĉ����i��p���o�ɖ𗧂j

    // ���O�Fdetach ��� out �Ɉڂ����A�h���X���o���i�L���[�̎����ǐՂ��y�ɂȂ�j
    RT_LogPtr("Detach.After(out)", out.color.Get(), out.depth.Get());
    return out;
}
