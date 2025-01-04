#pragma once
#include <rendering/Renderer.h>
#include <vulkan/vulkan.h>

#include "vulkan_types.h"

namespace hive::vk
{
    class VkRenderer : public IRenderer
    {
    public:
        explicit VkRenderer(const Window& window);
        ~VkRenderer();

        [[nodiscard]] bool isReady() const override;

        void recordCommandBuffer(VkCommandBuffer command_buffer, uint32_t uint32);

        void temp_draw() override;

        bool beginDrawing() override;

        void endDrawing() override;

        void frame() override;

        ::hive::ShaderProgramHandle createProgram(const char *vertex_path, const char *fragment_path) override;

        void destroyProgram(::hive::ShaderProgramHandle shader) override;

        void useProgram(::hive::ShaderProgramHandle shader) override;

        void createShaderLayout() override;

    protected:
        bool is_ready_ = false;

        u32 current_frame = 0;
        

        VkInstance instance_{};
        VkSurfaceKHR surface_khr_{};
        VkDebugUtilsMessengerEXT debugMessenger{};

        VulkanDevice device_{};
        VulkanSwapchain swapchain_{};
        VkRenderPass render_pass_{};
        VulkanFramebuffer framebuffer_{};

        static constexpr u32 MAX_FRAME_IN_FLIGHT = 2;
        VkCommandBuffer command_buffers_[MAX_FRAME_IN_FLIGHT] {};

        VkSemaphore sem_image_available_[MAX_FRAME_IN_FLIGHT] {};
        VkSemaphore sem_render_finished_[MAX_FRAME_IN_FLIGHT] {};
        VkFence fence_in_flight_[MAX_FRAME_IN_FLIGHT] {};


        VulkanPipeline default_pipeline_{};


    };
}
