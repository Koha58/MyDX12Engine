#pragma once
#include <functional>
#include <cstdint>

struct EditorContext {
    bool* pEnableEditor = nullptr;
    bool* pRequestResetLayout = nullptr;  // true が来たら Dock を作り直す
    bool* pAutoRelayout = nullptr;        // ★追加: リサイズ時に比率再構築するか
    std::uint32_t rtWidth = 0;
    std::uint32_t rtHeight = 0;
    float fps = 0.0f;

    std::function<void()> DrawInspector;
    std::function<void()> DrawHierarchy;
};
