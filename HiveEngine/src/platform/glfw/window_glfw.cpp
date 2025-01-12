#define GLFW_INCLUDE_VULKAN
#include <core/Window.h>
#include <core/Logger.h>
#include <GLFW/glfw3.h>
#include "window_glfw.h"



hive::WindowGLFW::~WindowGLFW()
{
    LOG_INFO("Destroying GLFW Window");
    glfwDestroyWindow(window_);
}

hive::WindowGLFW::WindowGLFW(const WindowConfig &config)
{
    if (!glfwInit())
    {
        //TODO: error handling
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window_ = glfwCreateWindow(config.width, config.height, config.title, nullptr, nullptr);

    if (!window_)
    {
        //TODO: error handling
    }

}

u64 hive::WindowGLFW::getSizeof() const
{
    return sizeof(WindowGLFW);
}

bool hive::WindowGLFW::shouldClose() const
{
    return glfwWindowShouldClose(window_);
}

void hive::WindowGLFW::pollEvents()
{
    glfwPollEvents();
}

void hive::WindowGLFW::waitEvents() const
{
    glfwWaitEvents();
}

void hive::WindowGLFW::getFramebufferSize(i32 &width, i32 &height) const
{
    glfwGetFramebufferSize(window_, &width, &height);
}

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
void hive::WindowGLFW::appendRequiredVulkanExtension(std::vector<const char *> &vector) const
{
    u32 count;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);

    for (int i = 0; i < count; i++)
    {
        vector.emplace_back(extensions[i]);
    }
}

void hive::WindowGLFW::createVulkanSurface(void *instance, void *surface_khr) const
{
    auto vk_instance = static_cast<VkInstance>(instance);
    auto vk_surface = static_cast<VkSurfaceKHR*>(surface_khr);
    glfwCreateWindowSurface(vk_instance, window_, nullptr, vk_surface);
}




#endif
