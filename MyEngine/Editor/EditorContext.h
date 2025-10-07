#pragma once
#include <functional>
#include <cstdint>
#include "imgui.h"

struct EditorContext {
    bool* pEnableEditor = nullptr;
    bool* pRequestResetLayout = nullptr;  // true が来たら Dock を作り直す
    bool* pAutoRelayout = nullptr;        // ★追加: リサイズ時に比率再構築するか

    // ビューポート情報（ImGui側が毎フレーム埋める）
    ImVec2 sceneViewportSize = ImVec2(0, 0);
    ImVec2 gameViewportSize = ImVec2(0, 0);
    bool sceneHovered = false;
    bool sceneFocused = false;
    bool gameHovered  = false;
    bool gameFocused  = false;

    // 将来：オフスクリーンRTの貼り付けに使う
    ImTextureID sceneTexture{0};
    ImTextureID gameTexture{0};

    std::uint32_t rtWidth = 0;
    std::uint32_t rtHeight = 0;
    float fps = 0.0f;

    std::function<void()> DrawInspector;
    std::function<void()> DrawHierarchy;
};
