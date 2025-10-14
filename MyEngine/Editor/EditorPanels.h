#pragma once
#include <memory>
#include "imgui.h"  // �� �K�{: ImTextureID, ImVec2 �̒�`

// �O���錾�i�w�b�_�[�ŏd���ˑ��������j
class Scene;
class GameObject;

namespace EditorPanels
{
    // GameObject��(UTF-8)
    const char* GONameUTF8(const GameObject* go);

    // Hierarchy(�I���̍X�V���܂�)
    void DrawHierarchy(Scene* scene, std::weak_ptr<GameObject>& selected);

    // Inspector(Transform�������)
    void DrawInspector(const std::weak_ptr<GameObject>& selected);

    // RT �� �g�g�Ȃ��h �œ\�郆�[�e�B���e�B
    void DrawViewportTextureNoEdge(ImTextureID tex, int texW, int texH, ImVec2 wantSize);
}
