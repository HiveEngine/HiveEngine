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
        void waitEvents() const  override;
        void getFramebufferSize(i32 &width, i32 &height) const override;

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
        void appendRequiredVulkanExtension(std::vector<const char*> &vector) const override;
        void createVulkanSurface(void *instance, void *surface_khr) const override;


#endif


    private:
        GLFWwindow* window_;
    };
}
