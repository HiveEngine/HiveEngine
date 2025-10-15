#include <swarm/precomp.h>
#include <swarm/shader.h>
#include <swarm/device.h>

#include <fstream>

namespace swarm
{
    constexpr const char *g_VulkanShaderFileExtension = ".spv";

    Shader::Shader(Device &device, const std::filesystem::path& path,
                   ShaderDescription description) : m_Stage(description.stage), m_Device(device), m_ShaderModule(VK_NULL_HANDLE)
    {
        //TODO make this more modular with different function
        if (path.extension() != g_VulkanShaderFileExtension)
        {
            //Log error
            return;
        }

        //TODO read file from path
        std::ifstream file(path);

        if (!file.is_open())
        {
            return;
        }

        const unsigned int fileSize = std::filesystem::file_size(path);
        std::vector<char> buffer(fileSize);
        file.read(buffer.data(), fileSize);
        file.close();

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = fileSize;
        createInfo.pCode = reinterpret_cast<unsigned int*>(buffer.data());

        vkCreateShaderModule(m_Device, &createInfo, nullptr, &m_ShaderModule);
    }

    Shader::~Shader()
    {
        if (m_ShaderModule != VK_NULL_HANDLE)
            vkDestroyShaderModule(m_Device, m_ShaderModule, nullptr);
    }
}
