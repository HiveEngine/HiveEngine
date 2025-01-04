#pragma once
#include <vulkan/vulkan.h>
#include "vulkan_config.h"
namespace hive::vk
{

    bool create_instance(VkInstance& instance, const Window& window);
    void destroy_instance(VkInstance& instance);
}