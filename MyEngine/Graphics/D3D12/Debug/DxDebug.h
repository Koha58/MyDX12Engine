#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <string>

/*
    DxDebug.h
    ----------------------------------------------------------------------------
    �ړI:
      - D3D12 ���g�����J���ŁA�f�o�b�O���ɖ𗧂��[�e�B���e�B�i���O/InfoQueue/LiveObjects�j
        �̐錾���܂Ƃ߂��w�b�_�B

    �񋟋@�\:
      1) LogHRESULTError(hr, msg)
         - HRESULT �����s�R�[�h�̂Ƃ������AOS �����G���[���b�Z�[�W��������擾���A
           "[D3D12Renderer ERROR] {msg} HRESULT=0x???????? : {OS�̐���}" �̌`����
           OutputDebugString �ɏo�͂���B

      2) SetupInfoQueue(device)   ��_DEBUG �r���h�ł̗��p��z��
         - ID3D12InfoQueue ��ʂ��āA�R�A�ȃG���[/�x���Ńu���[�N����ݒ��A
           �X�p���ɂȂ肪���ȃ��b�Z�[�W ID �̗}���t�B���^���d���ށB

      3) ReportLiveObjects(device)   ���I�����̃��[�N���o��
         - ID3D12DebugDevice/ID3D12DebugDevice2 ���擾�ł����ꍇ�ɁA
           ReportLiveDeviceObjects(D3D12_RLDO_DETAIL) ���Ăяo����
           ���C�u�I�u�W�F�N�g�ꗗ���f�o�b�K�o�͂փ_���v����B

    �����t�@�C��:
      - DxDebug.cpp ���Ɏ�����u���z��B

    ����:
      - SetupInfoQueue / ReportLiveObjects �� _DEBUG �̂Ƃ��������g�����������ɂ��Ă����ƈ��S�B
      - Release �r���h�ł̃I�[�o�[�w�b�h�������ꍇ�́A�������� #ifdef _DEBUG �K�[�h�����B
*/

namespace dxdbg
{
    /**
     * @brief ���s���̂� OS �̃G���[���b�Z�[�W�� OutputDebugString �֏o�͂���B
     * @param hr   �Ώۂ� HRESULT
     * @param msg  �Ăяo�����Ŏ��ʂ��₷���C�ӂ̃^�O/�����i��: "CreateCommittedResource"�j
     *
     * ��:
     *   dxdbg::LogHRESULTError(hr, "D3DCompile");
     */
    void LogHRESULTError(HRESULT hr, const char* msg);

    /**
     * @brief D3D12 InfoQueue �̃u���[�N/�t�B���^�ݒ���s���i_DEBUG �����j�B
     * @param device  �L���� ID3D12Device�iComPtr�j
     *
     * �ݒ��i�������j:
     *   - SEVERITY_CORRUPTION/ERROR/WARNING �Ńu���[�N
     *   - �悭�o�� MAP/UNMAP �� NULLRANGE �x����}�� �Ȃ�
     */
    void SetupInfoQueue(Microsoft::WRL::ComPtr<ID3D12Device> device);

    /**
     * @brief ���C�u�I�u�W�F�N�g�̃_���v�i�I�����Ȃǂ̃��[�N�m�F�p�j�B
     * @param device  �L���� ID3D12Device�iComPtr�j
     *
     * �������� ID3D12DebugDevice(2) �� Query ���� ReportLiveDeviceObjects ���ĂԁB
     */
    void ReportLiveObjects(Microsoft::WRL::ComPtr<ID3D12Device> device);

    // ----------------------------------------------------------------------------
    // �݊����b�p�i�����T�|�[�g�j:
    // �����R�[�h�Ō�Ԃ�� SetupImfoQueue(...) ���Ă�ł���ꍇ�̂��߂̃G�C���A�X�B
    // �V�K�R�[�h�ł� SetupInfoQueue(...) ���g�p���Ă��������B
    // ----------------------------------------------------------------------------
    inline void SetupImfoQueue(Microsoft::WRL::ComPtr<ID3D12Device> device) { SetupInfoQueue(device); }
}
