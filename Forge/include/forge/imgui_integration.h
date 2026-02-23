#pragma once

#include <swarm/types.h>
#include <cstdint>

struct GLFWwindow;
namespace swarm { struct Device; struct CommandBuffer; struct Swapchain; }

namespace forge
{
    [[nodiscard]] bool ForgeImGuiInit(swarm::Device& device, swarm::Swapchain& swapchain, GLFWwindow* window);

    void ForgeImGuiShutdown(swarm::Device& device);

    // Call once per frame, before any ImGui:: calls.
    void ForgeImGuiNewFrame();

    // Backbuffer must already be in RenderTarget layout.
    // Begins its own rendering pass (LoadOp::Load), records, then ends it.
    void ForgeImGuiRender(swarm::CommandBuffer& cmd, swarm::Device& device,
                          swarm::TextureHandle backbuffer, uint32_t width, uint32_t height);

    // Texture must have ShaderResource usage.
    [[nodiscard]] uint64_t ForgeRegisterTexture(swarm::Device& device, swarm::TextureHandle texture);

    // Call before destroying the texture.
    void ForgeUnregisterTexture(uint64_t texture_id);
}
