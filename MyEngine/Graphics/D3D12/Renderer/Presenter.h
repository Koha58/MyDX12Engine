#pragma once
#include <d3d12.h>

// d3dx12.h の相対パスはプロジェクト構成に合わせて
#include "d3dx12.h"

/*
    Presenter.h
    ----------------------------------------------------------------------------
    役割：
      - スワップチェインのバックバッファを「描画用に開く → クリア/描画 →
        Present 用に戻す」ための最小限のユーティリティ。
      - Begin() で PRESENT→RENDER_TARGET へ遷移し、RTV/VP/Scissor をセット。
      - End()   で RENDER_TARGET→PRESENT へ戻す。

    想定フロー：
      PresentTargets pt{ rtvHandle, backBuffer, w, h, {r,g,b,a} };
      presenter.Begin(cmd, pt);
      // ここで UI などバックバッファへの描画を行う
      presenter.End(cmd, pt.backBuffer);

    注意：
      - 本ユーティリティは *バックバッファ* 専用。オフスクリーンRTは RenderTarget クラス等で。
      - Begin/End の間で別のリソース状態に遷移しないこと（状態不整合の原因になる）。
*/

// バックバッファに描くときに必要なターゲット情報
struct PresentTargets {
    // スワップチェインのバックバッファに対応する RTV ハンドル（CPU 側）
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};

    // バックバッファのリソース本体（ResourceBarrier で状態遷移に使用）
    ID3D12Resource* backBuffer = nullptr;

    // 現在のバックバッファのピクセルサイズ（Viewport/Scissor 設定に使用）
    UINT width = 0;
    UINT height = 0;

    // クリアカラー（Begin で ClearRenderTargetView に適用）
    float clearColor[4] = { 0.2f, 0.2f, 0.4f, 1.0f };
};

class Presenter {
public:
    /*
        Begin
        ------------------------------------------------------------------------
        やること：
          1) PRESENT → RENDER_TARGET へ状態遷移（Transition Barrier）
          2) OMSetRenderTargets で RTV をセット
          3) ClearRenderTargetView でバックバッファを初期化
          4) Viewport / Scissor をフルサイズに設定

        前提：
          - cmd は DIRECT タイプのグラフィックスコマンドリスト
          - t.backBuffer と t.rtv が有効
    */
    void Begin(ID3D12GraphicsCommandList* cmd, const PresentTargets& t);

    /*
        End
        ------------------------------------------------------------------------
        やること：
          1) RENDER_TARGET → PRESENT へ状態遷移（Transition Barrier）
             （この後キューへサブミットし、Present を呼ぶ想定）

        前提：
          - backBuffer は Begin で使ったものと同一
    */
    void End(ID3D12GraphicsCommandList* cmd, ID3D12Resource* backBuffer);
};
