#pragma once
#include <vulkan/vulkan_core.h>
#include <vector>
#include <optional>
namespace hive
{
    class Window;
}

struct QueueFamilyIndices
{
    std::optional<u32> graphicsFamily;
    std::optional<u32> presentFamily;

    [[nodiscard]] bool isComplete() const
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};
namespace hive::vk
{

    class VulkanRenderer
    {
    public:
        VulkanRenderer(const Window& window);


        void drawFrame();
    private:

        bool createInstance(const Window& window);
        bool setupDebugMessenger();
        bool createSurface(const Window& window);
        bool pickPhysicalDevice();
        bool createLogicalDevice();
        bool createSwapChain(const Window& window);
        bool createImageViews();
        bool createRenderPass();
        bool createGraphicsPipeline();
        bool createFramebuffers();
        bool createCommandPool();
        bool createCommandBuffer();
        bool createSyncObjects();

        bool checkValidationLayerSupport();
        std::vector<const char*>  getRequiredExtensions(const Window& window);
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t uint32);


        bool isDeviceSuitable(VkPhysicalDevice device);


    protected:
        bool enableValidationLayers;
        VkInstance instance_;
        VkDebugUtilsMessengerEXT debugMessenger;
        VkSurfaceKHR surface_;
        VkPhysicalDevice physical_device_;
        VkDevice device_;
        VkQueue graphics_queue_, present_queue_;
        VkSwapchainKHR swapchain_khr_;
        std::vector<VkImage> swap_chain_images_;
        VkFormat swap_chain_image_format_;
        VkExtent2D swap_chain_extent_;
        std::vector<VkImageView> swap_chain_image_views_;
        std::vector<VkFramebuffer> swap_chain_framebuffers_;
        VkRenderPass render_pass_;

        VkCommandPool command_pool_;
        VkCommandBuffer command_buffer_;

        VkSemaphore image_available_semaphore_, render_finished_semaphore_;
        VkFence in_flight_fence_;

        VkPipelineLayout pipeline_layout_;
        VkPipeline graphics_pipeline_;
    };
}
