#include "core/Window.h"
#include "core/Memory.h"

#include <platform/Platform.h>
#include <platform/glfw/window_glfw.h>

#include "core/Logger.h"


hive::Window::Window(const WindowConfig &config) : window_handle(nullptr)
{
    LOG_INFO("Creating the Window");
    switch (config.type)
    {
        case WindowConfig::WindowType::Native:
            //not supported yet
            break;
        case WindowConfig::WindowType::GLFW:
        {
            auto window_glfw = Memory::createObject<WindowGLFW, Memory::ENGINE>(config);

            window_handle = window_glfw;
            break;
        }

        case WindowConfig::WindowType::Raylib:
            //not supported yet
            break;
        case WindowConfig::WindowType::NONE:
            break;
    }
}

hive::Window::~Window()
{
    //TODO: handle multiple type of window
    Memory::destroyObject<WindowGLFW, Memory::ENGINE>(window_handle);
}

bool hive::Window::shouldClose() const
{
    return window_handle->shouldClose();
}

void hive::Window::pollEvents()
{
    window_handle->pollEvents();
}

void hive::Window::waitEvents() const
{
    window_handle->waitEvents();
}

void hive::Window::getFramebufferSize(i32 &width, i32 &height) const
{
   window_handle->getFramebufferSize(width, height);
}

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include <vulkan/vulkan.h>
void hive::Window::appendRequiredVulkanExtension(std::vector<const char *> &vector) const
{
    window_handle->appendRequiredVulkanExtension(vector);
}

void hive::Window::createVulkanSurface(void* instance, void* surface_khr) const
{
    window_handle->createVulkanSurface(instance, surface_khr);
}


#endif
