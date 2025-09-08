#include "Time.h"

double Time::s_Frequency = 0.0;
double Time::s_DeltaTime = 0.0;
double Time::s_TotalTime = 0.0;
long long Time::s_PrevCounter = 0;
bool Time::s_Initialized = false;

void Time::Update()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);

    if (!s_Initialized)
    {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        s_Frequency = static_cast<double>(freq.QuadPart);
        s_PrevCounter = counter.QuadPart;
        s_Initialized = true;
    }

    // Œo‰ßŽžŠÔ‚ðŒvŽZ
    long long current = counter.QuadPart;
    s_DeltaTime = static_cast<double>(current - s_PrevCounter) / s_Frequency;
    s_TotalTime += s_DeltaTime;
    s_PrevCounter = current;
}

float Time::GetDeltaTime()
{
    return static_cast<float>(s_DeltaTime);
}

float Time::GetTime()
{
    return static_cast<float>(s_TotalTime);
}
