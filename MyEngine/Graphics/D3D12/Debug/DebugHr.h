// DebugHr.h
#pragma once
#include "DxDebug.h"
#include <windows.h>

/*
    DebugHr.h
    ----------------------------------------------------------------------------
    目的:
      - DirectX / Win32 API 呼び出しの HRESULT を簡潔にチェックするためのマクロを提供。
      - 失敗(SUCCEEDED(hr)==false)時は、エラー内容をデバッガ出力し、その場でブレーク。

    使い方:
      HRB(device->CreateCommittedResource(...));
      HRB(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err));

    動作:
      1) 引数として渡した式を一度だけ評価し、HRESULT を受け取る。
      2) FAILED(hr) の場合:
         - dxdbg::LogHRESULTError(hr, "式文字列") を呼び、OS に紐づくエラー文字列を出力。
           例: [D3D12Renderer ERROR] D3DCompile(...) HRESULT=0x887A0005 : The GPU device instance has been suspended...
         - __debugbreak() を呼び出してデバッガで停止（デバッガ非接続時は例外ダイアログ等）。

    設計メモ:
      - do{ ... }while(0) の形にすることで、if/else と組み合わせても構文的に安全な多文マクロになる。
      - #_hrcall_ により “呼び出した式そのもの” を文字列化してログに残す。
      - __debugbreak() は通常 Debug ビルドで便利。Release でも実行される点に注意。
        必要に応じて NDEBUG ガードで無効化する拡張も可。
      - 例外を投げたい場合は、__debugbreak() の代わりに throw を使う設計に置き換える。

    依存:
      - DxDebug.h : dxdbg::LogHRESULTError(HRESULT, const char*) を想定。
      - windows.h : __debugbreak, HRESULT/FAILED など。
*/

// 失敗したら OS メッセージを出してデバッガで止める
#ifndef HRB
#define HRB(_hrcall_) do{                            \
    const HRESULT _hr__ = (_hrcall_);                /* ① 一度だけ評価 */ \
    if (FAILED(_hr__)) {                             /* ② 失敗なら */ \
        dxdbg::LogHRESULTError(_hr__, #_hrcall_);    /*    呼び出し式名付きでログ */ \
        __debugbreak();                               /*    ③ その場でブレーク */ \
    }                                                \
}while(0)
#endif
