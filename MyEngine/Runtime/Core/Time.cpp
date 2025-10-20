// Time.cpp
//------------------------------------------------------------------------------
// 役割：高精度タイマ (QueryPerformanceCounter; QPC) を使って、
//       1) 直近フレームの経過時間（DeltaTime）
//       2) アプリ起動からの累計時間（TotalTime）
//       を “毎フレーム Update() を呼ぶだけ” で取得できるようにするユーティリティ。
// 使い方：
//   - メインループのフレーム先頭で Time::Update() を1回呼ぶ
//   - そのフレーム中は Time::GetDeltaTime() / Time::GetTime() で参照可能
// 注意：
//   - QPC はマシン依存の高精度カウンタ。必ず周波数(Hz)で割って秒に直すこと。
//   - 初回 Update() 呼び出し時に Frequency（周波数）と初期カウンタを取得する。
//   - マルチスレッドから同時に呼ばない前提（必要なら排他/atomic を検討）。
//------------------------------------------------------------------------------

#include "Time.h"

// ============================================================================
// 静的メンバの実体
// ============================================================================
double    Time::s_Frequency = 0.0;     // タイマの周波数（1秒あたりのカウント数）
double    Time::s_DeltaTime = 0.0;     // 直近フレームの経過秒数
double    Time::s_TotalTime = 0.0;     // 累計経過秒数（起動からの合計）
long long Time::s_PrevCounter = 0;       // 直近フレームで記録したカウンタ値
bool      Time::s_Initialized = false;   // 初回初期化が済んだかどうか

// ============================================================================
// Update
//  - 毎フレーム 1 回だけ呼ぶこと（フレーム境界処理）。
//  - 直近フレームの経過時間（DeltaTime）と累計時間（TotalTime）を更新。
//  - 初回呼び出し時：QPC 周波数を取得し、現在カウンタを基準として保存。
// ----------------------------------------------------------------------------
// 処理の流れ：
//   1) 現在のカウント値（currentCounter）を QPC から取得
//   2) 初回なら Frequency を取得して基準値として currentCounter を保存 → 戻らず続行
//   3) (currentCounter - s_PrevCounter) / s_Frequency で Δt[sec] を算出
//   4) 累計時間に Δt を加算
//   5) 次フレームに備えて s_PrevCounter を currentCounter に更新
// ============================================================================
void Time::Update()
{
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter); // 1) 今のカウント値を取得

    if (!s_Initialized)
    {
        // 2) 初回：周波数（1秒あたりのカウント数）を取得
        LARGE_INTEGER freq{};
        QueryPerformanceFrequency(&freq);
        s_Frequency = static_cast<double>(freq.QuadPart);
        s_PrevCounter = counter.QuadPart; // この値を基準として次フレームの差分を取る
        s_Initialized = true;
    }

    // 3) 経過時間（秒）=（現在カウント - 前回カウント）/ 周波数
    const long long current = counter.QuadPart;
    s_DeltaTime = static_cast<double>(current - s_PrevCounter) / s_Frequency;

    // 4) 累計時間を更新
    s_TotalTime += s_DeltaTime;

    // 5) 次フレームの比較用に、現在カウンタを保存
    s_PrevCounter = current;

    // （補足）
    // ・スリープやデバッガ停止直後などで非常に大きい Δt が出ることがある。
    //   それを嫌う場合は、s_DeltaTime に対して上限クランプを入れるなど
    //   呼び出し側ポリシーに合わせて調整するとよい。
}

// ============================================================================
// GetDeltaTime
//  - 直近の Update() 呼び出しからの経過秒数を返す（フレーム時間）。
//  - 物理・移動・アニメなど “フレーム非依存” にしたいロジックで使用。
// ============================================================================
float Time::GetDeltaTime()
{
    return static_cast<float>(s_DeltaTime);
}

// ============================================================================
// GetTime
//  - アプリ起動からの累計経過時間（秒）を返す。
//  - サイン波アニメやタイムライン演出などに便利。
// ============================================================================
float Time::GetTime()
{
    return static_cast<float>(s_TotalTime);
}
