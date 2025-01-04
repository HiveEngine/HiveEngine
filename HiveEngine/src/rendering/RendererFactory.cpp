#include "RendererFactory.h"

#include <core/Logger.h>
#include <core/Memory.h>

#include "Renderer.h"


// #include <rendering/vulkan/VulkanRenderer.h>
#include <rendering/vulkan/VkRenderer.h>

bool hive::RendererFactory::createRenderer(const RendererConfig &config, IRenderer** out_renderer)
{
    switch (config.type)
    {
        case RendererConfig::Type::VULKAN:
        {
            auto vulkan_renderer = Memory::createObject<vk::VkRenderer, Memory::RENDERER>(*config.window);
            if (vulkan_renderer->isReady())
            {
                *out_renderer = vulkan_renderer;
                return true;
            }
            LOG_ERROR("Failed to create Vulkan renderer");
        }
        case RendererConfig::Type::OPENGL:
            //TODO: error not supported yet
            break;
        case RendererConfig::Type::DIRECTX:
            //TODO: error not supported yet
            break;
        case RendererConfig::Type::NONE:
            //TODO: error not supported yet
            break;
    }

    return false;
}

void hive::RendererFactory::destroyRenderer(IRenderer *renderer)
{
    Memory::destroyObject<vk::VkRenderer, Memory::RENDERER>(renderer);
}
