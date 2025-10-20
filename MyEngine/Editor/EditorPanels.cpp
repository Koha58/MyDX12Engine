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
目的：
  - エディタ UI（階層パネル / インスペクタ / ビューポート描画）の実装集。
  - Dear ImGui を用いて、シーン内オブジェクトの閲覧・編集を行う。

設計のポイント：
  - 階層（Hierarchy）はツリー表示。選択は std::weak_ptr<GameObject> で受け渡し。
  - インスペクタ（Inspector）は Transform を簡易編集可能（DragFloat で直接編集）。
  - ビューポート表示（DrawViewportTextureNoEdge）は、半テクセル UV シフトで
    1px のにじみを抑制し、ウィンドウ内のパディング/スペーシングも 0 にして
    「フチなし」表示にする。
  - 即時 GUI の特性上、毎フレーム UI を構築・破棄する前提。

落とし穴：
  - CollapsingHeader のスタイルは push/pop の対応漏れに注意。
  - ImGui の ID 衝突を避けるため、PushID/ユニーク ID を使う。
  - ImTextureID はレンダラ依存（DX12 では GPU 可視 SRV ディスクリプタの可能性）。
===============================================================================
*/

namespace
{
    //--------------------------------------------------------------------------
    // BeginComponent / EndComponent
    // 目的：
    //   Inspector の各コンポーネント枠（見出し＋本文）を作るための薄いヘルパ。
    //   CollapsingHeader を少し“カード”風にするため、Header の色を FrameBg に寄せる。
    // 使い方：
    //   if (BeginComponent("Transform")) { ... EndComponent(); }
    //--------------------------------------------------------------------------
    static bool BeginComponent(const char* title,
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen)
    {
        // 余白を少し広めに
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));

        // ヘッダ色を既存のフレーム色系に合わせ、目立ちすぎない見出しに
        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));

        const bool open = ImGui::CollapsingHeader(title, flags);

        // スタイルは早めに戻す（本文は Group で囲う）
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        if (!open) return false;

        // 本文ブロック開始
        ImGui::BeginGroup();
        return true;
    }

    static void EndComponent()
    {
        // 本文ブロック終了＋区切り線を入れて視認性を上げる
        ImGui::EndGroup();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    //--------------------------------------------------------------------------
    // DrawVec3Row
    // 目的：
    //   Inspector の “Position/Rotation/Scale” のように Vec3 を 1 行で編集する。
    // 実装：
    //   - 4 列テーブル（ラベル / X / Y / Z）
    //   - 軸名は淡色テキスト、値は DragFloat（%.3f）。既定では範囲は設けず自由入力。
    // 引数：
    //   label       : 行のラベル（例："Position"）
    //   x, y, z     : 参照で受け、値を直接編集
    //   labelWidth  : ラベル列の幅
    //   spacing     : 行内のアイテム間隔
    //   dragSpeed   : DragFloat の感度
    // 戻り：
    //   いずれかの成分が変更されたら true
    //--------------------------------------------------------------------------
    static bool DrawVec3Row(const char* label, float& x, float& y, float& z,
        float labelWidth = 90.0f, float spacing = 6.0f, float dragSpeed = 0.01f)
    {
        bool changed = false;
        ImGui::PushID(label); // ラベルで ID 空間をスコープする

        // テーブル内の余白調整
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));

        if (ImGui::BeginTable("t", 4, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, labelWidth);
            ImGui::TableSetupColumn("x", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("y", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("z", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();

            // ラベル列
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);

            // 軸ごとの入力フィールド
            auto axisField = [&](const char* ax, float& v)
                {
                    ImGui::TableNextColumn();
                    ImGui::BeginGroup();

                    // 軸ラベルは淡色
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    ImGui::TextUnformatted(ax);
                    ImGui::PopStyleColor();

                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-1.0f);

                    // “##” でラベル非表示 ID（列ラベル+軸）をユニークに
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
    // 目的：
    //   GameObject ツリーの 1 ノードを描画。選択・開閉に対応。
    //   子が無ければ Leaf 扱いで Push/Pop を省略。
    // 引数：
    //   go       : 描画対象ノード
    //   selected : 現在の選択（weak_ptr）を参照で受け、クリックで更新
    //--------------------------------------------------------------------------
    static void DrawHierarchyNode(const std::shared_ptr<GameObject>& go,
        std::weak_ptr<GameObject>& selected)
    {
        if (!go) return;

        ImGui::PushID(go.get()); // 同名オブジェクト対策にポインタを ID に

        // 選択状態の判定（weak_ptr → shared_ptr へ lock して比較）
        const bool isSelected = (!selected.expired() && selected.lock().get() == go.get());

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
            | ImGuiTreeNodeFlags_SpanFullWidth
            | (isSelected ? ImGuiTreeNodeFlags_Selected : 0);

        // 子が無いなら Leaf 扱い（押下で開閉しない）
        if (go->GetChildren().empty())
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        // ラベルは UTF-8 の name をそのまま
        const bool open = ImGui::TreeNodeEx(EditorPanels::GONameUTF8(go.get()), flags);

        // 左クリックで選択更新（トグルではなく常に選択）
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            selected = go;

        // 子がある場合のみ Push されているので、開いていたら巡回描画
        if (open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
        {
            for (auto& ch : go->GetChildren())
                DrawHierarchyNode(ch, selected);
            ImGui::TreePop();
        }

        ImGui::PopID();
    }
} // namespace(匿名)

namespace EditorPanels
{
    //--------------------------------------------------------------------------
    // GONameUTF8
    // 目的：GameObject 名（UTF-8）を返す。null 安全の薄いラッパ。
    //--------------------------------------------------------------------------
    const char* GONameUTF8(const GameObject* go)
    {
        return go ? go->Name.c_str() : "(null)";
    }

    //--------------------------------------------------------------------------
    // DrawHierarchy
    // 目的：Scene のルート GameObject 配列を列挙し、ツリーを描画。
    //--------------------------------------------------------------------------
    void DrawHierarchy(Scene* scene, std::weak_ptr<GameObject>& selected)
    {
        if (!scene)
        {
            ImGui::TextDisabled("No scene");
            return;
        }

        // ルートから再帰的に描画（子は DrawHierarchyNode 内で処理）
        for (auto& root : scene->GetRootGameObjects())
        {
            DrawHierarchyNode(root, selected);
        }
    }

    //--------------------------------------------------------------------------
    // DrawInspector
    // 目的：選択中 GameObject の情報を表示/編集。
    //   - 今は Transform のみ対応。必要に応じて MeshRenderer などを追加していく。
    //--------------------------------------------------------------------------
    void DrawInspector(const std::weak_ptr<GameObject>& selected)
    {
        if (auto sel = selected.lock())
        {
            ImGui::Text("Selected: %s", GONameUTF8(sel.get()));
            ImGui::Separator();

            // Transform セクション
            if (BeginComponent("Transform"))
            {
                auto& tr = sel->Transform;
                // 位置/回転/スケールを 1 行ずつ編集可能に
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
    // 目的：
    //   ImGui ウィンドウ内にテクスチャを“フチなし”で表示する。
    //   - 表示サイズは小数点以下を切り捨てて整数化（1px ブラー/にじみ軽減）
    //   - UV を半テクセル分だけ内側へずらし、1px の枠が見える問題を軽減
    //   - WindowPadding / FramePadding / ItemSpacing を 0 にして余白を消す
    //
    // 引数：
    //   tex     : ImTextureID（DX12 なら SRV の GPU ハンドル由来）
    //   texW/H  : テクスチャ実サイズ（UV の半テクセル補正に必要）
    //   wantSize: 表示したいサイズ（レイアウト側計算済みの採寸結果を渡す）
    //--------------------------------------------------------------------------
    void DrawViewportTextureNoEdge(ImTextureID tex, int texW, int texH, ImVec2 wantSize)
    {
        if (!tex || texW <= 0 || texH <= 0) return;

        // 表示サイズは整数スナップ
        ImVec2 size(
            static_cast<float>(std::floor(wantSize.x)),
            static_cast<float>(std::floor(wantSize.y))
        );
        // 代替：ImVec2 size(std::floorf(wantSize.x), std::floorf(wantSize.y));

        // 半テクセル UV 補正（DX 既定のピクセルセンターずれ対策）
        const float epsU = 0.5f / static_cast<float>(texW);
        const float epsV = 0.5f / static_cast<float>(texH);
        ImVec2 uv0(epsU, epsV);
        ImVec2 uv1(1.0f - epsU, 1.0f - epsV);

        // 余白 0 にしてフチを消す（push/pop 漏れ注意）
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        ImGui::Image(tex, size, uv0, uv1);

        ImGui::PopStyleVar(3);
    }

} // namespace EditorPanels
