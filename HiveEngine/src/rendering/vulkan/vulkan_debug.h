#pragma once
#include <vulkan/vulkan.h>
#include <iostream>
namespace hive::vk
{
    VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                            VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                            const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                            void *pUserData);

    bool setup_debug_messenger(const VkInstance& instance, VkDebugUtilsMessengerEXT &debugMessenger);


    VkResult VKCreateDebugUtilsMessengerEXT(VkInstance instance,
                                            const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator,
                                            VkDebugUtilsMessengerEXT *pDebugMessenger);

    void destroy_debug_util_mesenger(const VkInstance& instance, VkDebugUtilsMessengerEXT& debug_messenger);
}