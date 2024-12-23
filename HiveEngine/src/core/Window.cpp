#include "Window.h"
#include <platform/Platform.h>


#include <platform/glfw/window_glfw.h>


hive::Window::Window(WindowConfig config)
{
    switch (config.type)
    {
        case WindowConfig::WindowType::Native:
            //not supported yet
            break;
        case WindowConfig::WindowType::GLFW:
            glfw_init_window(config);
            break;
        case WindowConfig::WindowType::Raylib:
            //not supported yet
            break;
    }
}
