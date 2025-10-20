#pragma once
// ============================================================================
// FrameScheduler.h
// �ړI�F
//   - 1�t���[���̃��C�t�T�C�N���iBegin �� �R�}���h�L�^ �� End/Present�j���W�񂷂鏬���Ȏi�ߓ��B
//   - �����t���[�������i�s�iFrameCount �� FrameResources �������O�ŉ񂷁j����
//     �t�F���X�҂��E�R�}���h�A���P�[�^�� Reset�EPresent/Signal�E�x���j���̎��s���ꊇ�Ǘ��B
// �݌v�|�C���g�F
//   - BeginFrame() �Łu���̃t���[���Ŏg�� FrameResources/�R�}���h���X�g�v��Ԃ��B
//   - EndFrame() �� Present + �t�F���X Signal�ARenderTarget �̒x���j���o�^�i�C�Ӂj�B
//   - �uBegin ���Ɏ擾�����o�b�N�o�b�t�@�C���f�b�N�X�v��ێ����� End �ł�������g���B
//     �iPresent ��Ɏ�蒼���ƃC���f�b�N�X���i��ł��܂��A�قȂ� FrameResources ��
//      �Q�Ƃ��Ă��܂����̂�h�~�j
// �g�����F
//   auto b = scheduler.BeginFrame();
//   ID3D12GraphicsCommandList* cmd = b.cmd;
//   // �ccmd �֋L�^�c
//   scheduler.EndFrame(&rtToDispose); // �j����������� nullptr ���f�t�H���g������OK
// ============================================================================

#include <cstdint>

// --- fwd declares�i�d�ˑ��������j ---
struct ID3D12Fence;
struct ID3D12GraphicsCommandList;
class  DeviceResources;
class  FrameResources;
class  GpuGarbageQueue;
struct RenderTargetHandles; // �O���錾�̂܂܂�OK�i���̂� Core/RenderTarget.h ���j

class FrameScheduler {
public:
    // BeginFrame() ���Ԃ��ŏ��Z�b�g
    struct BeginInfo {
        unsigned frameIndex;                 // ���̃t���[���Ŏg�p����t���[���C���f�b�N�X
        ID3D12GraphicsCommandList* cmd;      // �L�^�Ɏg���R�}���h���X�g
    };

    // ----------------------------------------------------------------------------
    // Initialize
    // �����F
    //   - �K�v�ȋ��L�I�u�W�F�N�g�i�f�o�C�X/�t�F���X/�C�x���g/�t���[���Q/�x���j���L���[�j������
    //   - �t�F���X�l�̏������i���� Signal �ς݂̉\�����l�����Acompleted+1 ����J�n�j
    // �O��F
    //   - dev/fence/frames �͗L��
    // ----------------------------------------------------------------------------
    void Initialize(DeviceResources* dev,
        ID3D12Fence* fence,
        void* fenceEvent,
        FrameResources* frames,
        GpuGarbageQueue* garbage);

    // ----------------------------------------------------------------------------
    // BeginFrame
    // �����F
    //   - �J�����g�̃o�b�N�o�b�t�@�C���f�b�N�X���擾���A�Ή����� FrameResources ��I��
    //   - �K�v�Ȃ炻�� Frame �̃t�F���X������҂��ăR�}���h�A���P�[�^�� Reset
    //   - �R�}���h���X�g���i����쐬 or�jReset ���ĕԂ�
    // �߂�l�F
    //   - BeginInfo�iframeIndex �� cmd�j
    // ----------------------------------------------------------------------------
    BeginInfo BeginFrame();

    // ----------------------------------------------------------------------------
    // EndFrame
    // �����F
    //   - �R�}���h���X�g�� Close �� Execute �� Present
    //   - �t�F���X�� Signal ���A���Y�t���[���� fenceValue ���L�^
    //   - �n���ꂽ RenderTargetHandles ������΁A���� fence �ɂԂ牺���Ēx���j���o�^
    // �����F
    //   - toDispose: ���t���[���̏I�����ɔj�������� RT �Z�b�g�inullptr �Ȃ�j���Ȃ��j
    // ���l�F
    //   - GpuGarbageQueue::Collect ���Ă�Ŋ����ςݕ���s�x���
    // ----------------------------------------------------------------------------
    void EndFrame(RenderTargetHandles* toDispose = nullptr);

    // ���݂̃t���[���Ŏg���Ă���R�}���h���X�g���O�ɓn����������
    ID3D12GraphicsCommandList* GetCmd() const { return m_cmd; }

    // �I�����̌�n���i�R�}���h���X�g�� Release �Ȃǁj
    ~FrameScheduler();

private:
    // �O�����狟������鋤�L�I�u�W�F�N�g�Q�i�ؗp�j
    DeviceResources* m_dev = nullptr; // �f�o�C�X/�X���b�v�`�F�C��/�L���[
    FrameResources* m_frames = nullptr; // �t���[�����Ƃ̃A���P�[�^/CB
    GpuGarbageQueue* m_garbage = nullptr; // �x���j���L���[
    ID3D12GraphicsCommandList* m_cmd = nullptr; // �g���� 1 �{�̃R�}���h���X�g
    ID3D12Fence* m_fence = nullptr; // GPU �����t�F���X�i�O�����L�j
    void* m_fenceEvent = nullptr; // �t�F���X�ҋ@�C�x���g�iHANDLE �� void* �ŕێ��j

    std::uint64_t               m_nextFence = 0;       // ���� Signal ����t�F���X�l

    // �� BeginFrame �Ŏ擾�����o�b�N�o�b�t�@�C���f�b�N�X��ۑ����A
    //   EndFrame �� Present ��Ɂu�Ď擾���Ȃ��v���߂̃L���b�V��
    unsigned                    m_inFlightFrameIndex = 0;
};
