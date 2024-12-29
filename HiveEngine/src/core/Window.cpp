#include "core/Window.h"
#include "core/Memory.h"

#include <platform/Platform.h>
#include <platform/glfw/window_glfw.h>

#include "core/Logger.h"


hive::Window::Window(const WindowConfig &config) : window_handle(nullptr)
{
    LOG_INFO("Creating the Window");
    switch (config.type)
    {
        case WindowConfig::WindowType::Native:
            //not supported yet
            break;
        case WindowConfig::WindowType::GLFW:
        {
            auto window_glfw = Memory::createObject<WindowGLFW, Memory::ENGINE>(config);

            window_handle = window_glfw;
            break;
        }

        case WindowConfig::WindowType::Raylib:
            //not supported yet
            break;
        case WindowConfig::WindowType::NONE:
            break;
    }
}

hive::Window::~Window()
{
    //TODO: handle multiple type of window
    Memory::destroyObject<WindowGLFW, Memory::ENGINE>(window_handle);
}

bool hive::Window::shouldClose() const
{
    return window_handle->shouldClose();
}

void hive::Window::pollEvents()
{
    window_handle->pollEvents();
}
