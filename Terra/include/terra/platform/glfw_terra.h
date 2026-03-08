#pragma once

#include <terra/terra.h>

#include <GLFW/glfw3.h>

namespace terra
{
    struct WindowContext
    {
        // Backend state. Prefer the accessors in <terra/terra.h> from calling code.
        GLFWwindow* m_window{nullptr};
        const char* m_title{nullptr};
        int m_width{0};
        int m_height{0};

        InputState m_currentInputState{};
        InputState m_lastInputState{};
    };

    [[nodiscard]] GLFWwindow* GetGlfwWindow(WindowContext* windowContext);

    [[nodiscard]] const GLFWwindow* GetGlfwWindow(const WindowContext* windowContext);
} // namespace terra
