#pragma once

#include <hive/hive_config.h>

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

    [[nodiscard]] HIVE_API GLFWwindow* GetGlfwWindow(WindowContext* windowContext);

    [[nodiscard]] HIVE_API const GLFWwindow* GetGlfwWindow(const WindowContext* windowContext);
} // namespace terra
