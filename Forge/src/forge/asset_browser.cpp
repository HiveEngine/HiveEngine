#include <forge/asset_browser.h>

#include <imgui.h>

#include <filesystem>
#include <algorithm>
#include <string>
#include <vector>

namespace forge
{
    static const char* IconForExtension(const std::string& ext)
    {
        if (ext == ".gltf" || ext == ".glb" || ext == ".obj") return "[3D]";
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") return "[Tex]";
        if (ext == ".hlsl" || ext == ".glsl" || ext == ".vert" || ext == ".frag") return "[Sh]";
        if (ext == ".hscene") return "[Scene]";
        return "";
    }

    static void DrawDirectory(const std::filesystem::path& dir)
    {
        // Collect and sort entries
        std::vector<std::filesystem::directory_entry> dirs;
        std::vector<std::filesystem::directory_entry> files;

        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
        {
            if (entry.is_directory())
                dirs.push_back(entry);
            else
                files.push_back(entry);
        }

        std::sort(dirs.begin(), dirs.end());
        std::sort(files.begin(), files.end());

        // Directories
        for (const auto& d : dirs)
        {
            std::string name = d.path().filename().string();
            if (ImGui::TreeNode(name.c_str()))
            {
                DrawDirectory(d.path());
                ImGui::TreePop();
            }
        }

        // Files
        for (const auto& f : files)
        {
            std::string name = f.path().filename().string();
            std::string ext = f.path().extension().string();
            const char* icon = IconForExtension(ext);

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

            char label[256];
            if (icon[0])
                snprintf(label, sizeof(label), "%s %s", icon, name.c_str());
            else
                snprintf(label, sizeof(label), "%s", name.c_str());

            ImGui::TreeNodeEx(label, flags);

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", f.path().string().c_str());
        }
    }

    void DrawAssetBrowser(const char* assets_root)
    {
        if (!assets_root || !std::filesystem::exists(assets_root))
        {
            ImGui::TextDisabled("Assets directory not found");
            return;
        }

        DrawDirectory(std::filesystem::path{assets_root});
    }
}
