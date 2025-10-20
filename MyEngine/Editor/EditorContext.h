#pragma once
#include <functional>
#include <cstdint>
#include "imgui.h"

/*
    EditorContext
    ----------------------------------------------------------------------------
    役割：
      - レンダラ側 ⇔ ImGui 側で共有する“編集 UI の状態”の受け渡し用 POD。
      - 1フレームの中で「レンダラが埋める → ImGui が参照/更新 → レンダラが結果を読む」
        という片方向データフローを想定。

    ライフサイクル・更新順のイメージ：
      1) レンダラがフレーム冒頭で、Scene/Game の SRV や実RTサイズ等を埋める
         （例：SceneLayer::FeedToUI / SyncStatsTo で更新）
      2) ImGui 側（EditorPanels など）がこの値を見てウィンドウ構築・入力取得
      3) 結果（例えば「Scene ビューポートの実表示サイズ」や「Hovered/Focused」）を
         この構造体に書き戻す
      4) レンダラは書き戻された値（sceneViewportSize など）を使って
         次フレームのリサイズ要求等を処理

    用語：
      - RTサイズ（sceneRTWidth/Height, gameRTWidth/Height）：
        実際のオフスクリーン RenderTarget のピクセルサイズ
      - ViewportSize（sceneViewportSize, gameViewportSize）：
        ImGui 上にテクスチャを何ピクセル四方で描くか（＝可視領域の希望サイズ）
      - ImTextureID（sceneTexId, gameTexId）：
        imgui_impl_dx12 が内部で保持する「GPU 可視 SRV ディスクリプタ」を指す ID
*/

struct EditorContext {
    // ----------------------------------------------------------------------------
    // トグル・リレイアウト
    // ----------------------------------------------------------------------------
    bool* pEnableEditor = nullptr; // エディタ UI 全体の有効/無効（チェックボックス連動など）
    bool* pRequestResetLayout = nullptr; // true にするとレイアウトを初期状態にリセット（1フレ使い捨て）
    bool* pAutoRelayout = nullptr; // 自動レイアウト最適化を有効化するか（運用ポリシー任せ）

    // ----------------------------------------------------------------------------
    // Scene ビュー（オフスクリーン RT を ImGui に表示）
    // ----------------------------------------------------------------------------
    ImTextureID sceneTexId = 0;               // ImGui::Image に渡す SRV の ID（imgui_impl_dx12 発行）
    ImVec2      sceneViewportSize = ImVec2(0, 0); // 実際に ImGui 上で描画された矩形サイズ（ピクセル）
    bool        sceneHovered = false;           // 今フレーム、Scene ビューがマウスホバーされているか
    bool        sceneFocused = false;           // 今フレーム、Scene ビューがキーボードフォーカスを持つか
    unsigned    sceneRTWidth = 0;               // 実 RT の幅（ピクセル）※ Viewports::FeedToUI で埋める
    unsigned    sceneRTHeight = 0;               // 実 RT の高さ（ピクセル）

    // ----------------------------------------------------------------------------
    // Game ビュー（固定カメラ出力を ImGui に表示）
    //   - 「向き(View)は固定・投影(Proj)のみアスペクト追従」にすることで、
    //     ウィンドウリサイズで“向きが変わる”のを防ぐ設計（Viewports 側参照）
    // ----------------------------------------------------------------------------
    ImTextureID gameTexId = 0;               // Game 用 SRV の ID
    ImVec2      gameViewportSize = ImVec2(0, 0);  // ImGui 上での表示サイズ（ピクセル）
    bool        gameHovered = false;           // 今フレーム、Game ビューがホバーされているか
    bool        gameFocused = false;           // 今フレーム、Game ビューがフォーカスを持つか
    unsigned    gameRTWidth = 0;               // 実 RT の幅（ピクセル）
    unsigned    gameRTHeight = 0;               // 実 RT の高さ（ピクセル）

    // ----------------------------------------------------------------------------
    // スワップチェイン（バックバッファ）側の情報・統計
    // ----------------------------------------------------------------------------
    std::uint32_t rtWidth = 0;               // 現在のバックバッファ幅（SwapChain の実ピクセル）
    std::uint32_t rtHeight = 0;               // 現在のバックバッファ高
    float         fps = 0.0f;            // ImGui::GetIO().Framerate 等から埋める

    // ----------------------------------------------------------------------------
    // パネル描画のエントリポイント（呼び出し側がラムダを詰める）
    //   - Hierarchy/Inspector の描画は関数ポインタではなく std::function で受ける
    // ----------------------------------------------------------------------------
    std::function<void()> DrawInspector;          // 例：選択中 GameObject の Transform 等
    std::function<void()> DrawHierarchy;          // 例：シーンツリー（親子関係の可視化）

    // ----------------------------------------------------------------------------
    // フラグ：Scene ビューのリサイズ操作が進行中か
    //   - 例：ドラッグでサイズが揺れている間は true、安定したら false
    //   - デバウンスの調整や「確定したら RT を作り直す」等の判定に使える
    // ----------------------------------------------------------------------------
    bool sceneResizing = false;
};
