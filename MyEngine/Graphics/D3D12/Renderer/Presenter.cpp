#include "Renderer/Presenter.h"

/*
    Presenter
    ----------------------------------------------------------------------------
    �����F
      - �X���b�v�`�F�C���̃o�b�N�o�b�t�@���u�`��\�ɂ��� �� �N���A/�`�� ��
        Present �\�ɖ߂��v�܂ł̒�^�������J�v�Z�����B
    �g�����i1�t���[���j�F
      1) Begin(cmd, targets)   // RT ��Ԃ֑J�� + RTV�ݒ� + �N���A + VP/Scissor �ݒ�
      2) ... �o�b�N�o�b�t�@�֕`�� ...
      3) End(cmd, backBuffer)  // Present ��Ԃ֑J�ځi���̌� FrameScheduler ���� Present�j
    ���ӁF
      - �o���A�́u���݂̐�������ԁv����u���ɕK�v�ȏ�ԁv�ւ̑J�ڂł��邱�Ƃ��d�v�B
      - �����ł̓o�b�N�o�b�t�@�͑O�t���[���� Present �ς݂Ɖ��肵��
        PRESENT �� RENDER_TARGET �� PRESENT �̏��őJ�ڂ��Ă���B
      - �[�x�o�b�t�@���g���`����s���ꍇ�́ABegin �O�� or �����
        DSV �̐ݒ��N���A���Ăяo�����ōs�����ƁB
*/

void Presenter::Begin(ID3D12GraphicsCommandList* cmd, const PresentTargets& t)
{
    // ==============================
    // 1) ��ԑJ��: PRESENT �� RENDER_TARGET
    //    �O�t���[���� Present �ɖ߂��Ă���o�b�N�o�b�t�@��`��\�ɂ���B
    //    �� ����Ԃ��قȂ�� DRED/���؃��C�����x������̂ŁA�^�p�Ő�����ۂ��ƁB
    // ==============================
    auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(
        t.backBuffer,
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmd->ResourceBarrier(1, &toRT);

    // ==============================
    // 2) OM �� RTV ���Z�b�g���āA�o�b�N�o�b�t�@���N���A
    //    - DSV ���K�v�Ȃ�Ăяo������ OMSetRenderTargets ��
    //      RTV+DSV �ōĐݒ肵�Ă��� ClearDepthStencilView ���ĂԂ��ƁB
    // ==============================
    cmd->OMSetRenderTargets(1, &t.rtv, FALSE, nullptr);
    cmd->ClearRenderTargetView(t.rtv, t.clearColor, 0, nullptr);

    // ==============================
    // 3) RS: �r���[�|�[�g/�V�U�[���t���X�N���[���ɐݒ�
    //    ImGui �Ȃǂ�`���O�ɉ�ʃT�C�Y�֍��킹�Ă����B
    //    �� t.width/height �̓X���b�v�`�F�C���̌��݃T�C�Y�ƈ�v���Ă���K�v������B
    // ==============================
    D3D12_VIEWPORT vp{
        0.f, 0.f,
        static_cast<float>(t.width),
        static_cast<float>(t.height),
        0.f, 1.f
    };
    D3D12_RECT sc{
        0, 0,
        static_cast<LONG>(t.width),
        static_cast<LONG>(t.height)
    };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);
}

void Presenter::End(ID3D12GraphicsCommandList* cmd, ID3D12Resource* backBuffer)
{
    // ==============================
    // ��ԑJ��: RENDER_TARGET �� PRESENT
    // ���̌�A�L���[�ɃT�u�~�b�g���X���b�v�`�F�C�� Present�i�O���̃t���[�Ŏ��{�j�B
    // ==============================
    auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    cmd->ResourceBarrier(1, &toPresent);
}

/*
�y�悭���闎�Ƃ����z
- Clear/�`��O�� RT ��Ԃ֑J�ڂ��Ă��Ȃ��F
  �� PRESENT �̂܂� OMSet/Clear ����ƌ��؃G���[/����`����BBegin ��K���ʂ��B
- �r���[�|�[�g/�V�U�[���Â��T�C�Y�̂܂܁F
  �� ���T�C�Y��ɕ�/�������ς�邽�߁A���t���[���ݒ肪���S�i�{�����̒ʂ�j�B
- DSV ���g���̂ɐݒ肵�Ă��Ȃ��F
  �� �[�x�e�X�g���K�v�ȕ`�������ꍇ�ABegin ����� RTV+DSV �� OM �֐ݒ肵�����A
     ClearDepthStencilView ���s�����Ɓi�{�N���X�� RTV �݈̂����z��j�B
*/
