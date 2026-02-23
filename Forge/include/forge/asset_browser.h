#pragma once

namespace forge
{
    // Draw the asset browser panel.
    // Must be called between ImGui::Begin("Asset Browser") and ImGui::End().
    // assets_root: path to the assets directory on disk.
    void DrawAssetBrowser(const char* assets_root);
}
