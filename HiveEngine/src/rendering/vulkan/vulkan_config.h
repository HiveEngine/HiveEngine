#pragma once
#include <hvpch.h>
#include <vector>
namespace hive::vk::config
{
    constexpr bool enable_validation = true;

    const std::vector<const char*> required_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    const std::vector<const char*> validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };


}