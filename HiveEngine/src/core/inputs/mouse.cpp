//
// Created by GuillaumeIsCoding on 7/28/2024.
//
#include "core/inputs/mouse.h"

#ifdef HIVE_PLATFORM_GLFW
#include "platform/glfw/glfw_mouse.h"
#endif

#ifdef HIVE_BACKEND_RAYLIB
#include "platform/raylib/RaylibMouse.h"
#endif

namespace hive
{
    std::unique_ptr<Mouse> Mouse::create(void* window, const MouseStates& configuration)
    {
        #ifdef HIVE_PLATFORM_GLFW
        return std::make_unique<hive::GlfwMouse>(window, configuration);
        #elifdef HIVE_BACKEND_RAYLIB
        return std::make_unique<hive::RaylibMouse>(window, configuration);
        #else
            // LYPO_CORE_ERROR("Unknown platform");
            return nullptr;
        #endif
    }
}