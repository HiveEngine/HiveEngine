#pragma once
#include <rendering/Renderer.h>
#include <vulkan/vulkan.h>

#include "vulkan_types.h"
#include <core/RessourceManager.h>

namespace hive::vk
{
    class VkRenderer : public IRenderer
    {
    public:
        explicit VkRenderer(const Window& window);
        ~VkRenderer() override;

        [[nodiscard]] bool isReady() const override;

        //Temporary
        void recordCommandBuffer(VkCommandBuffer command_buffer);
        void updateUniformBuffer();
        void temp_draw() override;



        //API
        bool beginDrawing() override;

        bool endDrawing() override;

        bool frame() override;

        ShaderProgramHandle createProgram(const char *vertex_path, const char *fragment_path) override;

        void destroyProgram(ShaderProgramHandle shader) override;

        void useProgram(ShaderProgramHandle shader) override;

        void createShaderLayout() override;

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

        static constexpr u32 MAX_FRAME_IN_FLIGHT = 2;
        VkCommandBuffer command_buffers_[MAX_FRAME_IN_FLIGHT] {};

        VkSemaphore sem_image_available_[MAX_FRAME_IN_FLIGHT] {};
        VkSemaphore sem_render_finished_[MAX_FRAME_IN_FLIGHT] {};
        VkFence fence_in_flight_[MAX_FRAME_IN_FLIGHT] {};


        RessourceManager<VulkanPipeline, ShaderProgramHandle> shaders;

        //Temporary
        VulkanPipeline default_pipeline_{};
        VulkanBuffer vertex_buffer_{};
        VulkanBuffer index_buffer_{};
        std::vector<VkDescriptorSet> descriptorSets;
        VkDescriptorPool descriptor_pool;
        VulkanBuffer ubos[MAX_FRAME_IN_FLIGHT]{};



    };
}
