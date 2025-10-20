#include "DeviceResources.h"
#include "d3dx12.h"
#include <cassert>
#include "Debug/DebugHr.h"
// �i����ɐ[�@��f�o�b�O�������j
//   #include <dxgidebug.h> + dxguid.lib
//   �� IDXGIInfoQueue �� DXGI/D3D �� runtime warning ���_���v�\

using Microsoft::WRL::ComPtr;

#include <sstream>
static void DR_Log(const char* s) { OutputDebugStringA(s); OutputDebugStringA("\n"); }
static void DR_LogHR(HRESULT hr, const char* where) {
    if (FAILED(hr)) {
        std::ostringstream oss; oss << "[DR][HR] 0x" << std::hex << hr << " @ " << where;
        OutputDebugStringA(oss.str().c_str()); OutputDebugStringA("\n");
    }
    else {
        std::ostringstream oss; oss << "[DR] OK @ " << where;
        OutputDebugStringA(oss.str().c_str()); OutputDebugStringA("\n");
    }
}


// ============================================================================
// DeviceResources
//   �����FD3D12 Device/Queue/SwapChain/RTV/DSV �̐����ƃ��T�C�Y���ꌳ�Ǘ��B
//   ���ӁF�`��R�}���h�⓯���iFence ���j�͌Ăяo�����̐Ӗ��B
//   DPI/�_�����W�F�����̓s�N�Z���𑜓x�i�����j���������BDPI�͏�ʂŋz���B
// ============================================================================
DeviceResources::DeviceResources()
{
    // �t�H�[�}�b�g�͐�Ɋ���l�����Ď��̂�h���i�����������p�̕ی��j
    m_rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    m_dsvFormat = DXGI_FORMAT_D32_FLOAT;
    m_rtvStride = 0;

    // GetWidth/Height �̕Ԓl���BResize() ���ɍX�V���Ȃ��ƌÂ��l�̂܂܂ɂȂ�_�ɒ��ӁB
    m_width = m_height = 0;
}

DeviceResources::~DeviceResources()
{
    // �T�C�Y�ˑ����\�[�X �� �f�o�C�X��ˑ��̏��ɉ���i�Q�Ƃ�؂��Ă��� Finalize�j
    ReleaseSizeDependentResources();
    m_queue.Reset();
    m_swapChain.Reset();
    m_device.Reset();
}

bool DeviceResources::Initialize(HWND hwnd, UINT width, UINT height, UINT frameCount)
{
    DR_Log("[DR] Initialize begin");

    // �����ł� width/height �́g�����E�B���h�E�𑜓x�h�BGetWidth/Height �̏����l�ɂ�����B
    m_width = width;
    m_height = height;

    // 1) �f�o�C�X�����i�n�[�h�E�F�A�A�_�v�^��I������ D3D12CreateDevice�j
    if (!CreateDevice()) { DR_Log("[DR] CreateDevice FAILED"); return false; }
    DR_Log("[DR] CreateDevice OK");

    // 2) DIRECT �L���[ 1 �{�i�O���t�B�b�N�X�p�j�BCopy/Compute �͕K�v���ɕʓr�B
    if (!CreateCommandQueue()) { DR_Log("[DR] CreateCommandQueue FAILED"); return false; }
    DR_Log("[DR] CreateCommandQueue OK");

    // 3) �X���b�v�`�F�C���� HWND �Ɋ֘A�t���č쐬�iFlip-Discard�j
    //    ��/�����͏����l�B���^�p�ł� WM_SIZE �� Resize() �ɂ���čX�V�����B
    if (!CreateSwapChain(hwnd, width, height, frameCount)) { DR_Log("[DR] CreateSwapChain FAILED"); return false; }
    DR_Log("[DR] CreateSwapChain OK");

    // 4) �o�b�N�o�b�t�@���� RTV ���쐬�iframeCount ���j
    if (!CreateRTVs(frameCount)) { DR_Log("[DR] CreateRTVs FAILED"); return false; }
    DR_Log("[DR] CreateRTVs OK");

    // 5) �[�x�e�N�X�`���{DSV ���g�E�B���h�E�T�C�Y�h�ŗp�ӁiSceneRT �ƕ��������ꍇ�͕ʊǗ��j
    if (!CreateDSV(width, height)) { DR_Log("[DR] CreateDSV FAILED"); return false; }
    DR_Log("[DR] CreateDSV OK");

    DR_Log("[DR] Initialize end OK");
    return true;
}


// ============================================================================
// Resize
//   �ړI�FSwapChain �o�b�t�@�Q + DSV ��V������/�����ō�蒼���B
//   �O��F�Ăяo������ GPU �����҂��ς݁i�g�p���̃o�b�N�o�b�t�@���������܂� ResizeBuffers ��NG�j�B
//   �T�^���[�g�FWM_SIZE �� Renderer::Resize() �� DeviceResources::Resize()�B
//   �d�v�FGetWidth/Height ���ŐV���������Ȃ� ResizeBuffers ������� m_width/m_height ���X�V����B
// ============================================================================
void DeviceResources::Resize(UINT width, UINT height)
{
    // ���O�i�E�B���h�E�ŏ����▢�����������ʂ��鏕���j
    {
        std::ostringstream oss; oss << "[DR] Resize(" << width << "," << height << ")";
        DR_Log(oss.str().c_str());
    }

    // �ŏ���(0x0)�E���������ESwapChain �������̂����ꂩ�Ȃ甲����
    if (width == 0 || height == 0 || !m_device || !m_swapChain) {
        DR_Log("[DR] Resize early return");
        return;
    }

    // ���݂� SC �ݒ���擾�BBufferCount/Format/Flags ���g���񂷁i�Ăяo�������ς��Ă��Ȃ��O��j
    DXGI_SWAP_CHAIN_DESC1 desc{};
    HRESULT hr = m_swapChain->GetDesc1(&desc);
    DR_LogHR(hr, "GetDesc1");
    if (FAILED(hr)) return;

    // �o�b�t�@�����FGetDesc1 ���Ԃ����l��D��B0 �̏ꍇ�͔O�̂��ߌ��݂̔z��T�C�Y���Q�ƁB
    const UINT bufferCount = desc.BufferCount ? desc.BufferCount : (UINT)m_backBuffers.size();
    if (bufferCount == 0) {
        // ���̃P�[�X�͒ʏ픭�����Ȃ��iCreateRTVs �����s�Ȃǁj�B���S�ɒ��f�B
        DR_Log("[DR] Resize bufferCount==0");
        return;
    }

    // 1) �T�C�Y�ˑ����\�[�X�̎Q�Ƃ�؂�i�����ӂ�� ResizeBuffers �� E_INVALIDARG �ȂǂŎ��s���₷���j
    ReleaseSizeDependentResources();

    // 2) �o�b�N�o�b�t�@��V������/�����ōĊm��
    //    DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING ���g���^�p�Ȃ� desc.Flags �Ɋ܂߂�K�v������B
    hr = m_swapChain->ResizeBuffers(bufferCount, width, height, desc.Format, desc.Flags);
    DR_LogHR(hr, "ResizeBuffers");
    if (FAILED(hr)) return;

    // 3) �i�I�v�V�����jGetWidth/Height ���ŐV���������ꍇ�͂����ōX�V
    //    ���ۂ� UI ���� m_dev->GetWidth()/GetHeight() ��\���������Ȃ�K�{�B
    // m_width  = width;
    // m_height = height;

    // 4) �V�����o�b�N�o�b�t�@�ɑ΂��� RTV ���쐬������
    if (!CreateRTVs(bufferCount)) {
        DR_Log("[DR] CreateRTVs FAILED after Resize");
        return;
    }

    // 5) DSV �����l�ɍč쐬�i�����ł̓E�B���h�E�T�C�Y�ɒǐ���������j�j
    if (!CreateDSV(width, height)) {
        DR_Log("[DR] CreateDSV FAILED after Resize");
        return;
    }

    DR_Log("[DR] Resize end OK");
}



// ============================================================================
// Present
//   Flip-Discard �O��BPresent ���s�� TDR ��f�o�C�X���X�g�̒���Ȃ̂� HR ���c���B
//   �σ��t���b�V��/�e�A�����O�� syncInterval/Flags �Ő���i�ʉ^�p�j�B
// ============================================================================
void DeviceResources::Present(UINT syncInterval)
{
    if (!m_swapChain) return;

    // syncInterval: 0=�e�A�����O�iFlags ���̐ݒ���K�v�j / 1=VSync
    // Flags: DXGI_PRESENT_ALLOW_TEARING ��t����ƃe�A�����O���iSwapChain ���� flag �Ƒ΁j
    HRESULT hr = m_swapChain->Present(syncInterval, 0);
    DR_LogHR(hr, "Present");
}



// ============================================================================
// GetRTVHandle
//   RTV �q�[�v�� index �Ԗڂ� CPU �n���h����Ԃ��B
//   ���ӁF�����́g�͈̓`�F�b�N�h�����Ȃ��B�Ăяo������ [0..FrameCount-1] ��ۏ؂���B
// ============================================================================
D3D12_CPU_DESCRIPTOR_HANDLE DeviceResources::GetRTVHandle(UINT index) const
{
    // �����O�ɌĂ΂�Ă��Ȃ����̕ی��Bassert �͊J�����`�F�b�N�p�B
    assert(m_rtvHeap && m_rtvStride != 0 && "RTV heap is not ready");
    D3D12_CPU_DESCRIPTOR_HANDLE h{};
    if (!m_rtvHeap || m_rtvStride == 0) return h; // �����[�X�r���h�ł� null ��Ԃ�

    // �q�[�v�擪 + index * �C���N�������g�T�C�Y
    h = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += SIZE_T(index) * SIZE_T(m_rtvStride);
    return h;
}

// ============================================================================
// GetDSVHandle
//   �P��� DSV ��z��B���������� null �n���h���B
// ============================================================================
D3D12_CPU_DESCRIPTOR_HANDLE DeviceResources::GetDSVHandle() const
{
    D3D12_CPU_DESCRIPTOR_HANDLE h{};
    if (!m_dsvHeap) return h;
    return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}



// ============================================================================
// CreateDevice
//   1) DXGI Factory �쐬�i�f�o�b�O���� DebugLayer + FactoryDebug�j
//   2) �\�t�g�E�F�A�A�_�v�^�����O���ăn�[�h�E�F�A�A�_�v�^��� �� �ŏ��ɒʂ���̂� Device �����B
//      �� WARP fallback ������ꍇ�͎��s���� D3D12CreateDevice(nullptr, ...) �������B
// ============================================================================
bool DeviceResources::CreateDevice()
{
    UINT flags = 0;

#ifdef _DEBUG
    // �f�o�b�O���C���[�I���iGPU Validation �Ȃǂ͊O���ݒ萄���j
    if (Microsoft::WRL::ComPtr<ID3D12Debug> dbg; SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)))) {
        dbg->EnableDebugLayer();
        flags = DXGI_CREATE_FACTORY_DEBUG; // DXGI �̃f�o�b�O�����o��
    }
#endif

    // Factory �͖{�֐������[�J���i�ێ��s�v�j�B�f�o�C�X�쐬�ɂ̂ݎg�p�B
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    HRB(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory))); // ���s���� HRB �����O/��O�B

    // �n�[�h�E�F�A�A�_�v�^�̗񋓁B�v�����x���� D3D_FEATURE_LEVEL_11_0�i�K�v�Ȃ�グ��j�B
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);

        // �\�t�g�E�F�A�iWARP�j�����O
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

        // �ʂ�� m_device �ɃZ�b�g�����B���s�Ȃ玟�̃A�_�v�^�ցB
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device))))
            break;
    }

    // ���ׂĎ��s�����ꍇ�� false�i�Ăяo�����Ń��O�ς݁j
    if (!m_device) return false;
    return true;
}



// ============================================================================
// CreateCommandQueue
//   DIRECT�i�O���t�B�b�N�X�j�L���[�� 1 �{�B�^�C�����C��/Fence �͊O���ŊǗ��B
// ============================================================================
bool DeviceResources::CreateCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC q{};
    q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;   // �`��R�}���h�����{��
    q.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;    // �K�v�ɉ����ă^�C���X�^���v����t�^
    return SUCCEEDED(m_device->CreateCommandQueue(&q, IID_PPV_ARGS(&m_queue)));
}



// ============================================================================
// CreateSwapChain
//   HWND �^�[�Q�b�g�� SwapChain ���쐬�iFlip-Discard, R8G8B8A8_UNORM�j�B
//   Alt+Enter �͗}�~�i�S��ʐؑւ̓A�v�����ŊǗ��j�B
//   ���ӁF�e�A�����O���EHDR�i�J���[��ԁj�͕ʓr�ݒ肪�K�v�B
// ============================================================================
bool DeviceResources::CreateSwapChain(HWND hwnd, UINT width, UINT height, UINT frameCount)
{
    DR_Log("[DR] CreateSwapChain begin");

    // Factory�F�����ł̓��[�J���BSwapChain �쐬�ɂ̂ݎg�p�B
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    UINT flags = 0;
#ifdef _DEBUG
    flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    HRESULT hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory));
    DR_LogHR(hr, "CreateDXGIFactory2");
    if (FAILED(hr)) return false;

    // �ŏ����� SC �ݒ�BTearing �����������Ȃ� Flags/Present �������킹�ėv�ݒ�B
    DXGI_SWAP_CHAIN_DESC1 sc{};
    sc.BufferCount = frameCount;          // �_�u��/�g���v���o�b�t�@
    sc.Width = width;                    // �����o�b�N�o�b�t�@�̕��i��� Resize() �ōX�V�j
    sc.Height = height;                   // ��������
    sc.Format = m_rtvFormat;              // ��: R8G8B8A8_UNORM
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // ����
    sc.SampleDesc = { 1, 0 };             // MSAA ����

    // IDXGISwapChain1 ���܂�����āAIDXGISwapChain4 �ɏ��i
    Microsoft::WRL::ComPtr<IDXGISwapChain1> sc1;
    hr = factory->CreateSwapChainForHwnd(m_queue.Get(), hwnd, &sc, nullptr, nullptr, &sc1);
    DR_LogHR(hr, "CreateSwapChainForHwnd");
    if (FAILED(hr)) return false;

    hr = sc1.As(&m_swapChain);
    DR_LogHR(hr, "As IDXGISwapChain4");
    if (FAILED(hr)) return false;

    // Alt+Enter �� OS ����ɔC���Ȃ��i�A�v������ɂ���j
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    DR_Log("[DR] CreateSwapChain end OK");
    return true;
}



// ============================================================================
// CreateRTVs
//   ���݂� SwapChain ���� BackBuffer ���擾���ARTV ��A�Ԃō쐬�B
//   ���T�C�Y���ɂ��Ă΂�邽�߁A�`���ŋ����\�[�X�̎Q�Ƃ�K���f�B
// ============================================================================
bool DeviceResources::CreateRTVs(UINT frameCount)
{
    // ���o�b�N�o�b�t�@/RTV �q�[�v�̎Q�Ƃ��N���A�iResizeBuffers �́g���ߍ��݁h�������j
    m_backBuffers.clear();                 // vector �̃T�C�Y���ς���̂� clear��resize �̏�
    m_backBuffers.resize(frameCount);
    m_rtvHeap.Reset();
    m_rtvStride = 0;

    // RTV �q�[�v�iCPU ��p�j
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.NumDescriptors = frameCount;      // �o�b�N�o�b�t�@�����ƈ�v
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    if (FAILED(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvHeap)))) {
        m_backBuffers.clear();
        return false;
    }

    // �n���h���v�Z�p�̃C���N�������g
    m_rtvStride = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // �擪����A�Ԃ� RTV ������Ă���
    CD3DX12_CPU_DESCRIPTOR_HANDLE h(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < frameCount; ++i) {
        // SC �� i �Ԗڂ̃o�b�N�o�b�t�@�iID3D12Resource�j���擾
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])))) {
            // �r�����s���F���r���[�ȏ�Ԃ��c���Ȃ�
            m_backBuffers.clear();
            m_rtvHeap.Reset();
            m_rtvStride = 0;
            return false;
        }

        // �f�t�H���g�L�q�q�� RTV �쐬�i�o�b�N�o�b�t�@�� sRGB RTV ���T�|�[�g���Ȃ��_�ɒ��Ӂj
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, h);

        // ���̃X���b�g�֐i�߂�
        h.Offset(1, m_rtvStride);
    }
    return true;
}



// ============================================================================
// CreateDSV
//   �E�B���h�E�T�C�Y�� Depth �e�N�X�`�������A�P��� DSV ��؂�B
//   �\���p�̐[�x�Ƃ��ĉ^�p�BSceneRT ��p�̐[�x���~������ΕʂɊǗ�����B
// ============================================================================
bool DeviceResources::CreateDSV(UINT width, UINT height)
{
    // �q�[�v/���\�[�X����蒼�����߁A�܂��͎Q�Ƃ�؂�
    m_dsvHeap.Reset();
    m_depth.Reset();

    // DSV �q�[�v�iCPU ��p�E1�j
    D3D12_DESCRIPTOR_HEAP_DESC dsv{};
    dsv.NumDescriptors = 1;
    dsv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    if (FAILED(m_device->CreateDescriptorHeap(&dsv, IID_PPV_ARGS(&m_dsvHeap))))
        return false;

    // Depth �e�N�X�`���{�̂�����t�H�[�}�b�g/�T�C�Y�ō쐬
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        m_dsvFormat, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    // �N���A�l�iDSV �쐬���ɍœK�������j
    D3D12_CLEAR_VALUE clear{};
    clear.Format = m_dsvFormat;
    clear.DepthStencil.Depth = 1.0f;
    clear.DepthStencil.Stencil = 0;

    // �[�x���\�[�X�쐬�i������ԁFDEPTH_WRITE�j
    if (FAILED(m_device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear, IID_PPV_ARGS(&m_depth))))
        return false;

    // DSV �L�q�q�i�f�t�H���g�j
    D3D12_DEPTH_STENCIL_VIEW_DESC v{};
    v.Format = m_dsvFormat;
    v.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    // DSV �쐬�i�P��j
    m_device->CreateDepthStencilView(m_depth.Get(), &v, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
}



// ============================================================================
// ReleaseSizeDependentResources
//   ���T�C�Y�O/�f�o�C�X���X�g�O�ȂǂɕK���ĂԁB
//   �o�b�N�o�b�t�@��q�[�v�́g�Q�Ɓh��؂��Ă��� ResizeBuffers ���Ă΂Ȃ��Ǝ��s�������B
// ============================================================================
void DeviceResources::ReleaseSizeDependentResources()
{
    // BackBuffer �Q�Ƃ����ׂĉ��
    for (auto& bb : m_backBuffers) bb.Reset();
    m_backBuffers.clear();

    // �[�x/�q�[�v������i�n���h���������N���A�j
    m_depth.Reset();
    m_rtvHeap.Reset();
    m_dsvHeap.Reset();
    m_rtvStride = 0;
}
