#pragma once
#include <functional>
#include <cstdint>
#include "imgui.h"

struct EditorContext {
    bool* pEnableEditor = nullptr;
    bool* pRequestResetLayout = nullptr;  // true �������� Dock ����蒼��
    bool* pAutoRelayout = nullptr;        // ���ǉ�: ���T�C�Y���ɔ䗦�č\�z���邩

    // �r���[�|�[�g���iImGui�������t���[�����߂�j
    ImVec2 sceneViewportSize = ImVec2(0, 0);
    ImVec2 gameViewportSize = ImVec2(0, 0);
    bool sceneHovered = false;
    bool sceneFocused = false;
    bool gameHovered  = false;
    bool gameFocused  = false;

    // �����F�I�t�X�N���[��RT�̓\��t���Ɏg��
    ImTextureID sceneTexture{0};
    ImTextureID gameTexture{0};

    std::uint32_t rtWidth = 0;
    std::uint32_t rtHeight = 0;
    float fps = 0.0f;

    std::function<void()> DrawInspector;
    std::function<void()> DrawHierarchy;
};
