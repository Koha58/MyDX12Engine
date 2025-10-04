#include "Time.h"

// ============================================================================
// Time クラス（静的ユーティリティ）
//  - 高精度タイマー (QueryPerformanceCounter) を利用して、
//    フレーム経過時間 (DeltaTime) や累計時間 (TotalTime) を管理する。
//  - マルチプラットフォーム対応を意識するなら std::chrono も候補だが、
//    Windows ネイティブでは QPC が推奨される。
// ============================================================================

// 静的メンバの初期化
double Time::s_Frequency = 0.0;      // タイマー周波数（1秒あたりカウント数）
double Time::s_DeltaTime = 0.0;      // 前フレームとの経過秒数
double Time::s_TotalTime = 0.0;      // 起動からの累計秒数
long long Time::s_PrevCounter = 0;   // 前回取得したカウンタ値
bool Time::s_Initialized = false;    // 初期化済みかどうか

// ----------------------------------------------------------------------------
// Update
//  - 毎フレーム呼び出すことで DeltaTime / TotalTime を更新する。
//  - 初回呼び出し時に周波数 (Frequency) を取得して基準値をセットする。
// ----------------------------------------------------------------------------
void Time::Update()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter); // 現在のカウント値を取得

    if (!s_Initialized)
    {
        // 初回のみ周波数を取得（1秒間あたりのカウント数）
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        s_Frequency = static_cast<double>(freq.QuadPart);

        // 今回のカウント値を基準値として保存
        s_PrevCounter = counter.QuadPart;

        // 初期化済みに設定
        s_Initialized = true;
    }

    // --- 経過時間を計算 ---
    // (現在カウント - 前回カウント) / 周波数 = 秒単位の経過時間
    long long current = counter.QuadPart;
    s_DeltaTime = static_cast<double>(current - s_PrevCounter) / s_Frequency;

    // 累計時間に加算
    s_TotalTime += s_DeltaTime;

    // 次フレームに備えて今回のカウントを保存
    s_PrevCounter = current;
}

// ----------------------------------------------------------------------------
// GetDeltaTime
//  - 前回 Update 呼び出しからの経過時間（秒）を返す。
//  - 主にゲーム内の移動/アニメーション速度のフレーム依存性を解消するために使用。
// ----------------------------------------------------------------------------
float Time::GetDeltaTime()
{
    return static_cast<float>(s_DeltaTime);
}

// ----------------------------------------------------------------------------
// GetTime
//  - アプリケーション開始からの累計経過時間（秒）を返す。
//  - 演出や周期的な動きなどに利用できる。
// ----------------------------------------------------------------------------
float Time::GetTime()
{
    return static_cast<float>(s_TotalTime);
}
