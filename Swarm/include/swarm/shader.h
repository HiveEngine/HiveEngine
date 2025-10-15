#pragma once

#include <filesystem>
namespace swarm
{
    class Device;

    enum class ShaderStage
    {
        VERTEX, FRAGMENT
    };

    struct ShaderDescription
    {
        ShaderStage stage;
    };

    class Shader
    {
    public:
        explicit Shader(Device& device, const std::filesystem::path& path, ShaderDescription description);
        ~Shader();

        ShaderStage GetState() const { return m_Stage; }

    protected:
        Device& m_Device;
        ShaderStage m_Stage;
    private:
        #include <swarm/shaderbackend.inl>
    };
}