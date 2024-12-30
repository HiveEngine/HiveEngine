#pragma once
#include <core/Window.h>
#include <GLFW/glfw3.h>

namespace hive
{

    class WindowGLFW final : public IWindow
    {
    public:
        ~WindowGLFW() override;
        WindowGLFW(const WindowConfig &config);



        [[nodiscard]] u64 getSizeof() const override;

        [[nodiscard]] bool shouldClose() const override;

        void pollEvents() override;

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
        void appendRequiredVulkanExtension(std::vector<const char*> &vector) const override;
        void createVulkanSurface(VkInstance& instance, VkSurfaceKHR *surface_khr) const override;
#endif


    private:
        GLFWwindow* window_;
    };
}
