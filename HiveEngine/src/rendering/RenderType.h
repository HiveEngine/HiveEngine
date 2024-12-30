#pragma once
#include <hvpch.h>
#include <vector>

#define DEFINE_HANDLE(_name) struct _name##Handle {u32 id; }

namespace hive
{
    DEFINE_HANDLE(ShaderProgram);

    struct PhysicalDeviceRequirements
    {
        bool graphics;
        bool presentation;
        bool compute;
        bool transfer;
        bool discrete_gpu;


        std::vector<const char*> extensions;
        //... other
    };
}