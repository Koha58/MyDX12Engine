#include "EditorInterop.h"

namespace {
    bool g_sceneHovered = false;
    bool g_sceneFocused = false;
}

void EditorInterop::SetSceneHovered(bool v) { g_sceneHovered = v; }
void EditorInterop::SetSceneFocused(bool v) { g_sceneFocused = v; }
bool EditorInterop::IsSceneHovered() { return g_sceneHovered; }
bool EditorInterop::IsSceneFocused() { return g_sceneFocused; }
