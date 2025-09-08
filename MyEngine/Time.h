#pragma once
#include <windows.h>

class Time
{
public:
    // ���t���[���ŏ��ɌĂ�
    static void Update();

    // �O�t���[������̌o�ߎ��ԁi�b�j
    static float GetDeltaTime();

    // �A�v���N������̌o�ߎ��ԁi�b�j
    static float GetTime();

private:
    static double s_Frequency;     // �p�t�H�[�}���X�J�E���^�̎��g��
    static double s_DeltaTime;     // �O�t���[������̌o�ߎ���
    static double s_TotalTime;     // �A�v���N������̌o�ߎ���
    static long long s_PrevCounter;// �O�t���[���̃J�E���^�l
    static bool s_Initialized;     // �������ς݃t���O
};
