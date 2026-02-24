#pragma once

#include <cstdint>

struct GLFWwindow;
namespace swarm { struct RenderContext; struct ViewportRT; }

namespace forge
{
    [[nodiscard]] bool ForgeImGuiInit(swarm::RenderContext* ctx, GLFWwindow* window);

    void ForgeImGuiShutdown(swarm::RenderContext* ctx);

    // Call once per frame, before any ImGui:: calls.
    void ForgeImGuiNewFrame();

    // Renders ImGui draw data into the current swapchain backbuffer.
    // Must be called between BeginFrame and EndFrame.
    void ForgeImGuiRender(swarm::RenderContext* ctx);

    // Register/unregister a ViewportRT for display via ImGui::Image.
    void* ForgeRegisterViewportRT(swarm::RenderContext* ctx, swarm::ViewportRT* rt);
    void ForgeUnregisterViewportRT(void* texture_id);
}
