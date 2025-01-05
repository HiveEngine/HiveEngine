#pragma once
#include <hvpch.h>

#define DEFINE_HANDLE(_name) struct _name##Handle {u32 id; }

namespace hive
{
    DEFINE_HANDLE(ShaderProgram);
    DEFINE_HANDLE(VertexBuffer);

}