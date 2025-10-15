#pragma once
#include <d3d12.h>

// d3dx12.h �̑��΃p�X�̓v���W�F�N�g�\���ɍ��킹��
#include "d3dx12.h"

// �o�b�N�o�b�t�@�ɕ`���Ƃ��ɕK�v�ȃ^�[�Q�b�g���
struct PresentTargets {
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{}; // BackBuffer �� RTV
    ID3D12Resource* backBuffer = nullptr;
    UINT                        width = 0;
    UINT                        height = 0;
    float                       clearColor[4] = { 0.2f, 0.2f, 0.4f, 1.0f };
};

class Presenter {
public:
    void Begin(ID3D12GraphicsCommandList* cmd, const PresentTargets& t);
    void End(ID3D12GraphicsCommandList* cmd, ID3D12Resource* backBuffer);
};
