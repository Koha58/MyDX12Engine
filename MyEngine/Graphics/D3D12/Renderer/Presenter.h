#pragma once
#include <d3d12.h>

// d3dx12.h �̑��΃p�X�̓v���W�F�N�g�\���ɍ��킹��
#include "d3dx12.h"

/*
    Presenter.h
    ----------------------------------------------------------------------------
    �����F
      - �X���b�v�`�F�C���̃o�b�N�o�b�t�@���u�`��p�ɊJ�� �� �N���A/�`�� ��
        Present �p�ɖ߂��v���߂̍ŏ����̃��[�e�B���e�B�B
      - Begin() �� PRESENT��RENDER_TARGET �֑J�ڂ��ARTV/VP/Scissor ���Z�b�g�B
      - End()   �� RENDER_TARGET��PRESENT �֖߂��B

    �z��t���[�F
      PresentTargets pt{ rtvHandle, backBuffer, w, h, {r,g,b,a} };
      presenter.Begin(cmd, pt);
      // ������ UI �Ȃǃo�b�N�o�b�t�@�ւ̕`����s��
      presenter.End(cmd, pt.backBuffer);

    ���ӁF
      - �{���[�e�B���e�B�� *�o�b�N�o�b�t�@* ��p�B�I�t�X�N���[��RT�� RenderTarget �N���X���ŁB
      - Begin/End �̊Ԃŕʂ̃��\�[�X��ԂɑJ�ڂ��Ȃ����Ɓi��ԕs�����̌����ɂȂ�j�B
*/

// �o�b�N�o�b�t�@�ɕ`���Ƃ��ɕK�v�ȃ^�[�Q�b�g���
struct PresentTargets {
    // �X���b�v�`�F�C���̃o�b�N�o�b�t�@�ɑΉ����� RTV �n���h���iCPU ���j
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};

    // �o�b�N�o�b�t�@�̃��\�[�X�{�́iResourceBarrier �ŏ�ԑJ�ڂɎg�p�j
    ID3D12Resource* backBuffer = nullptr;

    // ���݂̃o�b�N�o�b�t�@�̃s�N�Z���T�C�Y�iViewport/Scissor �ݒ�Ɏg�p�j
    UINT width = 0;
    UINT height = 0;

    // �N���A�J���[�iBegin �� ClearRenderTargetView �ɓK�p�j
    float clearColor[4] = { 0.2f, 0.2f, 0.4f, 1.0f };
};

class Presenter {
public:
    /*
        Begin
        ------------------------------------------------------------------------
        ��邱�ƁF
          1) PRESENT �� RENDER_TARGET �֏�ԑJ�ځiTransition Barrier�j
          2) OMSetRenderTargets �� RTV ���Z�b�g
          3) ClearRenderTargetView �Ńo�b�N�o�b�t�@��������
          4) Viewport / Scissor ���t���T�C�Y�ɐݒ�

        �O��F
          - cmd �� DIRECT �^�C�v�̃O���t�B�b�N�X�R�}���h���X�g
          - t.backBuffer �� t.rtv ���L��
    */
    void Begin(ID3D12GraphicsCommandList* cmd, const PresentTargets& t);

    /*
        End
        ------------------------------------------------------------------------
        ��邱�ƁF
          1) RENDER_TARGET �� PRESENT �֏�ԑJ�ځiTransition Barrier�j
             �i���̌�L���[�փT�u�~�b�g���APresent ���Ăԑz��j

        �O��F
          - backBuffer �� Begin �Ŏg�������̂Ɠ���
    */
    void End(ID3D12GraphicsCommandList* cmd, ID3D12Resource* backBuffer);
};
