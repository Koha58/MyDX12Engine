// Renderer/FrameScheduler.cpp
// PCHを使っているなら
// #include "pch.h"

#include "Renderer/FrameScheduler.h"

#include <windows.h>
#include <d3d12.h>
#include <algorithm>
#include <utility>
#include <cassert>

#include "Core/DeviceResources.h"
#include "Core/FrameResources.h"
#include "Core/GpuGarbage.h"
#include "Core/RenderTarget.h"

/*
    FrameScheduler
    ----------------------------------------------------------------------------
    目的：
      - 1フレームの「開始～終了」までの共通処理をカプセル化。
        * 前フレーム完了待ち（Fence）
        * CommandAllocator/CommandList の Reset
        * Submit/Present/Signal
        * フレームごとの Fence 値の紐付け（後続の待機に使う）
        * 遅延破棄キュー（GpuGarbageQueue）への投入と回収

    使い方：
      - Initialize() で必要なリソースをセット。
      - 毎フレーム Render() 相当の流れで：
         1) auto begin = BeginFrame();  // cmd を取得してレコード開始
         2) ... コマンドを記録 ...
         3) EndFrame(&deadRT);          // 提出＆Present＆遅延破棄登録

    注意（落とし穴）：
      - BackBufferIndex を Begin で捕捉し、End で「取り直さない」こと。
        Present 後にインデックスが切り替わるため、Begin/End でズレると
        別フレームの FrameResources を触ってしまう。
      - Fence 値は「外部で Signal されることがある」前提で単調増加を担保。
      - GpuGarbageQueue への投入は Submit と同じフレームの Fence 値で行い、
        Collect() は毎フレーム呼んでリークを防ぐ。
*/

void FrameScheduler::Initialize(DeviceResources* dev,
    ID3D12Fence* fence,
    void* fenceEvent,
    FrameResources* frames,
    GpuGarbageQueue* garbage)
{
    // 呼び出し側の所有物（寿命管理は呼び出し側）を「借りる」
    m_dev = dev;
    m_fence = fence;
    m_fenceEvent = fenceEvent;
    m_frames = frames;
    m_garbage = garbage;

    // 既に他所でSignalされてる可能性を考慮して completed+1 から開始
    const std::uint64_t completed = (m_fence ? m_fence->GetCompletedValue() : 0);
    m_nextFence = completed + 1;

    // インフライトのフレームインデックス初期化（BeginFrameで更新）
    m_inFlightFrameIndex = 0;
}

FrameScheduler::BeginInfo FrameScheduler::BeginFrame()
{
    // ==============================
    // 1) 今フレームの BackBufferIndex を捕捉
    //    ※ EndFrame で再取得しない（Present により切り替わるため）
    // ==============================
    const unsigned fi = m_dev->GetCurrentBackBufferIndex();
    m_inFlightFrameIndex = fi;

    // フレーム専用のリソースセット（Allocator/UPL領域など）
    auto& fr = m_frames->Get(fi);

    // ==============================
    // 2) 前回このフレームインデックスに投げた仕事の完了待ち
    //    （Fence が未完了ならイベント待ち）
    // ==============================
    if (fr.fenceValue != 0 && m_fence->GetCompletedValue() < fr.fenceValue) {
        m_fence->SetEventOnCompletion(fr.fenceValue, static_cast<HANDLE>(m_fenceEvent));
        WaitForSingleObject(static_cast<HANDLE>(m_fenceEvent), INFINITE);
    }

    // ==============================
    // 3) Allocator を Reset（このフレームの記録を開始できる状態へ）
    // ==============================
    fr.cmdAlloc->Reset();

    // ==============================
    // 4) 初回のみ CommandList を生成（以降は Reset で再利用）
    // ==============================
    if (!m_cmd) {
        ID3D12Device* dev = m_dev->GetDevice();
        HRESULT hr = dev->CreateCommandList(
            /*nodeMask=*/0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            fr.cmdAlloc.Get(),
            /*initialPSO=*/nullptr,
            IID_PPV_ARGS(&m_cmd));
        if (SUCCEEDED(hr)) {
            // Reset 前提に統一したいので、一度 Close しておく
            m_cmd->Close();
        }
    }

    // PSO は呼び出し側で Set する前提（ここでは nullptr）
    // ここから呼び出し側が記録を開始できる
    m_cmd->Reset(fr.cmdAlloc.Get(), nullptr);

    return { fi, m_cmd };
}

void FrameScheduler::EndFrame(RenderTargetHandles* toDispose)
{
    // ==============================
    // 1) BeginFrame で捕捉した fi を使う
    //    （Present によって変わる可能性があるため再取得しない）
    // ==============================
    const unsigned fi = m_inFlightFrameIndex;
    auto& fr = m_frames->Get(fi);

    // ==============================
    // 2) コマンドを提出 & Present
    // ==============================
    m_cmd->Close();
    ID3D12CommandList* lists[] = { m_cmd };
    m_dev->GetQueue()->ExecuteCommandLists(1, lists);
    m_dev->Present(1); // syncInterval=1（VSync有効）。必要に応じて外部から指定しても良い。

    // ==============================
    // 3) Fence 値の管理
    //    - 外部Signalがあっても、常に「completed+1 以上」を次の Signal に使う
    //      → Fence 値の単調増加を保証
    // ==============================
    const std::uint64_t completed = m_fence->GetCompletedValue();
    m_nextFence = std::max<std::uint64_t>(m_nextFence, completed + 1);

    const std::uint64_t sig = m_nextFence++;                  // 今フレームの Signal 値を確定
    m_dev->GetQueue()->Signal(m_fence, sig);                  // GPU へ Signal

    // このフレームの FrameResource に fence を紐づける（次回 Begin の完了待ちで参照）
    fr.fenceValue = sig;

    // ==============================
    // 4) 遅延破棄（RenderTargetHandles）
    //    - このフレームの完了（=sig到達）後に破棄したいものを登録
    //    - 登録後 toDispose はクリア（所有権はキューへ移動済み）
    // ==============================
    if (toDispose && (toDispose->color || toDispose->depth || toDispose->rtvHeap || toDispose->dsvHeap)) {
        if (m_garbage) EnqueueRenderTarget(*m_garbage, sig, std::move(*toDispose));
        *toDispose = {}; // 二重破棄防止のため空にする
    }

    // 破棄可能になった分を回収（毎フレーム呼ぶ：溜め込み防止）
    if (m_garbage) m_garbage->Collect(m_fence);
}

FrameScheduler::~FrameScheduler()
{
    // CommandList はここで解放（Allocator は FrameResources 側が管理）
    if (m_cmd) {
        m_cmd->Release();
        m_cmd = nullptr;
    }
}
