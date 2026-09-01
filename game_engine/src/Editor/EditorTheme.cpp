#ifdef LINUX_BUILD

#include "Editor/EditorTheme.h"

#include <imgui.h>

namespace GameEngine {

EditorTheme::Id EditorTheme::currentTheme = EditorTheme::Id::Cyberpunk;

void EditorTheme::applyViewportCorrections() {
    ImGuiStyle& style = ImGui::GetStyle();
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
}

void EditorTheme::applyCyberpunk() {
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ScrollbarSize = 13.0f;
    style.GrabMinSize = 10.0f;

    style.WindowRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;

    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;

    const ImVec4 green   = ImVec4(0.00f, 1.00f, 0.62f, 1.00f);
    const ImVec4 pink    = ImVec4(1.00f, 0.00f, 0.25f, 1.00f);
    const ImVec4 yellow  = ImVec4(1.00f, 0.93f, 0.04f, 1.00f);
    const ImVec4 bgDeep  = ImVec4(0.02f, 0.02f, 0.04f, 1.00f);
    const ImVec4 bgPanel = ImVec4(0.05f, 0.05f, 0.10f, 1.00f);

    colors[ImGuiCol_Text] = green;
    colors[ImGuiCol_TextDisabled] = ImVec4(0.20f, 0.40f, 0.35f, 1.00f);

    colors[ImGuiCol_WindowBg] = bgDeep;
    colors[ImGuiCol_ChildBg] = ImVec4(0.02f, 0.02f, 0.04f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.02f, 0.02f, 0.04f, 0.98f);

    colors[ImGuiCol_Border] = ImVec4(1.00f, 0.00f, 0.25f, 0.60f);
    colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 0.00f, 0.25f, 0.20f);

    colors[ImGuiCol_FrameBg] = bgPanel;
    colors[ImGuiCol_FrameBgHovered] = ImVec4(1.00f, 0.00f, 0.25f, 0.20f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(1.00f, 0.00f, 0.25f, 0.40f);

    colors[ImGuiCol_TitleBg] = bgDeep;
    colors[ImGuiCol_TitleBgActive] = bgPanel;
    colors[ImGuiCol_TitleBgCollapsed] = bgDeep;

    colors[ImGuiCol_MenuBarBg] = bgPanel;

    colors[ImGuiCol_ScrollbarBg] = bgDeep;
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(1.00f, 0.93f, 0.04f, 0.60f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.00f, 0.93f, 0.04f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive] = yellow;

    colors[ImGuiCol_CheckMark] = yellow;
    colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 0.00f, 0.25f, 0.80f);
    colors[ImGuiCol_SliderGrabActive] = pink;

    colors[ImGuiCol_Button] = ImVec4(0.00f, 1.00f, 0.62f, 0.20f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.00f, 1.00f, 0.62f, 0.50f);
    colors[ImGuiCol_ButtonActive] = green;

    colors[ImGuiCol_Header] = ImVec4(1.00f, 0.00f, 0.25f, 0.30f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(1.00f, 0.00f, 0.25f, 0.50f);
    colors[ImGuiCol_HeaderActive] = pink;

    colors[ImGuiCol_Tab] = bgPanel;
    colors[ImGuiCol_TabHovered] = ImVec4(1.00f, 0.00f, 0.25f, 0.80f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.80f, 0.00f, 0.20f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline] = green;
    colors[ImGuiCol_TabDimmed] = bgDeep;
    colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.35f, 0.00f, 0.10f, 1.00f);
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.00f, 1.00f, 0.62f, 0.40f);

    colors[ImGuiCol_TextSelectedBg] = ImVec4(1.00f, 0.93f, 0.04f, 0.30f);
    colors[ImGuiCol_NavCursor] = pink;

    colors[ImGuiCol_Separator] = ImVec4(1.00f, 0.00f, 0.25f, 0.40f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(1.00f, 0.00f, 0.25f, 0.70f);
    colors[ImGuiCol_SeparatorActive] = pink;

    colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 1.00f, 0.62f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.00f, 1.00f, 0.62f, 0.50f);
    colors[ImGuiCol_ResizeGripActive] = green;

    colors[ImGuiCol_DragDropTarget] = yellow;
    colors[ImGuiCol_TextLink] = yellow;
    colors[ImGuiCol_TreeLines] = ImVec4(1.00f, 0.00f, 0.25f, 0.35f);
    colors[ImGuiCol_InputTextCursor] = green;

    colors[ImGuiCol_TableHeaderBg] = bgPanel;
    colors[ImGuiCol_TableBorderStrong] = ImVec4(1.00f, 0.00f, 0.25f, 0.60f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(1.00f, 0.00f, 0.25f, 0.25f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);

    colors[ImGuiCol_PlotLines] = green;
    colors[ImGuiCol_PlotLinesHovered] = yellow;
    colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.25f, 0.80f);
    colors[ImGuiCol_PlotHistogramHovered] = yellow;

    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.02f, 0.02f, 0.04f, 0.60f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.02f, 0.02f, 0.04f, 0.60f);

#ifdef IMGUI_HAS_DOCK
    colors[ImGuiCol_DockingPreview] = ImVec4(0.00f, 1.00f, 0.62f, 0.40f);
    colors[ImGuiCol_DockingEmptyBg] = bgDeep;
#endif
}

void EditorTheme::apply(Id theme) {
    switch (theme) {
        case Id::Cyberpunk:
            applyCyberpunk();
            break;
        case Id::Light:
            ImGui::StyleColorsLight();
            break;
        case Id::Dark:
        default:
            ImGui::StyleColorsDark();
            theme = Id::Dark;
            break;
    }

    applyViewportCorrections();
    currentTheme = theme;
}

EditorTheme::Id EditorTheme::getCurrent() {
    return currentTheme;
}

const char* EditorTheme::name(Id theme) {
    switch (theme) {
        case Id::Cyberpunk: return "Cyberpunk";
        case Id::Dark:      return "Dark";
        case Id::Light:     return "Light";
        default:            return nullptr;
    }
}

}

#endif
