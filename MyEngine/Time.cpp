#include "Time.h"

// ============================================================================
// Time �N���X�i�ÓI���[�e�B���e�B�j
//  - �����x�^�C�}�[ (QueryPerformanceCounter) �𗘗p���āA
//    �t���[���o�ߎ��� (DeltaTime) ��݌v���� (TotalTime) ���Ǘ�����B
//  - �}���`�v���b�g�t�H�[���Ή����ӎ�����Ȃ� std::chrono ����₾���A
//    Windows �l�C�e�B�u�ł� QPC �����������B
// ============================================================================

// �ÓI�����o�̏�����
double Time::s_Frequency = 0.0;      // �^�C�}�[���g���i1�b������J�E���g���j
double Time::s_DeltaTime = 0.0;      // �O�t���[���Ƃ̌o�ߕb��
double Time::s_TotalTime = 0.0;      // �N������̗݌v�b��
long long Time::s_PrevCounter = 0;   // �O��擾�����J�E���^�l
bool Time::s_Initialized = false;    // �������ς݂��ǂ���

// ----------------------------------------------------------------------------
// Update
//  - ���t���[���Ăяo�����Ƃ� DeltaTime / TotalTime ���X�V����B
//  - ����Ăяo�����Ɏ��g�� (Frequency) ���擾���Ċ�l���Z�b�g����B
// ----------------------------------------------------------------------------
void Time::Update()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter); // ���݂̃J�E���g�l���擾

    if (!s_Initialized)
    {
        // ����̂ݎ��g�����擾�i1�b�Ԃ�����̃J�E���g���j
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        s_Frequency = static_cast<double>(freq.QuadPart);

        // ����̃J�E���g�l����l�Ƃ��ĕۑ�
        s_PrevCounter = counter.QuadPart;

        // �������ς݂ɐݒ�
        s_Initialized = true;
    }

    // --- �o�ߎ��Ԃ��v�Z ---
    // (���݃J�E���g - �O��J�E���g) / ���g�� = �b�P�ʂ̌o�ߎ���
    long long current = counter.QuadPart;
    s_DeltaTime = static_cast<double>(current - s_PrevCounter) / s_Frequency;

    // �݌v���Ԃɉ��Z
    s_TotalTime += s_DeltaTime;

    // ���t���[���ɔ����č���̃J�E���g��ۑ�
    s_PrevCounter = current;
}

// ----------------------------------------------------------------------------
// GetDeltaTime
//  - �O�� Update �Ăяo������̌o�ߎ��ԁi�b�j��Ԃ��B
//  - ��ɃQ�[�����̈ړ�/�A�j���[�V�������x�̃t���[���ˑ������������邽�߂Ɏg�p�B
// ----------------------------------------------------------------------------
float Time::GetDeltaTime()
{
    return static_cast<float>(s_DeltaTime);
}

// ----------------------------------------------------------------------------
// GetTime
//  - �A�v���P�[�V�����J�n����̗݌v�o�ߎ��ԁi�b�j��Ԃ��B
//  - ���o������I�ȓ����Ȃǂɗ��p�ł���B
// ----------------------------------------------------------------------------
float Time::GetTime()
{
    return static_cast<float>(s_TotalTime);
}
