// DebugHr.h
#pragma once
#include "DxDebug.h"
#include <windows.h>

/*
    DebugHr.h
    ----------------------------------------------------------------------------
    �ړI:
      - DirectX / Win32 API �Ăяo���� HRESULT ���Ȍ��Ƀ`�F�b�N���邽�߂̃}�N����񋟁B
      - ���s(SUCCEEDED(hr)==false)���́A�G���[���e���f�o�b�K�o�͂��A���̏�Ńu���[�N�B

    �g����:
      HRB(device->CreateCommittedResource(...));
      HRB(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err));

    ����:
      1) �����Ƃ��ēn����������x�����]�����AHRESULT ���󂯎��B
      2) FAILED(hr) �̏ꍇ:
         - dxdbg::LogHRESULTError(hr, "��������") ���ĂсAOS �ɕR�Â��G���[��������o�́B
           ��: [D3D12Renderer ERROR] D3DCompile(...) HRESULT=0x887A0005 : The GPU device instance has been suspended...
         - __debugbreak() ���Ăяo���ăf�o�b�K�Œ�~�i�f�o�b�K��ڑ����͗�O�_�C�A���O���j�B

    �݌v����:
      - do{ ... }while(0) �̌`�ɂ��邱�ƂŁAif/else �Ƒg�ݍ��킹�Ă��\���I�Ɉ��S�ȑ����}�N���ɂȂ�B
      - #_hrcall_ �ɂ�� �g�Ăяo���������̂��́h �𕶎��񉻂��ă��O�Ɏc���B
      - __debugbreak() �͒ʏ� Debug �r���h�ŕ֗��BRelease �ł����s�����_�ɒ��ӁB
        �K�v�ɉ����� NDEBUG �K�[�h�Ŗ���������g�����B
      - ��O�𓊂������ꍇ�́A__debugbreak() �̑���� throw ���g���݌v�ɒu��������B

    �ˑ�:
      - DxDebug.h : dxdbg::LogHRESULTError(HRESULT, const char*) ��z��B
      - windows.h : __debugbreak, HRESULT/FAILED �ȂǁB
*/

// ���s������ OS ���b�Z�[�W���o���ăf�o�b�K�Ŏ~�߂�
#ifndef HRB
#define HRB(_hrcall_) do{                            \
    const HRESULT _hr__ = (_hrcall_);                /* �@ ��x�����]�� */ \
    if (FAILED(_hr__)) {                             /* �A ���s�Ȃ� */ \
        dxdbg::LogHRESULTError(_hr__, #_hrcall_);    /*    �Ăяo�������t���Ń��O */ \
        __debugbreak();                               /*    �B ���̏�Ńu���[�N */ \
    }                                                \
}while(0)
#endif
