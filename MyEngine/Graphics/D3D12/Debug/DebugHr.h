// DebugHr.h
#pragma once
#include "DxDebug.h"
#include <windows.h>

// ���s������ OS ���b�Z�[�W���o���ăf�o�b�K�Ŏ~�߂�
#ifndef HRB
#define HRB(_hrcall_) do{ \
    const HRESULT _hr__ = (_hrcall_); \
    if (FAILED(_hr__)) { \
        dxdbg::LogHRESULTError(_hr__, #_hrcall_); \
        __debugbreak(); \
    } \
}while(0)
#endif
