// Time.cpp
//------------------------------------------------------------------------------
// �����F�����x�^�C�} (QueryPerformanceCounter; QPC) ���g���āA
//       1) ���߃t���[���̌o�ߎ��ԁiDeltaTime�j
//       2) �A�v���N������̗݌v���ԁiTotalTime�j
//       �� �g���t���[�� Update() ���ĂԂ����h �Ŏ擾�ł���悤�ɂ��郆�[�e�B���e�B�B
// �g�����F
//   - ���C�����[�v�̃t���[���擪�� Time::Update() ��1��Ă�
//   - ���̃t���[������ Time::GetDeltaTime() / Time::GetTime() �ŎQ�Ɖ\
// ���ӁF
//   - QPC �̓}�V���ˑ��̍����x�J�E���^�B�K�����g��(Hz)�Ŋ����ĕb�ɒ������ƁB
//   - ���� Update() �Ăяo������ Frequency�i���g���j�Ə����J�E���^���擾����B
//   - �}���`�X���b�h���瓯���ɌĂ΂Ȃ��O��i�K�v�Ȃ�r��/atomic �������j�B
//------------------------------------------------------------------------------

#include "Time.h"

// ============================================================================
// �ÓI�����o�̎���
// ============================================================================
double    Time::s_Frequency = 0.0;     // �^�C�}�̎��g���i1�b������̃J�E���g���j
double    Time::s_DeltaTime = 0.0;     // ���߃t���[���̌o�ߕb��
double    Time::s_TotalTime = 0.0;     // �݌v�o�ߕb���i�N������̍��v�j
long long Time::s_PrevCounter = 0;       // ���߃t���[���ŋL�^�����J�E���^�l
bool      Time::s_Initialized = false;   // ���񏉊������ς񂾂��ǂ���

// ============================================================================
// Update
//  - ���t���[�� 1 �񂾂��ĂԂ��Ɓi�t���[�����E�����j�B
//  - ���߃t���[���̌o�ߎ��ԁiDeltaTime�j�Ɨ݌v���ԁiTotalTime�j���X�V�B
//  - ����Ăяo�����FQPC ���g�����擾���A���݃J�E���^����Ƃ��ĕۑ��B
// ----------------------------------------------------------------------------
// �����̗���F
//   1) ���݂̃J�E���g�l�icurrentCounter�j�� QPC ����擾
//   2) ����Ȃ� Frequency ���擾���Ċ�l�Ƃ��� currentCounter ��ۑ� �� �߂炸���s
//   3) (currentCounter - s_PrevCounter) / s_Frequency �� ��t[sec] ���Z�o
//   4) �݌v���Ԃ� ��t �����Z
//   5) ���t���[���ɔ����� s_PrevCounter �� currentCounter �ɍX�V
// ============================================================================
void Time::Update()
{
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter); // 1) ���̃J�E���g�l���擾

    if (!s_Initialized)
    {
        // 2) ����F���g���i1�b������̃J�E���g���j���擾
        LARGE_INTEGER freq{};
        QueryPerformanceFrequency(&freq);
        s_Frequency = static_cast<double>(freq.QuadPart);
        s_PrevCounter = counter.QuadPart; // ���̒l����Ƃ��Ď��t���[���̍��������
        s_Initialized = true;
    }

    // 3) �o�ߎ��ԁi�b�j=�i���݃J�E���g - �O��J�E���g�j/ ���g��
    const long long current = counter.QuadPart;
    s_DeltaTime = static_cast<double>(current - s_PrevCounter) / s_Frequency;

    // 4) �݌v���Ԃ��X�V
    s_TotalTime += s_DeltaTime;

    // 5) ���t���[���̔�r�p�ɁA���݃J�E���^��ۑ�
    s_PrevCounter = current;

    // �i�⑫�j
    // �E�X���[�v��f�o�b�K��~����ȂǂŔ��ɑ傫�� ��t ���o�邱�Ƃ�����B
    //   ����������ꍇ�́As_DeltaTime �ɑ΂��ď���N�����v������Ȃ�
    //   �Ăяo�����|���V�[�ɍ��킹�Ē�������Ƃ悢�B
}

// ============================================================================
// GetDeltaTime
//  - ���߂� Update() �Ăяo������̌o�ߕb����Ԃ��i�t���[�����ԁj�B
//  - �����E�ړ��E�A�j���Ȃ� �g�t���[����ˑ��h �ɂ��������W�b�N�Ŏg�p�B
// ============================================================================
float Time::GetDeltaTime()
{
    return static_cast<float>(s_DeltaTime);
}

// ============================================================================
// GetTime
//  - �A�v���N������̗݌v�o�ߎ��ԁi�b�j��Ԃ��B
//  - �T�C���g�A�j����^�C�����C�����o�Ȃǂɕ֗��B
// ============================================================================
float Time::GetTime()
{
    return static_cast<float>(s_TotalTime);
}
