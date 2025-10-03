#include "DxDebug.h"
#include <comdef.h>
#include<sstream>

using Microsoft::WRL::ComPtr;

namespace dxdbg
{

	void LogHRESULTError(HRESULT hr, const char* msg)
	{
		if (FAILED(hr))
		{
			_com_error err(hr);
			std::wstring errMsg = err.ErrorMessage();
			std::wstringstream wss;
			wss << L"[D3D12Renderer ERROR] " << msg
				<< L" HRESULT=0x" << std::hex << hr
				<< L" : " << errMsg << L"\n";
			OutputDebugStringW(wss.str().c_str());
		}
	}

	void SetupImfoQueue(ComPtr<ID3D12Device> device)
	{
	#ifdef _DEBUG

		if (!device) return;

		ComPtr<ID3D12InfoQueue> iq;

		if (SUCCEEDED(device.As(&iq)))
		{
			iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

			D3D12_MESSAGE_ID deny[] = {
				D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
				D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
			};

			D3D12_INFO_QUEUE_FILTER f{};
			f.DenyList.NumIDs = _countof(deny);
			f.DenyList.pIDList = deny;
			iq->PushStorageFilter(&f);
		}

	#endif

	}

	void ReportLiveObjects(ComPtr<ID3D12Device> device)
	{
	#ifdef _DEBUG

		if (!device) return;

		ComPtr<ID3D12DebugDevice> dbg;

		if (SUCCEEDED(device.As(&dbg)))
		{
			dbg->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
		}

	#endif // DEBUG

	}
}

