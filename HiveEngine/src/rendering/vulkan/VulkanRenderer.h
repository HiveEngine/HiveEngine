#pragma once
#include <vulkan/vulkan_core.h>
#include <vector>
#include <optional>
#include <rendering/Renderer.h>

#include "vulkan_device.h"
#include "vulkan_framebuffer.h"
#include "vulkan_renderpass.h"
#include "vulkan_swapchain.h"
#include <core/RessourceManager.h>

#include "vulkan_shader.h"


namespace hive
{
    class Window;
}

namespace hive::vk
{

    class VulkanRenderer : public IRenderer
    {
    public:
        explicit VulkanRenderer(const Window& window);
        ~VulkanRenderer();


        [[nodiscard]] bool isReady() const override;

        void temp_draw() override;


        bool beginDrawing() override;

        void endDrawing() override;

        void frame() override;

        ShaderProgramHandle createProgram(const char *vertex_path, const char *fragment_path) override;

        void destroyProgram(ShaderProgramHandle shader) override;

        void useProgram(ShaderProgramHandle shader) override;
    private:
        bool createInstance(const Window& window, const DeviceConfig &config);
        bool setupDebugMessenger();
        bool createSurface(const Window& window);
        bool createDevice();
        bool createSwapChain(const Window& window);
        bool createRenderPass();
        bool createFramebuffers();
        bool createCommandPool();
        bool createCommandBuffer();
        bool createSyncObjects();

        void recreateSwapChain();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);

    public:

    protected:
        bool is_ready_ = false;
        u32 imageIndex = 0;
        u32 current_frame = 0;
        VkInstance instance_{};
        VkSurfaceKHR surface_{};
        VkDebugUtilsMessengerEXT debugMessenger{};
        Device device_{};
        Swapchain swapchain_;
        RenderPass render_pass_;
        Framebuffer framebuffer_;



        static constexpr i32 MAX_FRAME_IN_FLIGHT = 2;
        VkCommandBuffer command_buffers_[MAX_FRAME_IN_FLIGHT];
        // VkCommandBuffer command_buffer_{};

        VkSemaphore image_available_semaphore_[MAX_FRAME_IN_FLIGHT], render_finished_semaphore_[MAX_FRAME_IN_FLIGHT];
        VkFence in_flight_fence_[MAX_FRAME_IN_FLIGHT];


        RessourceManager<Shader> shaders_manager_;

        const Window& window_;

    };
}
