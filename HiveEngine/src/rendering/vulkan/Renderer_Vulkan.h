#pragma once
#include <rendering/Renderer.h>
#include <vulkan/vulkan.h>

namespace hive::vk
{
    class RendererVulkan : public IRenderer
    {
    public:
        RendererVulkan(const Window& window);
        ~RendererVulkan() override;

        void beginDrawing() override;
        void endDrawing() override;

        void frame() override;

        ShaderProgramHandle createProgram() override;
        void destroyProgram(ShaderProgramHandle shader) override;
        void useProgram(ShaderProgramHandle shader) override;

    private:
        void createInstance(const Window& window);

    protected:
        VkInstance instance_{};
    };
}
