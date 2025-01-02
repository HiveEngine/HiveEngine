#pragma once
#include <rendering/Renderer.h>
#include <vulkan/vulkan.h>
#include "vulkan_types.h"
#include <core/RessourceManager.h>
namespace hive::vk
{
    class RendererVulkan : public IRenderer
    {
    public:

        explicit RendererVulkan(const Window& window);
        ~RendererVulkan() override;

        [[nodiscard]] bool isReady() const override;

        void beginDrawing() override;
        void endDrawing() override;

        void frame() override;

        ShaderProgramHandle createProgram(const char *vertex_path, const char *fragment_path) override;
        void destroyProgram(ShaderProgramHandle shader) override;
        void useProgram(ShaderProgramHandle shader) override;

    private:
        //TODO move some of those function into other file (they are simple function are not closely related to the renderer)
        bool createInstance(const Window& window);
        bool createSurface(const Window& window);
        bool pickPhysicalDevice();
        bool createLogicalDevice();
        bool createSwapChain(const Window& window);
        bool createImageView();
        bool createRenderPass();
        bool createFramebuffer();
        bool createCommandPool();
        bool createCommandBuffer();
        bool createSyncObject();


    protected:
        VkInstance instance_{};
        VkSurfaceKHR surface_{};
        VkRenderPass render_pass_{};
        VulkanDevice device_{};
        VulkanSwapchain swapchain_{};
        VkCommandPool command_pool_{};
        VkCommandBuffer command_buffer_{};

        VkSemaphore image_available_semaphore_;
        VkSemaphore render_finished_semaphore_;
        VkFence in_flight_fence_;

        RessourceManager<VulkanShader> shaders_;


        std::vector<VkFramebuffer> framebuffers_;

        bool is_ready_ = false;

    };


}
