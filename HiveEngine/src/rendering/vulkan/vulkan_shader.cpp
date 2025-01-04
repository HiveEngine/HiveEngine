#include <rendering/RendererPlatform.h>

#ifdef HIVE_BACKEND_VULKAN_SUPPORTED
#include "vulkan_shader.h"
#include "vulkan_types.h"

#include <core/Logger.h>

#include <fstream>
#include <vector>

namespace hive::vk
{
    bool create_shader_module(const VulkanDevice& device, const char* path, VkShaderModule &out_module)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            LOG_ERROR("Vulkan: failed to open file at: '%s'", path);
            return false;
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();


        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = buffer.size();
        create_info.pCode = reinterpret_cast<const u32 *>(buffer.data());

        if(vkCreateShaderModule(device.logical_device, &create_info, nullptr, &out_module) != VK_SUCCESS)
        {
            LOG_ERROR("Vulkan: failed to create shader module");
            return false;
        }

        return true;
    }

    void destroy_shader_module(const VulkanDevice &device, VkShaderModule &module)
    {
        vkDestroyShaderModule(device.logical_device, module, nullptr);
        module = VK_NULL_HANDLE;
    }

    VkPipelineShaderStageCreateInfo create_stage_info(const VkShaderModule &shader_module, const StageType type)
    {
        VkPipelineShaderStageCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        create_info.module = shader_module;
        create_info.pName = "main";

        switch (type)
        {
            case StageType::VERTEX:
                create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
                break;
            case StageType::FRAGMENT:
                create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
        }

        return create_info;
    }
}
#endif
