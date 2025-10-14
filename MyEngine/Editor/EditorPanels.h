#pragma once
#include <memory>
#include "imgui.h"  // ★ 必須: ImTextureID, ImVec2 の定義

// 前方宣言（ヘッダーで重い依存を避ける）
class Scene;
class GameObject;

namespace EditorPanels
{
    // GameObject名(UTF-8)
    const char* GONameUTF8(const GameObject* go);

    // Hierarchy(選択の更新を含む)
    void DrawHierarchy(Scene* scene, std::weak_ptr<GameObject>& selected);

    // Inspector(Transformだけ先に)
    void DrawInspector(const std::weak_ptr<GameObject>& selected);

    // RT を “枠なし” で貼るユーティリティ
    void DrawViewportTextureNoEdge(ImTextureID tex, int texW, int texH, ImVec2 wantSize);
}
