#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <string>

namespace dxdbg
{
	// 失敗時のみOSのメッセージをOutoutDebugStringへ
	void LogHRESULTError(HRESULT hr, const char* msg);

	// InfoQueueによるデバッグ設定(_DEBUGのみ有効にしてOK)
	void SetupImfoQueue(Microsoft::WRL::ComPtr<ID3D12Device> device);

	// Live Objectsダンプ(終了時など)
	void ReportLiveObjects(Microsoft::WRL::ComPtr<ID3D12Device> device);
}
