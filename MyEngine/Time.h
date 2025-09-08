#pragma once
#include <windows.h>

class Time
{
public:
    // 毎フレーム最初に呼ぶ
    static void Update();

    // 前フレームからの経過時間（秒）
    static float GetDeltaTime();

    // アプリ起動からの経過時間（秒）
    static float GetTime();

private:
    static double s_Frequency;     // パフォーマンスカウンタの周波数
    static double s_DeltaTime;     // 前フレームからの経過時間
    static double s_TotalTime;     // アプリ起動からの経過時間
    static long long s_PrevCounter;// 前フレームのカウンタ値
    static bool s_Initialized;     // 初期化済みフラグ
};
