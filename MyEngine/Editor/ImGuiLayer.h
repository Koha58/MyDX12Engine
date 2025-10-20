#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <windows.h>
#include "imgui.h"

// 前方宣言（Editor 側の状態を受け渡すための軽量 POD）
struct EditorContext;

/*
    ImGuiLayer
    ----------------------------------------------------------------------------
    目的：
      - Dear ImGui を DirectX 12 で駆動するための薄いラッパ。
      - 初期化／フレーム境界／描画コマンド記録／終了処理 を一本化。
      - 任意のテクスチャ（ID3D12Resource）に対する SRV を「指定スロット」に作り、
        ImGui::Image で使える ImTextureID を返すユーティリティを提供。

    設計メモ：
      - SRV は「GPU 可視の CBV/SRV/UAV ヒープ（Shader Visible）」に作る。
      - ImTextureID は imgui_impl_dx12 の仕様により “GPU ディスクリプタハンドル” を整数化したもの。
      - フレーム毎のダブル/トリプルバッファと衝突しないよう、呼び出し側は
          baseSlot + frameIndex
        の形でスロットを管理する（例：Scene 用は 1000 番台、Game 用は 2000 番台など）。
      - CreateOrUpdateTextureSRV() は同一スロットの再発行を許容（上書き）する。
        テクスチャ側の寿命管理（GPU 使用中に解放しない）は呼び出し側の責務。
      - スレッドセーフではない（UI スレッド/描画スレッドからのみ呼ぶ前提）。
*/
class ImGuiLayer
{
public:
    /*
        Initialize
        ------------------------------------------------------------------------
        - Win32 + DX12 用の ImGui 実装を初期化し、内部 SRV ヒープを確保する。
        - numFramesInFlight は imgui_impl_dx12 のフレーム数（通常は SwapChain の数）。
        - rtvFormat/dsvFormat は ImGui のレンダーターゲット用設定（パイプライン整合のため）。
        戻り値：成功で true
    */
    bool Initialize(HWND hwnd,
        ID3D12Device* device,
        ID3D12CommandQueue* queue,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat,
        UINT numFramesInFlight);

    /*
        NewFrame
        ------------------------------------------------------------------------
        - 1フレームの ImGui 記録を開始（ImGui::NewFrame のラッパ）。
        - ImGui ウィジェット構築（BuildDockAndWindows）より前に呼ぶ。
    */
    void NewFrame();

    /*
        BuildDockAndWindows
        ------------------------------------------------------------------------
        - ドッキングスペースを構築し、各種パネル（Hierarchy / Inspector / Viewport 等）
          のウィンドウを生成する。
        - EditorContext を介して、テクスチャIDや表示サイズ、Hover/Focus 状態などを
          受け取り/書き戻す。
    */
    void BuildDockAndWindows(EditorContext& ctx);

    /*
        Render
        ------------------------------------------------------------------------
        - ImGui の描画コマンドを受け取り、cmd（ID3D12GraphicsCommandList）へ記録。
        - 事前に OM（バックバッファの RTV 設定など）が済んでいる想定。
        - この関数は「1フレームに1回」呼ぶ。
    */
    void Render(ID3D12GraphicsCommandList* cmd);

    /*
        Shutdown
        ------------------------------------------------------------------------
        - ImGui 実装のリソース解放。アプリ終了時に呼ぶ。
    */
    void Shutdown();

    /*
        CreateOrUpdateTextureSRV
        ------------------------------------------------------------------------
        目的：
          - 指定の D3D12 テクスチャ（Resource）に対応する SRV を、
            “CPU/GPU ディスクリプタヒープの指定スロット” に作成/更新。
          - ImGui::Image に渡せる ImTextureID を返す（＝GPU ディスクリプタハンドルを整数化）。

        引数：
          tex  : SRV を作成したい ID3D12Resource（Texture2D など）。
          fmt  : SRV のフォーマット（DXGI_FORMAT_R8G8B8A8_UNORM 等）。
          slot : ヒープ上のインデックス。フレーム別で衝突しないよう呼び出し側で管理。

        重要：
          - この関数は内部ヒープの範囲チェックを行う想定（実装側で assert / 拡張）。
          - 同一 slot へ複数回呼ぶと上書き（Update）になる。
          - tex の寿命は呼び出し側が管理する（GPU 使用中 Release 防止は別層で）。
    */
    ImTextureID CreateOrUpdateTextureSRV(ID3D12Resource* tex, DXGI_FORMAT fmt, UINT slot);

private:
    // GPU 可視 SRV ヒープ（imgui_impl_dx12 互換）
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;

    // 初期化済みフラグ（多重初期化/終了を避けるため）
    bool m_initialized = false;

    // 便宜的に保持：SRV 作成に必要（インクリメントサイズや基点ハンドル）
    ID3D12Device* m_device = nullptr;
    UINT m_srvIncSize = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvCpuStart{}; // ヒープ先頭の CPU ハンドル
    D3D12_GPU_DESCRIPTOR_HANDLE m_srvGpuStart{}; // ヒープ先頭の GPU ハンドル
};
