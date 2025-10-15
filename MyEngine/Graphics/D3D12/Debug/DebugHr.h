// DebugHr.h
#pragma once
#include "DxDebug.h"
#include <windows.h>

// 失敗したら OS メッセージを出してデバッガで止める
#ifndef HRB
#define HRB(_hrcall_) do{ \
    const HRESULT _hr__ = (_hrcall_); \
    if (FAILED(_hr__)) { \
        dxdbg::LogHRESULTError(_hr__, #_hrcall_); \
        __debugbreak(); \
    } \
}while(0)
#endif
