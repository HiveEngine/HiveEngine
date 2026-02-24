#pragma once

#include <terra/terra.h>
#include <GLFW/glfw3.h>

namespace terra
{
    struct WindowContext
    {
        //GLFW stuff here
        GLFWwindow *window_{nullptr};
        const char *title_{nullptr};
        int width_{0};
        int height_{0};

        InputState currentInputState_{};
        InputState lastInputState_{};
    };
}