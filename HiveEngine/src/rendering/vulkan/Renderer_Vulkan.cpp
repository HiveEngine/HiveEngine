
#include "rendering/RendererPlatform.h"


#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include "vulkan/vulkan.h"
#include <vector>

#include "Renderer_Vulkan.h"
#include <core/Logger.h>

hive::vk::RendererVulkan::RendererVulkan(const Window &window)
{
    LOG_INFO("Initializing Vulkan renderer");
    createInstance(window);
}

hive::vk::RendererVulkan::~RendererVulkan()
{
    LOG_INFO("Shutting down Vulkan renderer");
    vkDestroyInstance(instance_, nullptr);
}

void hive::vk::RendererVulkan::beginDrawing()
{
}

void hive::vk::RendererVulkan::endDrawing()
{
}

void hive::vk::RendererVulkan::frame()
{
}

hive::ShaderProgramHandle hive::vk::RendererVulkan::createProgram()
{
    return {0};
}

void hive::vk::RendererVulkan::destroyProgram(ShaderProgramHandle shader)
{
}

void hive::vk::RendererVulkan::useProgram(ShaderProgramHandle shader)
{
}

void hive::vk::RendererVulkan::createInstance(const Window& window)
{
    VkApplicationInfo application_info{};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = "Hive";
    application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.pEngineName = "Hive";
    application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &application_info;

    std::vector<char*> extensions;

    VkResult result = vkCreateInstance(&create_info, nullptr, &instance_);
    if(result != VK_SUCCESS)
    {
        //TODO: handle error
    }

}

#endif
