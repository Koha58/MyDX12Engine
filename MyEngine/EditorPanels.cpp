#include "EditorPanels.h"
#include "Scene/Scene.h"
#include"Scene/GameObject.h"
#include"Components/TransformComponent.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "imgui_internal.h"
#include <string>

namespace
{
    // Inspector ê‹ÇËèÙÇ›ÉwÉbÉ_ÇÃä»à’ÉwÉãÉp
    static bool BeginComponent(const char* title, ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));
        ImGui::PushStyleColor(ImGuiCol_Header,        ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
        const bool open = ImGui::CollapsingHeader(title, flags);
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        if (!open) return false;
        ImGui::BeginGroup();
        return true;
    }
    static void EndComponent() { ImGui::EndGroup(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); }

    static bool DrawVec3Row(const char* label, float& x, float& y, float& z,
        float labelWidth = 90.0f, float spacing = 6.0f, float dragSpeed = 0.01f)
    {
        bool changed = false;
        ImGui::PushID(label);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));
        if (ImGui::BeginTable("t", 4, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, labelWidth);
            ImGui::TableSetupColumn("x", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("y", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("z", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);

            auto axisField = [&](const char* ax, float& v)
                {
                    ImGui::TableNextColumn();
                    ImGui::BeginGroup();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    ImGui::TextUnformatted(ax);
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-1.0f);
                    std::string id = std::string("##") + label + "_" + ax;
                    changed |= ImGui::DragFloat(id.c_str(), &v, dragSpeed, 0.0f, 0.0f, "%.3f");
                    ImGui::EndGroup();
                };
            axisField("X", x); axisField("Y", y); axisField("Z", z);
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
        ImGui::PopID();
        return changed;
    }

    static void DrawHierarchyNode(const std::shared_ptr<GameObject>& go, std::weak_ptr<GameObject>& selected)
    {
        if (!go) return;
        ImGui::PushID(go.get());
        const bool isSelected = (!selected.expired() && selected.lock().get() == go.get());

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
            | ImGuiTreeNodeFlags_SpanFullWidth
            | (isSelected ? ImGuiTreeNodeFlags_Selected : 0);

        if (go->GetChildren().empty())
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        const bool open = ImGui::TreeNodeEx(EditorPanels::GONameUTF8(go.get()), flags);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            selected = go;

        if (open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
        {
            for (auto& ch : go->GetChildren())
                DrawHierarchyNode(ch, selected);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

namespace EditorPanels
{
    const char* GONameUTF8(const GameObject* go)
    {
        return go ? go->Name.c_str() : "(null)";
    }

    void DrawHierarchy(Scene* scene, std::weak_ptr<GameObject>& selected)
    {
        if (!scene)
        {
            ImGui::TextDisabled("No scene");
            return;
        }

        for (auto& root : scene->GetRootGameObjects())
        {
            DrawHierarchyNode(root, selected);
        }
    }

    void DrawInspector(const std::weak_ptr<GameObject>& selected)
    {
        if (auto sel = selected.lock())
        {
            ImGui::Text("Selected: %s", GONameUTF8(sel.get()));
            ImGui::Separator();
            if (BeginComponent("Transform"))
            {
                auto& tr = sel->Transform;
                DrawVec3Row("Position", tr->Position.x, tr->Position.y, tr->Position.z);
                DrawVec3Row("Rotation", tr->Rotation.x, tr->Rotation.y, tr->Rotation.z);
                DrawVec3Row("Scale", tr->Scale.x, tr->Scale.y, tr->Scale.z);
                EndComponent();
            }
        }
        else {
            ImGui::TextDisabled("No selection");
        }
    }
}