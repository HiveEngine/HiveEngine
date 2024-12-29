#include "RendererFactory.h"

#include <core/Logger.h>
#include <core/Memory.h>

#include "Renderer.h"


#include <rendering/vulkan/Renderer_Vulkan.h>

hive::IRenderer * hive::RendererFactory::createRenderer(const RendererConfig &config)
{
    switch (config.type)
    {
        case RendererConfig::Type::VULKAN:
            return Memory::createObject<vk::RendererVulkan, Memory::RENDERER>(*config.window);
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

    LOG_ERROR("RendererFactory: vulkan is the only supported backend for now");
    return nullptr;
}

void hive::RendererFactory::destroyRenderer(IRenderer *renderer)
{
    Memory::destroyObject<vk::RendererVulkan, Memory::RENDERER>(renderer);
}
