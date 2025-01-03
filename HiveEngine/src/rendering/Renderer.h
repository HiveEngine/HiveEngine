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

        [[nodiscard]] virtual bool isReady() const = 0;

        virtual void temp_draw() = 0;
        virtual bool beginDrawing() = 0;
        virtual void endDrawing() = 0;
        virtual void frame() = 0;

        virtual ShaderProgramHandle createProgram(const char* vertex_path, const char* fragment_path) = 0;
        virtual void destroyProgram(ShaderProgramHandle shader) = 0;
        virtual void useProgram(ShaderProgramHandle shader) = 0;


        // virtual VertexBufferHandle createVertexBuffer();
        // virtual void destroyVertexBuffer(VertexBufferHandle buffer);
        // virtual void useVertexBuffer(VertexBufferHandle buffer);
    };

}