#pragma once
#include <DirectXMath.h>

// Lambert�p �萔�o�b�t�@
struct SceneConstantBuffer
{
    DirectX::XMFLOAT4X4 mvp;   // ���f���r���[�ˉe�s��
    DirectX::XMFLOAT3 lightDir; // ��������
    float pad;                  // 16�o�C�g���E���킹�p�i�K�{�j
};



