#pragma once
#include <functional>
#include <cstdint>

struct EditorContext {
    bool* pEnableEditor = nullptr;
    bool* pRequestResetLayout = nullptr;  // true �������� Dock ����蒼��
    bool* pAutoRelayout = nullptr;        // ���ǉ�: ���T�C�Y���ɔ䗦�č\�z���邩
    std::uint32_t rtWidth = 0;
    std::uint32_t rtHeight = 0;
    float fps = 0.0f;

    std::function<void()> DrawInspector;
    std::function<void()> DrawHierarchy;
};
