#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <string>

namespace dxdbg
{
	// ���s���̂�OS�̃��b�Z�[�W��OutoutDebugString��
	void LogHRESULTError(HRESULT hr, const char* msg);

	// InfoQueue�ɂ��f�o�b�O�ݒ�(_DEBUG�̂ݗL���ɂ���OK)
	void SetupImfoQueue(Microsoft::WRL::ComPtr<ID3D12Device> device);

	// Live Objects�_���v(�I�����Ȃ�)
	void ReportLiveObjects(Microsoft::WRL::ComPtr<ID3D12Device> device);
}
