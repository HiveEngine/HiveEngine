//
// Created by GuillaumeIsCoding on 7/28/2024.
//

#ifdef HIVE_PLATFORM_GLFW
#include "core/inputs/mouse.h"
#include "platform/glfw/glfw_mouse.h"

namespace hive
{
    std::unique_ptr<Mouse> Mouse::create(void* window, const MouseStates& configuration)
    {
        #ifdef HIVE_PLATFORM_GLFW
        return std::make_unique<hive::GlfwMouse>(window, configuration);
        #else
            // LYPO_CORE_ERROR("Unknown platform");
            return nullptr;
        #endif
    }
}
#endif
