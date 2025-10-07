#pragma once
#include <functional>
#include <cstdint>
#include "imgui.h"

struct EditorContext {
    bool* pEnableEditor = nullptr;
    bool* pRequestResetLayout = nullptr;
    bool* pAutoRelayout = nullptr;

    // Scene
    ImTextureID sceneTexId = 0;
    ImVec2      sceneViewportSize = ImVec2(0, 0);
    bool        sceneHovered = false, sceneFocused = false;
    unsigned    sceneRTWidth = 0, sceneRTHeight = 0;

    // Game
    ImTextureID gameTexId = 0;
    ImVec2      gameViewportSize = ImVec2(0, 0);
    bool        gameHovered = false, gameFocused = false;
    unsigned    gameRTWidth = 0, gameRTHeight = 0;

    std::uint32_t rtWidth = 0;
    std::uint32_t rtHeight = 0;
    float fps = 0.0f;

    std::function<void()> DrawInspector;
    std::function<void()> DrawHierarchy;
};

