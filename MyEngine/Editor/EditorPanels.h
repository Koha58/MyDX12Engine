#pragma once
#include <memory>
#include "Scene/Scene.h"

namespace EditorPanels
{
	// GameObject��(UTF-8)
	const char* GONameUTF8(const GameObject* go);

	// Hierarchy(�I���̍X�V���܂�)
	void DrawHierarchy(Scene* scene, std::weak_ptr<GameObject>& selected);

	// Inspector(Transform�������)
	void DrawInspector(const std::weak_ptr<GameObject>& selected);
}
