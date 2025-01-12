#pragma once
#include <rendering/Renderer.h>
#include <vulkan/vulkan.h>

#include "vulkan_types.h"
#include <core/RessourceManager.h>

namespace hive
{
    struct Vertex;

}
namespace hive::vk
{
    class VkRenderer final : public IRenderer
    {
    public:

        explicit VkRenderer(const Window& window);
        ~VkRenderer() override;

        [[nodiscard]] bool isReady() const override;

        //Temporary
        void recordCommandBuffer(VkCommandBuffer command_buffer);
        void updateUniformBuffer();
        void temp_draw() override;
        void load_model();




        //API
        bool beginDrawing() override;
        bool endDrawing() override;
        bool frame() override;

        ShaderProgramHandle createShader(const char* vertex_path, const char* fragment_path, UniformBufferObjectHandle ubo, u32 mode) override;
        void destroyShader(ShaderProgramHandle shader) override;
        void useShader(ShaderProgramHandle shader) override;

        UniformBufferObjectHandle createUbo() override;
        void updateUbo(UniformBufferObjectHandle handle, const UniformBufferObject &ubo) override;
        void destroyUbo(UniformBufferObjectHandle handle) override;

    protected:
        bool is_ready_ = false;
        u32 current_frame_ = 0;
        u32 image_index_ = 0;
        

        VkInstance instance_{};
        VkSurfaceKHR surface_khr_{};
        VkDebugUtilsMessengerEXT debugMessenger{};

        VulkanDevice device_{};
        VulkanSwapchain swapchain_{};
        VkRenderPass render_pass_{};
        VulkanFramebuffer framebuffer_{};

        static constexpr u32 MAX_FRAME_IN_FLIGHT = 3;
        VkCommandBuffer command_buffers_[MAX_FRAME_IN_FLIGHT] {};

        VkSemaphore sem_image_available_[MAX_FRAME_IN_FLIGHT] {};
        VkSemaphore sem_render_finished_[MAX_FRAME_IN_FLIGHT] {};
        VkFence fence_in_flight_[MAX_FRAME_IN_FLIGHT] {};


        RessourceManager<VulkanPipeline, ShaderProgramHandle> shaders_manager_;
        RessourceManager<std::array<VulkanBuffer, MAX_FRAME_IN_FLIGHT>, UniformBufferObjectHandle> ubos_manager_;

        //Temporary
        VulkanBuffer vertex_buffer_{};
        VulkanBuffer index_buffer_{};
        VulkanImage texture_image_;

        std::vector<Vertex> vertices;
        std::vector<u32> indices;



    };
}
