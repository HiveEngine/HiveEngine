#pragma once
#include "RenderType.h"

namespace hive
{
    class Window;

    struct RendererConfig
    {
        enum class Type
        {
            VULKAN, OPENGL, DIRECTX, NONE
        };
        Type type;
        Window* window;
    };

    class IRenderer
    {
    public:
        virtual ~IRenderer() = default;

        virtual void beginDrawing() = 0;
        virtual void endDrawing() = 0;
        virtual void frame() = 0;

        virtual ShaderProgramHandle createProgram() = 0;
        virtual void destroyProgram(ShaderProgramHandle shader) = 0;
        virtual void useProgram(ShaderProgramHandle shader) = 0;
    };

}