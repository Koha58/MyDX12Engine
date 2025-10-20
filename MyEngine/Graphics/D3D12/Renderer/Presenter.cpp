#include "Renderer/Presenter.h"

/*
    Presenter
    ----------------------------------------------------------------------------
    役割：
      - スワップチェインのバックバッファを「描画可能にする → クリア/描画 →
        Present 可能に戻す」までの定型処理をカプセル化。
    使い方（1フレーム）：
      1) Begin(cmd, targets)   // RT 状態へ遷移 + RTV設定 + クリア + VP/Scissor 設定
      2) ... バックバッファへ描画 ...
      3) End(cmd, backBuffer)  // Present 状態へ遷移（この後 FrameScheduler 側で Present）
    注意：
      - バリアは「現在の正しい状態」から「次に必要な状態」への遷移であることが重要。
      - ここではバックバッファは前フレームで Present 済みと仮定して
        PRESENT → RENDER_TARGET → PRESENT の順で遷移している。
      - 深度バッファを使う描画を行う場合は、Begin 前後 or 直後に
        DSV の設定やクリアを呼び出し側で行うこと。
*/

void Presenter::Begin(ID3D12GraphicsCommandList* cmd, const PresentTargets& t)
{
    // ==============================
    // 1) 状態遷移: PRESENT → RENDER_TARGET
    //    前フレームで Present に戻っているバックバッファを描画可能にする。
    //    ※ 現状態が異なると DRED/検証レイヤが警告するので、運用で整合を保つこと。
    // ==============================
    auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(
        t.backBuffer,
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmd->ResourceBarrier(1, &toRT);

    // ==============================
    // 2) OM に RTV をセットして、バックバッファをクリア
    //    - DSV が必要なら呼び出し側で OMSetRenderTargets を
    //      RTV+DSV で再設定してから ClearDepthStencilView を呼ぶこと。
    // ==============================
    cmd->OMSetRenderTargets(1, &t.rtv, FALSE, nullptr);
    cmd->ClearRenderTargetView(t.rtv, t.clearColor, 0, nullptr);

    // ==============================
    // 3) RS: ビューポート/シザーをフルスクリーンに設定
    //    ImGui などを描く前に画面サイズへ合わせておく。
    //    ※ t.width/height はスワップチェインの現在サイズと一致している必要がある。
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
    // 状態遷移: RENDER_TARGET → PRESENT
    // この後、キューにサブミット＆スワップチェイン Present（外側のフローで実施）。
    // ==============================
    auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    cmd->ResourceBarrier(1, &toPresent);
}

/*
【よくある落とし穴】
- Clear/描画前に RT 状態へ遷移していない：
  → PRESENT のまま OMSet/Clear すると検証エラー/未定義動作。Begin を必ず通す。
- ビューポート/シザーが古いサイズのまま：
  → リサイズ後に幅/高さが変わるため、毎フレーム設定が安全（本実装の通り）。
- DSV を使うのに設定していない：
  → 深度テストが必要な描画をする場合、Begin 直後に RTV+DSV を OM へ設定し直し、
     ClearDepthStencilView も行うこと（本クラスは RTV のみ扱う想定）。
*/
