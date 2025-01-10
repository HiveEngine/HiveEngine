#pragma once
#include <hvpch.h>
#include <glm/glm.hpp>

#define DEFINE_HANDLE(_name) struct _name##Handle {u32 id; }

namespace hive
{

    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;
    };

    struct UniformBufferObject
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    DEFINE_HANDLE(ShaderProgram);
    DEFINE_HANDLE(VertexBuffer);
    DEFINE_HANDLE(UniformBufferObject);
}