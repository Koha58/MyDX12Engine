#pragma once
struct EditorInterop {
    static void SetSceneHovered(bool v);
    static void SetSceneFocused(bool v);
    static bool IsSceneHovered();
    static bool IsSceneFocused();
};

