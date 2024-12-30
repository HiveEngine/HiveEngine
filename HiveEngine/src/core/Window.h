#pragma once
#include <hvpch.h>
#include <vector>

#include <rendering/RendererPlatform.h>
#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include <vulkan/vulkan_core.h>
#endif

namespace hive
{
    struct WindowConfig
    {
        enum class WindowType
        {
            Native, GLFW, Raylib, NONE
        };

        WindowType type;
        u16 width, height;
        const char* title;

    };

    class IWindow
    {
    public:
        virtual ~IWindow()= default;
        [[nodiscard]] virtual u64 getSizeof() const = 0;
        [[nodiscard]] virtual bool shouldClose() const = 0;
        virtual void pollEvents() = 0;

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
        virtual void appendRequiredVulkanExtension(std::vector<const char*> &vector) const = 0;
        virtual void createVulkanSurface(VkInstance& instance, VkSurfaceKHR *surface_khr) const = 0;
#endif
    };


    class Window
    {
    public:
        explicit Window(const WindowConfig &config);
        ~Window();

        [[nodiscard]] bool shouldClose() const;
        void pollEvents();

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
        void appendRequiredVulkanExtension(std::vector<const char*> &vector) const;
        void createVulkanSurface(VkInstance& instance, VkSurfaceKHR *surface_khr) const;
#endif
    private:
        IWindow* window_handle{};
    };
}
