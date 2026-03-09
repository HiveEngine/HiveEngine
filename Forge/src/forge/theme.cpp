#include <forge/theme.h>

#include <imgui.h>

#include <cmath>
#include <cstring>

#ifdef _WIN32
#   include <cstdio>
#else
#   include <fontconfig/fontconfig.h>
#endif

namespace forge
{
#ifdef _WIN32
    static const char* FindSystemFont([[maybe_unused]] const char* family, [[maybe_unused]] bool bold)
    {
        return bold ? "C:\\Windows\\Fonts\\segoeuib.ttf" : "C:\\Windows\\Fonts\\segoeui.ttf";
    }
#else
    // Uses fontconfig to resolve a font family name to a file path (works on all distros)
    static const char* FindSystemFont(const char* family, bool bold)
    {
        static char regularPath[512];
        static char boldPath[512];
        char* dest = bold ? boldPath : regularPath;

        FcConfig* config = FcInitLoadConfigAndFonts();
        if (!config) return nullptr;

        FcPattern* pattern = FcPatternCreate();
        FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(family));
        FcPatternAddInteger(pattern, FC_WEIGHT, bold ? FC_WEIGHT_BOLD : FC_WEIGHT_REGULAR);
        FcConfigSubstitute(config, pattern, FcMatchPattern);
        FcDefaultSubstitute(pattern);

        FcResult result;
        FcPattern* match = FcFontMatch(config, pattern, &result);
        const char* found = nullptr;
        if (match)
        {
            FcChar8* file = nullptr;
            if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch && file)
            {
                std::strncpy(dest, reinterpret_cast<const char*>(file), 511);
                dest[511] = '\0';
                found = dest;
            }
            FcPatternDestroy(match);
        }
        FcPatternDestroy(pattern);
        FcConfigDestroy(config);
        return found;
    }
#endif

    // Swapchain is B8G8R8A8_UNORM_SRGB - GPU applies sRGB encoding on output.
    // ImGui colors are specified in sRGB, but the GPU gamma-corrects them again.
    // Convert sRGB → linear so the final output matches intended sRGB values.
    static float SrgbToLinear(float s)
    {
        return s <= 0.04045f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
    }

    static ImVec4 L(float r, float g, float b, float a)
    {
        return {SrgbToLinear(r), SrgbToLinear(g), SrgbToLinear(b), a};
    }

    void ApplyForgeTheme()
    {
        ImGuiIO& io = ImGui::GetIO();

        // Font - Segoe UI on Windows, fontconfig-resolved sans-serif on Linux
        const char* regular = FindSystemFont("sans-serif", false);
        if (regular && io.Fonts->AddFontFromFileTTF(regular, 15.f))
        {
            const char* bold = FindSystemFont("sans-serif", true);
            if (bold)
                io.Fonts->AddFontFromFileTTF(bold, 15.f);
        }

        ImGuiStyle& style = ImGui::GetStyle();

        // Rounding - subtle
        style.WindowRounding = 3.f;
        style.FrameRounding = 3.f;
        style.GrabRounding = 2.f;
        style.TabRounding = 3.f;
        style.ChildRounding = 0.f;
        style.PopupRounding = 3.f;
        style.ScrollbarRounding = 6.f;

        // Sizing - more generous
        style.WindowPadding = {10.f, 10.f};
        style.FramePadding = {8.f, 4.f};
        style.ItemSpacing = {8.f, 5.f};
        style.ItemInnerSpacing = {5.f, 4.f};
        style.IndentSpacing = 18.f;
        style.ScrollbarSize = 11.f;
        style.GrabMinSize = 8.f;
        style.TabBarBorderSize = 1.f;

        // Borders
        style.WindowBorderSize = 1.f;
        style.ChildBorderSize = 0.f;
        style.FrameBorderSize = 0.f;
        style.TabBorderSize = 0.f;
        style.PopupBorderSize = 1.f;

        // All color values are sRGB, converted to linear via L()
        auto* c = style.Colors;

        // Window - #141414 bg
        c[ImGuiCol_WindowBg] = L(0.078f, 0.078f, 0.078f, 1.f);
        c[ImGuiCol_ChildBg] = L(0.098f, 0.098f, 0.098f, 1.f);
        c[ImGuiCol_PopupBg] = L(0.090f, 0.090f, 0.090f, 0.97f);

        // Borders - #2e2e2e
        c[ImGuiCol_Border] = L(0.180f, 0.180f, 0.180f, 0.50f);
        c[ImGuiCol_BorderShadow] = {0.f, 0.f, 0.f, 0.f};

        // Title bar - #0f0f0f
        c[ImGuiCol_TitleBg] = L(0.060f, 0.060f, 0.060f, 1.f);
        c[ImGuiCol_TitleBgActive] = L(0.078f, 0.078f, 0.078f, 1.f);
        c[ImGuiCol_TitleBgCollapsed] = L(0.060f, 0.060f, 0.060f, 0.7f);

        // Menu bar - #141414
        c[ImGuiCol_MenuBarBg] = L(0.078f, 0.078f, 0.078f, 1.f);

        // Scrollbar
        c[ImGuiCol_ScrollbarBg] = L(0.060f, 0.060f, 0.060f, 0.5f);
        c[ImGuiCol_ScrollbarGrab] = L(0.220f, 0.220f, 0.220f, 1.f);
        c[ImGuiCol_ScrollbarGrabHovered] = L(0.310f, 0.310f, 0.310f, 1.f);
        c[ImGuiCol_ScrollbarGrabActive] = L(0.400f, 0.400f, 0.400f, 1.f);

        // Frame (inputs) - #1f1f1f
        c[ImGuiCol_FrameBg] = L(0.120f, 0.120f, 0.120f, 1.f);
        c[ImGuiCol_FrameBgHovered] = L(0.170f, 0.170f, 0.170f, 1.f);
        c[ImGuiCol_FrameBgActive] = L(0.100f, 0.250f, 0.400f, 1.f);

        // Buttons - #262626
        c[ImGuiCol_Button] = L(0.150f, 0.150f, 0.150f, 1.f);
        c[ImGuiCol_ButtonHovered] = L(0.000f, 0.400f, 0.720f, 0.85f);
        c[ImGuiCol_ButtonActive] = L(0.000f, 0.471f, 0.831f, 1.f);

        // Header (tree nodes, selectable)
        c[ImGuiCol_Header] = L(0.120f, 0.120f, 0.120f, 0.8f);
        c[ImGuiCol_HeaderHovered] = L(0.000f, 0.400f, 0.720f, 0.45f);
        c[ImGuiCol_HeaderActive] = L(0.000f, 0.471f, 0.831f, 0.65f);

        // Separator
        c[ImGuiCol_Separator] = L(0.180f, 0.180f, 0.180f, 0.5f);
        c[ImGuiCol_SeparatorHovered] = L(0.000f, 0.400f, 0.720f, 0.8f);
        c[ImGuiCol_SeparatorActive] = L(0.000f, 0.471f, 0.831f, 1.f);

        // Resize grip
        c[ImGuiCol_ResizeGrip] = L(0.180f, 0.180f, 0.180f, 0.3f);
        c[ImGuiCol_ResizeGripHovered] = L(0.000f, 0.400f, 0.720f, 0.6f);
        c[ImGuiCol_ResizeGripActive] = L(0.000f, 0.471f, 0.831f, 0.9f);

        // Tabs
        c[ImGuiCol_Tab] = L(0.060f, 0.060f, 0.060f, 1.f);
        c[ImGuiCol_TabHovered] = L(0.000f, 0.400f, 0.720f, 0.45f);
        c[ImGuiCol_TabSelected] = L(0.078f, 0.078f, 0.078f, 1.f);
        c[ImGuiCol_TabDimmed] = L(0.050f, 0.050f, 0.050f, 1.f);
        c[ImGuiCol_TabDimmedSelected] = L(0.078f, 0.078f, 0.078f, 1.f);

        // Docking
        c[ImGuiCol_DockingPreview] = L(0.000f, 0.471f, 0.831f, 0.5f);
        c[ImGuiCol_DockingEmptyBg] = L(0.040f, 0.040f, 0.040f, 1.f);

        // Check mark / Slider - blue accent
        c[ImGuiCol_CheckMark] = L(0.102f, 0.549f, 1.000f, 1.f);
        c[ImGuiCol_SliderGrab] = L(0.000f, 0.471f, 0.831f, 0.85f);
        c[ImGuiCol_SliderGrabActive] = L(0.102f, 0.549f, 1.000f, 1.f);

        // Text - #cccccc
        c[ImGuiCol_Text] = L(0.800f, 0.800f, 0.800f, 1.f);
        c[ImGuiCol_TextDisabled] = L(0.502f, 0.502f, 0.502f, 1.f);

        // Plot
        c[ImGuiCol_PlotLines] = L(0.000f, 0.471f, 0.831f, 1.f);
        c[ImGuiCol_PlotLinesHovered] = L(0.102f, 0.549f, 1.000f, 1.f);
        c[ImGuiCol_PlotHistogram] = L(0.000f, 0.471f, 0.831f, 1.f);
        c[ImGuiCol_PlotHistogramHovered] = L(0.102f, 0.549f, 1.000f, 1.f);

        // Table
        c[ImGuiCol_TableHeaderBg] = L(0.098f, 0.098f, 0.098f, 1.f);
        c[ImGuiCol_TableBorderStrong] = L(0.180f, 0.180f, 0.180f, 0.8f);
        c[ImGuiCol_TableBorderLight] = L(0.120f, 0.120f, 0.120f, 0.5f);
        c[ImGuiCol_TableRowBg] = {0.f, 0.f, 0.f, 0.f};
        c[ImGuiCol_TableRowBgAlt] = {0.01f, 0.01f, 0.01f, 0.3f};

        // Nav
        c[ImGuiCol_NavHighlight] = L(0.000f, 0.471f, 0.831f, 0.8f);

        // Modal dim
        c[ImGuiCol_ModalWindowDimBg] = {0.f, 0.f, 0.f, 0.55f};

        // Text selection - #264f78
        c[ImGuiCol_TextSelectedBg] = L(0.149f, 0.310f, 0.471f, 0.60f);

        // Drag-drop
        c[ImGuiCol_DragDropTarget] = L(0.102f, 0.549f, 1.000f, 0.9f);
    }
} // namespace forge
