#pragma once
#include <memory>
#include "Scene/Scene.h"

namespace EditorPanels
{
	// GameObject–¼(UTF-8)
	const char* GONameUTF8(const GameObject* go);

	// Hierarchy(‘I‘ğ‚ÌXV‚ğŠÜ‚Ş)
	void DrawHierarchy(Scene* scene, std::weak_ptr<GameObject>& selected);

	// Inspector(Transform‚¾‚¯æ‚É)
	void DrawInspector(const std::weak_ptr<GameObject>& selected);
}
