#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <string>

/*
    DxDebug.h
    ----------------------------------------------------------------------------
    目的:
      - D3D12 を使った開発で、デバッグ時に役立つユーティリティ（ログ/InfoQueue/LiveObjects）
        の宣言をまとめたヘッダ。

    提供機能:
      1) LogHRESULTError(hr, msg)
         - HRESULT が失敗コードのときだけ、OS が持つエラーメッセージ文字列を取得し、
           "[D3D12Renderer ERROR] {msg} HRESULT=0x???????? : {OSの説明}" の形式で
           OutputDebugString に出力する。

      2) SetupInfoQueue(device)   ※_DEBUG ビルドでの利用を想定
         - ID3D12InfoQueue を通して、コアなエラー/警告でブレークする設定や、
           スパムになりがちなメッセージ ID の抑制フィルタを仕込む。

      3) ReportLiveObjects(device)   ※終了時のリーク検出に
         - ID3D12DebugDevice/ID3D12DebugDevice2 を取得できた場合に、
           ReportLiveDeviceObjects(D3D12_RLDO_DETAIL) を呼び出して
           ライブオブジェクト一覧をデバッガ出力へダンプする。

    実装ファイル:
      - DxDebug.cpp 側に実装を置く想定。

    注意:
      - SetupInfoQueue / ReportLiveObjects は _DEBUG のときだけ中身が動く実装にしておくと安全。
      - Release ビルドでのオーバーヘッドを嫌う場合は、実装側で #ifdef _DEBUG ガード推奨。
*/

namespace dxdbg
{
    /**
     * @brief 失敗時のみ OS のエラーメッセージを OutputDebugString へ出力する。
     * @param hr   対象の HRESULT
     * @param msg  呼び出し元で識別しやすい任意のタグ/説明（例: "CreateCommittedResource"）
     *
     * 例:
     *   dxdbg::LogHRESULTError(hr, "D3DCompile");
     */
    void LogHRESULTError(HRESULT hr, const char* msg);

    /**
     * @brief D3D12 InfoQueue のブレーク/フィルタ設定を行う（_DEBUG 向け）。
     * @param device  有効な ID3D12Device（ComPtr）
     *
     * 設定例（実装側）:
     *   - SEVERITY_CORRUPTION/ERROR/WARNING でブレーク
     *   - よく出る MAP/UNMAP の NULLRANGE 警告を抑制 など
     */
    void SetupInfoQueue(Microsoft::WRL::ComPtr<ID3D12Device> device);

    /**
     * @brief ライブオブジェクトのダンプ（終了時などのリーク確認用）。
     * @param device  有効な ID3D12Device（ComPtr）
     *
     * 実装側で ID3D12DebugDevice(2) に Query して ReportLiveDeviceObjects を呼ぶ。
     */
    void ReportLiveObjects(Microsoft::WRL::ComPtr<ID3D12Device> device);

    // ----------------------------------------------------------------------------
    // 互換ラッパ（旧名サポート）:
    // 既存コードで誤綴りの SetupImfoQueue(...) を呼んでいる場合のためのエイリアス。
    // 新規コードでは SetupInfoQueue(...) を使用してください。
    // ----------------------------------------------------------------------------
    inline void SetupImfoQueue(Microsoft::WRL::ComPtr<ID3D12Device> device) { SetupInfoQueue(device); }
}
