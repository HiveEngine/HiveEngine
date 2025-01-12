#pragma once
#include <hvpch.h>
#include <vector>

#include <rendering/RendererPlatform.h>

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
        virtual void waitEvents() const = 0;
        virtual void getFramebufferSize(i32& width, i32 &height) const = 0;


#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
        virtual void appendRequiredVulkanExtension(std::vector<const char*> &vector) const = 0;
        virtual void createVulkanSurface(void* instance, void *surface_khr) const = 0;
#endif
    };


    class Window
    {
    public:
        explicit Window(const WindowConfig &config);
        ~Window();

        [[nodiscard]] bool shouldClose() const;
        void pollEvents();
        void waitEvents() const ;

        void getFramebufferSize(i32& width, i32 &height) const;

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
        void appendRequiredVulkanExtension(std::vector<const char*> &vector) const;
        void createVulkanSurface(void* instance, void *surface_khr) const;
#endif
    private:
        IWindow* window_handle{};
    };
}
