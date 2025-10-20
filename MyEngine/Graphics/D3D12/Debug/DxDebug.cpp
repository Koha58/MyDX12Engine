#include "DxDebug.h"
#include <comdef.h>   // _com_error（HRESULT→人間可読メッセージ）
#include <sstream>    // std::wstringstream

using Microsoft::WRL::ComPtr;

namespace dxdbg
{

    // ----------------------------------------------------------------------------
    // LogHRESULTError
    // 目的: 失敗した HRESULT を、人間が読める形でデバッグ出力ウィンドウへ出す。
    // 用法: DX12/Win32 API 呼び出し直後に hr を渡す。FAILED(hr) の時だけ出力。
    // 出力: [D3D12Renderer ERROR] <msg> HRESULT=0xXXXXXXXX : <システムメッセージ>
    // 注意: 大量に呼ばれるとデバッグ出力が埋まるので、要所に限定すること。
    // ----------------------------------------------------------------------------
    void LogHRESULTError(HRESULT hr, const char* msg)
    {
        if (FAILED(hr))
        {
            // _com_error を使うと HRESULT に対応する Windows の説明文を取得できる
            _com_error err(hr);
            std::wstring errMsg = err.ErrorMessage();

            std::wstringstream wss;
            wss << L"[D3D12Renderer ERROR] " << msg
                << L" HRESULT=0x" << std::hex << hr
                << L" : " << errMsg << L"\n";

            // Visual Studio の "出力" ウィンドウへ
            OutputDebugStringW(wss.str().c_str());
        }
    }

    // ----------------------------------------------------------------------------
    // SetupInfoQueue（※綴り: Info の typo。既存 API 互換のためそのまま）
    // 目的: D3D12 の InfoQueue をセットアップして、重大度ごとにデバッガでブレークする。
    // 呼ぶ場所の目安: デバイス生成後、初期化の早い段階（Initialize 内など）。
    // 効果:
    //   ・CORRUPTION/ERROR/WARNING 発生時にブレーク（Debug 構成のみ）。
    //   ・特定の冗長メッセージをフィルタ（MAP/UNMAP の NULL range）。
    // 注意:
    //   ・Debug ビルド限定（_DEBUG が立っている時のみ有効）。
    //   ・大量出力が気になる場合は Severity を下げる/ID を追加でフィルタする。
    // ----------------------------------------------------------------------------
    void SetupInfoQueue(ComPtr<ID3D12Device> device)
    {
#ifdef _DEBUG
        if (!device) return;

        ComPtr<ID3D12InfoQueue> iq;
        if (SUCCEEDED(device.As(&iq)))
        {
            // 重大度別に「発生時にブレーク」させる
            iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE); // メモリ破損など致命的
            iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);      // エラー
            iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);    // 警告

            // よく出るがノイズになりやすいメッセージを抑制
            // 例: Map/Unmap での NULL 範囲は仕様上 OK なコードでも出がち
            D3D12_MESSAGE_ID deny[] = {
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
            };

            D3D12_INFO_QUEUE_FILTER f{};
            f.DenyList.NumIDs = _countof(deny);
            f.DenyList.pIDList = deny;

            // 以降、InfoQueue のストレージへ入るメッセージにフィルタが掛かる
            iq->PushStorageFilter(&f);
        }
#endif
    }

    // ----------------------------------------------------------------------------
    // ReportLiveObjects
    // 目的: デバイス破棄前などに「生き残っている」D3D12 オブジェクトをレポートする。
    // 呼ぶ場所の目安: シャットダウン直前（全 GPU 完了待ち・自前リソース解放後）。
    // 出力: D3D12_RLDO_DETAIL で詳細を出力（リーク追跡に便利）。
    // 注意:
    //   ・Debug ビルド限定。
    //   ・GPU 使用中のまま呼ぶと大量に出る/誤検知になるので、WaitForGPU 後に呼ぶ。
    // ----------------------------------------------------------------------------
    void ReportLiveObjects(ComPtr<ID3D12Device> device)
    {
#ifdef _DEBUG
        if (!device) return;

        ComPtr<ID3D12DebugDevice> dbg;
        if (SUCCEEDED(device.As(&dbg)))
        {
            // D3D12_RLDO_DETAIL: 詳細まで出す。他に SUMMARY/IGNORE_INTERNAL などもある。
            dbg->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
        }
#endif // _DEBUG
    }

} // namespace dxdbg
