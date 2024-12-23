#pragma once
#include <hvpch.h>

namespace hive
{

    struct WindowConfig
    {
        enum class WindowType
        {
            Native, GLFW, Raylib
        };

        WindowType type;
        u16 width, height;
        const char* title;

    };
    class Window
    {
    public:
        explicit Window(WindowConfig config);
    private:
        void* window_handle{};
    };
}