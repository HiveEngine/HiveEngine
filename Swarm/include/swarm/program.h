#pragma once
#include <swarm/renderpass.h>

namespace swarm
{
    class Device;
    class Shader;
    class RenderPass;


    //For now we assume the layout of a program is the following:
    //binding 0: ubo for vertex shader
    //binding 1: sampler for fragment shader

    class Program
    {
    public:
        explicit Program(Device& device, Shader& vertexShader, Shader& fragmentShader, RenderPass &renderpass);
        ~Program();

    protected:
        Device& m_Device;

        Shader& m_VertexShader;
        Shader& m_FragmentShader;
        RenderPass& m_RenderPass;

    private:
        #include <swarm/programbackend.inl>
    };
}
