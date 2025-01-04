#include <rendering/RendererPlatform.h>

#include "vulkan_debug.h"
#include "vulkan_utils.h"
#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include <core/Window.h>
#include <core/Logger.h>

#include "vulkan_init.h"

namespace hive::vk
{
    void get_required_extensions(VkInstanceCreateInfo& create_info, const Window& window);
    bool create_instance(VkInstance& instance, const Window& window)
    {
        if (config::enable_validation && !check_validation_layer_support(config::validation_layers))
        {
            LOG_ERROR("Validation layers requrested but are not available");
            return false;
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;


        std::vector<const char *> extensions;
        window.appendRequiredVulkanExtension(extensions);

        VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info{};
        if (config::enable_validation)
        {
            createInfo.enabledLayerCount = config::validation_layers.size();
            createInfo.ppEnabledLayerNames = config::validation_layers.data();
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

            debug_messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debug_messenger_create_info.messageSeverity =
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debug_messenger_create_info.messageType =
                    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debug_messenger_create_info.pfnUserCallback = debug_messenger_callback;

            createInfo.pNext = &debug_messenger_create_info;
        } else
        {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            LOG_ERROR("Vulkan: failed to create instance!");
            return false;
        }


        return true;
    }

    void destroy_instance(VkInstance&instance)
    {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;

    }


    void get_required_extensions(VkInstanceCreateInfo& create_info, const Window& window)
    {
        std::vector<const char*> extensions;
        window.appendRequiredVulkanExtension(extensions);

        VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info{};
        if (config::enable_validation)
        {
            create_info.enabledLayerCount = config::validation_layers.size();
            create_info.ppEnabledLayerNames = config::validation_layers.data();
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

            debug_messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debug_messenger_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debug_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debug_messenger_create_info.pfnUserCallback = debug_messenger_callback;

            create_info.pNext = &debug_messenger_create_info;
        } else
        {
            create_info.enabledLayerCount = 0;

            create_info.pNext = nullptr;
        }

        create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        create_info.ppEnabledExtensionNames = extensions.data();

    }
}
#endif
