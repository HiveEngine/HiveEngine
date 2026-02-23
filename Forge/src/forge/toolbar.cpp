#include <forge/toolbar.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>

namespace forge
{
    static float StoL(float s) { return s <= 0.04045f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f); }
    static ImVec4 L(float r, float g, float b, float a) { return {StoL(r), StoL(g), StoL(b), a}; }

    static const ImVec4 kGreen  = L(0.298f, 0.686f, 0.314f, 1.f);
    static const ImVec4 kAccent = L(0.000f, 0.471f, 0.831f, 1.f);
    static const ImVec4 kRed    = L(0.820f, 0.200f, 0.200f, 1.f);
    static const ImVec4 kAmber  = L(0.900f, 0.680f, 0.050f, 1.f);

    static bool ColoredButton(const char* label, const ImVec4& color)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            {color.x * 1.2f, color.y * 1.2f, color.z * 1.2f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
            {color.x * 0.8f, color.y * 0.8f, color.z * 0.8f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text, L(1.f, 1.f, 1.f, 1.f));
        bool pressed = ImGui::Button(label);
        ImGui::PopStyleColor(4);
        return pressed;
    }

    static bool ToggleButton(const char* label, bool active)
    {
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, kAccent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccent);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccent);
            ImGui::PushStyleColor(ImGuiCol_Text, L(1.f, 1.f, 1.f, 1.f));
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, L(0.15f, 0.15f, 0.15f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, L(0.25f, 0.25f, 0.25f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccent);
            ImGui::PushStyleColor(ImGuiCol_Text, L(0.7f, 0.7f, 0.7f, 1.f));
        }

        bool pressed = ImGui::Button(label);
        ImGui::PopStyleColor(4);
        return pressed;
    }

    ToolbarAction DrawToolbarButtons(PlayState play_state, GizmoState& gizmo)
    {
        ToolbarAction action{};

        // Play / Pause / Stop
        if (play_state == PlayState::Editing)
        {
            action.play_pressed = ColoredButton("Play", kGreen);
        }
        else if (play_state == PlayState::Playing)
        {
            action.pause_pressed = ColoredButton("Pause", kAmber);
            ImGui::SameLine();
            action.stop_pressed = ColoredButton("Stop", kRed);
        }
        else
        {
            action.play_pressed = ColoredButton("Resume", kGreen);
            ImGui::SameLine();
            action.stop_pressed = ColoredButton("Stop", kRed);
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 2.f);
        ImGui::SameLine();

        // Gizmo mode
        if (ToggleButton("Move", gizmo.mode == GizmoMode::Translate))
            gizmo.mode = GizmoMode::Translate;
        ImGui::SameLine();
        if (ToggleButton("Rotate", gizmo.mode == GizmoMode::Rotate))
            gizmo.mode = GizmoMode::Rotate;
        ImGui::SameLine();
        if (ToggleButton("Scale", gizmo.mode == GizmoMode::Scale))
            gizmo.mode = GizmoMode::Scale;

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 2.f);
        ImGui::SameLine();

        // Space toggle
        const char* space_label = (gizmo.space == GizmoSpace::World) ? "World" : "Local";
        if (ImGui::Button(space_label))
            gizmo.space = (gizmo.space == GizmoSpace::World) ? GizmoSpace::Local : GizmoSpace::World;

        return action;
    }
}
