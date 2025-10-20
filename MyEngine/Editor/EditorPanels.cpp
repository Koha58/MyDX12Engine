#include "EditorPanels.h"
#include "Scene/Scene.h"
#include "Scene/GameObject.h"
#include "Components/TransformComponent.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "imgui_internal.h"

#include <string>
#include <cmath>

/*
===============================================================================
EditorPanels.cpp
-------------------------------------------------------------------------------
�ړI�F
  - �G�f�B�^ UI�i�K�w�p�l�� / �C���X�y�N�^ / �r���[�|�[�g�`��j�̎����W�B
  - Dear ImGui ��p���āA�V�[�����I�u�W�F�N�g�̉{���E�ҏW���s���B

�݌v�̃|�C���g�F
  - �K�w�iHierarchy�j�̓c���[�\���B�I���� std::weak_ptr<GameObject> �Ŏ󂯓n���B
  - �C���X�y�N�^�iInspector�j�� Transform ���ȈՕҏW�\�iDragFloat �Œ��ڕҏW�j�B
  - �r���[�|�[�g�\���iDrawViewportTextureNoEdge�j�́A���e�N�Z�� UV �V�t�g��
    1px �̂ɂ��݂�}�����A�E�B���h�E���̃p�f�B���O/�X�y�[�V���O�� 0 �ɂ���
    �u�t�`�Ȃ��v�\���ɂ���B
  - ���� GUI �̓�����A���t���[�� UI ���\�z�E�j������O��B

���Ƃ����F
  - CollapsingHeader �̃X�^�C���� push/pop �̑Ή��R��ɒ��ӁB
  - ImGui �� ID �Փ˂�����邽�߁APushID/���j�[�N ID ���g���B
  - ImTextureID �̓����_���ˑ��iDX12 �ł� GPU �� SRV �f�B�X�N���v�^�̉\���j�B
===============================================================================
*/

namespace
{
    //--------------------------------------------------------------------------
    // BeginComponent / EndComponent
    // �ړI�F
    //   Inspector �̊e�R���|�[�l���g�g�i���o���{�{���j����邽�߂̔����w���p�B
    //   CollapsingHeader �������g�J�[�h�h���ɂ��邽�߁AHeader �̐F�� FrameBg �Ɋ񂹂�B
    // �g�����F
    //   if (BeginComponent("Transform")) { ... EndComponent(); }
    //--------------------------------------------------------------------------
    static bool BeginComponent(const char* title,
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen)
    {
        // �]���������L�߂�
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));

        // �w�b�_�F�������̃t���[���F�n�ɍ��킹�A�ڗ��������Ȃ����o����
        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));

        const bool open = ImGui::CollapsingHeader(title, flags);

        // �X�^�C���͑��߂ɖ߂��i�{���� Group �ň͂��j
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        if (!open) return false;

        // �{���u���b�N�J�n
        ImGui::BeginGroup();
        return true;
    }

    static void EndComponent()
    {
        // �{���u���b�N�I���{��؂�������Ď��F�����グ��
        ImGui::EndGroup();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    //--------------------------------------------------------------------------
    // DrawVec3Row
    // �ړI�F
    //   Inspector �� �gPosition/Rotation/Scale�h �̂悤�� Vec3 �� 1 �s�ŕҏW����B
    // �����F
    //   - 4 ��e�[�u���i���x�� / X / Y / Z�j
    //   - �����͒W�F�e�L�X�g�A�l�� DragFloat�i%.3f�j�B����ł͔͈݂͂͐������R���́B
    // �����F
    //   label       : �s�̃��x���i��F"Position"�j
    //   x, y, z     : �Q�ƂŎ󂯁A�l�𒼐ڕҏW
    //   labelWidth  : ���x����̕�
    //   spacing     : �s���̃A�C�e���Ԋu
    //   dragSpeed   : DragFloat �̊��x
    // �߂�F
    //   �����ꂩ�̐������ύX���ꂽ�� true
    //--------------------------------------------------------------------------
    static bool DrawVec3Row(const char* label, float& x, float& y, float& z,
        float labelWidth = 90.0f, float spacing = 6.0f, float dragSpeed = 0.01f)
    {
        bool changed = false;
        ImGui::PushID(label); // ���x���� ID ��Ԃ��X�R�[�v����

        // �e�[�u�����̗]������
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));

        if (ImGui::BeginTable("t", 4, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, labelWidth);
            ImGui::TableSetupColumn("x", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("y", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("z", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();

            // ���x����
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);

            // �����Ƃ̓��̓t�B�[���h
            auto axisField = [&](const char* ax, float& v)
                {
                    ImGui::TableNextColumn();
                    ImGui::BeginGroup();

                    // �����x���͒W�F
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    ImGui::TextUnformatted(ax);
                    ImGui::PopStyleColor();

                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-1.0f);

                    // �g##�h �Ń��x����\�� ID�i�񃉃x��+���j�����j�[�N��
                    std::string id = std::string("##") + label + "_" + ax;
                    changed |= ImGui::DragFloat(id.c_str(), &v, dragSpeed, 0.0f, 0.0f, "%.3f");

                    ImGui::EndGroup();
                };
            axisField("X", x); axisField("Y", y); axisField("Z", z);

            ImGui::EndTable();
        }

        ImGui::PopStyleVar(); // ItemSpacing
        ImGui::PopID();
        return changed;
    }

    //--------------------------------------------------------------------------
    // DrawHierarchyNode
    // �ړI�F
    //   GameObject �c���[�� 1 �m�[�h��`��B�I���E�J�ɑΉ��B
    //   �q��������� Leaf ������ Push/Pop ���ȗ��B
    // �����F
    //   go       : �`��Ώۃm�[�h
    //   selected : ���݂̑I���iweak_ptr�j���Q�ƂŎ󂯁A�N���b�N�ōX�V
    //--------------------------------------------------------------------------
    static void DrawHierarchyNode(const std::shared_ptr<GameObject>& go,
        std::weak_ptr<GameObject>& selected)
    {
        if (!go) return;

        ImGui::PushID(go.get()); // �����I�u�W�F�N�g�΍�Ƀ|�C���^�� ID ��

        // �I����Ԃ̔���iweak_ptr �� shared_ptr �� lock ���Ĕ�r�j
        const bool isSelected = (!selected.expired() && selected.lock().get() == go.get());

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
            | ImGuiTreeNodeFlags_SpanFullWidth
            | (isSelected ? ImGuiTreeNodeFlags_Selected : 0);

        // �q�������Ȃ� Leaf �����i�����ŊJ���Ȃ��j
        if (go->GetChildren().empty())
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        // ���x���� UTF-8 �� name �����̂܂�
        const bool open = ImGui::TreeNodeEx(EditorPanels::GONameUTF8(go.get()), flags);

        // ���N���b�N�őI���X�V�i�g�O���ł͂Ȃ���ɑI���j
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            selected = go;

        // �q������ꍇ�̂� Push ����Ă���̂ŁA�J���Ă����珄��`��
        if (open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
        {
            for (auto& ch : go->GetChildren())
                DrawHierarchyNode(ch, selected);
            ImGui::TreePop();
        }

        ImGui::PopID();
    }
} // namespace(����)

namespace EditorPanels
{
    //--------------------------------------------------------------------------
    // GONameUTF8
    // �ړI�FGameObject ���iUTF-8�j��Ԃ��Bnull ���S�̔������b�p�B
    //--------------------------------------------------------------------------
    const char* GONameUTF8(const GameObject* go)
    {
        return go ? go->Name.c_str() : "(null)";
    }

    //--------------------------------------------------------------------------
    // DrawHierarchy
    // �ړI�FScene �̃��[�g GameObject �z���񋓂��A�c���[��`��B
    //--------------------------------------------------------------------------
    void DrawHierarchy(Scene* scene, std::weak_ptr<GameObject>& selected)
    {
        if (!scene)
        {
            ImGui::TextDisabled("No scene");
            return;
        }

        // ���[�g����ċA�I�ɕ`��i�q�� DrawHierarchyNode ���ŏ����j
        for (auto& root : scene->GetRootGameObjects())
        {
            DrawHierarchyNode(root, selected);
        }
    }

    //--------------------------------------------------------------------------
    // DrawInspector
    // �ړI�F�I�� GameObject �̏���\��/�ҏW�B
    //   - ���� Transform �̂ݑΉ��B�K�v�ɉ����� MeshRenderer �Ȃǂ�ǉ����Ă����B
    //--------------------------------------------------------------------------
    void DrawInspector(const std::weak_ptr<GameObject>& selected)
    {
        if (auto sel = selected.lock())
        {
            ImGui::Text("Selected: %s", GONameUTF8(sel.get()));
            ImGui::Separator();

            // Transform �Z�N�V����
            if (BeginComponent("Transform"))
            {
                auto& tr = sel->Transform;
                // �ʒu/��]/�X�P�[���� 1 �s���ҏW�\��
                DrawVec3Row("Position", tr->Position.x, tr->Position.y, tr->Position.z);
                DrawVec3Row("Rotation", tr->Rotation.x, tr->Rotation.y, tr->Rotation.z);
                DrawVec3Row("Scale", tr->Scale.x, tr->Scale.y, tr->Scale.z);
                EndComponent();
            }
        }
        else
        {
            ImGui::TextDisabled("No selection");
        }
    }

    //--------------------------------------------------------------------------
    // DrawViewportTextureNoEdge
    // �ړI�F
    //   ImGui �E�B���h�E���Ƀe�N�X�`�����g�t�`�Ȃ��h�ŕ\������B
    //   - �\���T�C�Y�͏����_�ȉ���؂�̂ĂĐ������i1px �u���[/�ɂ��݌y���j
    //   - UV �𔼃e�N�Z�������������ւ��炵�A1px �̘g������������y��
    //   - WindowPadding / FramePadding / ItemSpacing �� 0 �ɂ��ė]��������
    //
    // �����F
    //   tex     : ImTextureID�iDX12 �Ȃ� SRV �� GPU �n���h���R���j
    //   texW/H  : �e�N�X�`�����T�C�Y�iUV �̔��e�N�Z���␳�ɕK�v�j
    //   wantSize: �\���������T�C�Y�i���C�A�E�g���v�Z�ς݂̍̐����ʂ�n���j
    //--------------------------------------------------------------------------
    void DrawViewportTextureNoEdge(ImTextureID tex, int texW, int texH, ImVec2 wantSize)
    {
        if (!tex || texW <= 0 || texH <= 0) return;

        // �\���T�C�Y�͐����X�i�b�v
        ImVec2 size(
            static_cast<float>(std::floor(wantSize.x)),
            static_cast<float>(std::floor(wantSize.y))
        );
        // ��ցFImVec2 size(std::floorf(wantSize.x), std::floorf(wantSize.y));

        // ���e�N�Z�� UV �␳�iDX ����̃s�N�Z���Z���^�[����΍�j
        const float epsU = 0.5f / static_cast<float>(texW);
        const float epsV = 0.5f / static_cast<float>(texH);
        ImVec2 uv0(epsU, epsV);
        ImVec2 uv1(1.0f - epsU, 1.0f - epsV);

        // �]�� 0 �ɂ��ăt�`�������ipush/pop �R�꒍�Ӂj
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        ImGui::Image(tex, size, uv0, uv1);

        ImGui::PopStyleVar(3);
    }

} // namespace EditorPanels
