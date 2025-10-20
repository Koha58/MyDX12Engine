#include "DxDebug.h"
#include <comdef.h>   // _com_error�iHRESULT���l�ԉǃ��b�Z�[�W�j
#include <sstream>    // std::wstringstream

using Microsoft::WRL::ComPtr;

namespace dxdbg
{

    // ----------------------------------------------------------------------------
    // LogHRESULTError
    // �ړI: ���s���� HRESULT ���A�l�Ԃ��ǂ߂�`�Ńf�o�b�O�o�̓E�B���h�E�֏o���B
    // �p�@: DX12/Win32 API �Ăяo������� hr ��n���BFAILED(hr) �̎������o�́B
    // �o��: [D3D12Renderer ERROR] <msg> HRESULT=0xXXXXXXXX : <�V�X�e�����b�Z�[�W>
    // ����: ��ʂɌĂ΂��ƃf�o�b�O�o�͂����܂�̂ŁA�v���Ɍ��肷�邱�ƁB
    // ----------------------------------------------------------------------------
    void LogHRESULTError(HRESULT hr, const char* msg)
    {
        if (FAILED(hr))
        {
            // _com_error ���g���� HRESULT �ɑΉ����� Windows �̐��������擾�ł���
            _com_error err(hr);
            std::wstring errMsg = err.ErrorMessage();

            std::wstringstream wss;
            wss << L"[D3D12Renderer ERROR] " << msg
                << L" HRESULT=0x" << std::hex << hr
                << L" : " << errMsg << L"\n";

            // Visual Studio �� "�o��" �E�B���h�E��
            OutputDebugStringW(wss.str().c_str());
        }
    }

    // ----------------------------------------------------------------------------
    // SetupInfoQueue�i���Ԃ�: Info �� typo�B���� API �݊��̂��߂��̂܂܁j
    // �ړI: D3D12 �� InfoQueue ���Z�b�g�A�b�v���āA�d��x���ƂɃf�o�b�K�Ńu���[�N����B
    // �Ăԏꏊ�̖ڈ�: �f�o�C�X������A�������̑����i�K�iInitialize ���Ȃǁj�B
    // ����:
    //   �ECORRUPTION/ERROR/WARNING �������Ƀu���[�N�iDebug �\���̂݁j�B
    //   �E����̏璷���b�Z�[�W���t�B���^�iMAP/UNMAP �� NULL range�j�B
    // ����:
    //   �EDebug �r���h����i_DEBUG �������Ă��鎞�̂ݗL���j�B
    //   �E��ʏo�͂��C�ɂȂ�ꍇ�� Severity ��������/ID ��ǉ��Ńt�B���^����B
    // ----------------------------------------------------------------------------
    void SetupInfoQueue(ComPtr<ID3D12Device> device)
    {
#ifdef _DEBUG
        if (!device) return;

        ComPtr<ID3D12InfoQueue> iq;
        if (SUCCEEDED(device.As(&iq)))
        {
            // �d��x�ʂɁu�������Ƀu���[�N�v������
            iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE); // �������j���Ȃǒv���I
            iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);      // �G���[
            iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);    // �x��

            // �悭�o�邪�m�C�Y�ɂȂ�₷�����b�Z�[�W��}��
            // ��: Map/Unmap �ł� NULL �͈͎͂d�l�� OK �ȃR�[�h�ł��o����
            D3D12_MESSAGE_ID deny[] = {
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
            };

            D3D12_INFO_QUEUE_FILTER f{};
            f.DenyList.NumIDs = _countof(deny);
            f.DenyList.pIDList = deny;

            // �ȍ~�AInfoQueue �̃X�g���[�W�֓��郁�b�Z�[�W�Ƀt�B���^���|����
            iq->PushStorageFilter(&f);
        }
#endif
    }

    // ----------------------------------------------------------------------------
    // ReportLiveObjects
    // �ړI: �f�o�C�X�j���O�ȂǂɁu�����c���Ă���vD3D12 �I�u�W�F�N�g�����|�[�g����B
    // �Ăԏꏊ�̖ڈ�: �V���b�g�_�E�����O�i�S GPU �����҂��E���O���\�[�X�����j�B
    // �o��: D3D12_RLDO_DETAIL �ŏڍׂ��o�́i���[�N�ǐՂɕ֗��j�B
    // ����:
    //   �EDebug �r���h����B
    //   �EGPU �g�p���̂܂܌ĂԂƑ�ʂɏo��/�댟�m�ɂȂ�̂ŁAWaitForGPU ��ɌĂԁB
    // ----------------------------------------------------------------------------
    void ReportLiveObjects(ComPtr<ID3D12Device> device)
    {
#ifdef _DEBUG
        if (!device) return;

        ComPtr<ID3D12DebugDevice> dbg;
        if (SUCCEEDED(device.As(&dbg)))
        {
            // D3D12_RLDO_DETAIL: �ڍׂ܂ŏo���B���� SUMMARY/IGNORE_INTERNAL �Ȃǂ�����B
            dbg->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
        }
#endif // _DEBUG
    }

} // namespace dxdbg
