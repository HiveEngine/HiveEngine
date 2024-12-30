#pragma once
#include <rendering/Renderer.h>
#include <vulkan/vulkan.h>

namespace hive::vk
{
    struct VulkanDevice
    {
        VkPhysicalDevice physicalDevice_{};
        VkDevice device_{};

        i32 graphics_queue_index_{};
        i32 transfer_queue_index_{};
        i32 present_queue_index_{};

        VkQueue graphicsQueue_{};
    };

    struct PhysicalDeviceFamilyQueueInfo
    {
        i32 graphics_family_index;
        i32 present_family_index;
        i32 compute_family_index;
        i32 transfer_family_index;
    };

    class RendererVulkan : public IRenderer
    {
    public:

        explicit RendererVulkan(const Window& window);
        ~RendererVulkan() override;

        [[nodiscard]] bool isReady() const override;

        void beginDrawing() override;
        void endDrawing() override;

        void frame() override;

        ShaderProgramHandle createProgram() override;
        void destroyProgram(ShaderProgramHandle shader) override;
        void useProgram(ShaderProgramHandle shader) override;

    private:
        //TODO move some of those function into other file (they are simple function are not closely related to the renderer)
        bool createInstance(const Window& window);
        bool createSurface(const Window& window);
        bool pickPhysicalDevice();
        bool createLogicalDevice();
        bool createSwapChain();

        //Helper function
        bool isDeviceSuitable(const VkPhysicalDevice& device, const PhysicalDeviceRequirements& requirements, PhysicalDeviceFamilyQueueInfo& out_familyQueueInfo) const;


    protected:
        VkInstance instance_{};
        VkSurfaceKHR surface_{};
        VulkanDevice device_{};

        bool is_ready_ = false;

    };


}
