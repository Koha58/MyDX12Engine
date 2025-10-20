#pragma once

/*
===============================================================================
 EditorInterop.h
-------------------------------------------------------------------------------
目的
- ImGui 側（エディタ UI）とゲーム側（コンポーネント/入力処理）で共有したい
  「Scene ビューのホバー/フォーカス状態」を受け渡すための極小ブリッジ。
- 具体例：
  * ImGuiLayer で Scene ウィンドウがホバーされたら SetSceneHovered(true)
  * CameraController などは IsSceneHovered()/IsSceneFocused() を参照して
    入力を受け付けるかどうかを決める

設計メモ
- グローバルなフラグを getter/setter で包んだだけの軽量 API。
- スレッドセーフではありません（通常、UI と入力処理は同スレッドで回す想定）。
- 「直近フレームでの状態」を示すラッチ的な使い方を想定。
  フレームの頭で UI 側が Set～ し、ゲーム側が参照する運用にしてください。
===============================================================================
*/
struct EditorInterop {
    /**
     * @brief Scene ビューがマウスホバー中かどうかを設定する
     * @param v true: ホバー中 / false: 非ホバー
     * @note ImGuiLayer::BuildDockAndWindows など UI 構築時に更新する想定
     */
    static void SetSceneHovered(bool v);

    /**
     * @brief Scene ビューがフォーカス（アクティブ）を持っているか設定する
     * @param v true: フォーカスあり / false: フォーカスなし
     * @note ImGui::IsWindowFocused() 等の結果で更新する想定
     */
    static void SetSceneFocused(bool v);

    /**
     * @brief 現在のフレームで Scene ビューがホバー中かどうか
     * @return true: ホバー中 / false: 非ホバー
     * @note 入力コンポーネント側（カメラ操作など）が参照
     */
    static bool IsSceneHovered();

    /**
     * @brief 現在のフレームで Scene ビューがフォーカスを持つかどうか
     * @return true: フォーカスあり / false: フォーカスなし
     * @note 入力の優先度決定やショートカットの可否などで使用
     */
    static bool IsSceneFocused();
};
