#include <terra/precomp.h>

#include <terra/window/window.h>
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>

#include <cstdlib>
#include <cstring>

namespace terra
{
    Window::NativeHandle Window::GetNativeHandle() const
    {
        NativeHandle handle{};

        const std::string envSessionType = getenv("XDG_SESSION_TYPE");
        if (envSessionType == "")
        {
            //TODO log error
        }

        switch (const NativeHandle::SessionType sessionType = envSessionType == "wayland" ? NativeHandle::SessionType::WAYLAND : NativeHandle::SessionType::X11)
        {
            case NativeHandle::SessionType::X11:
            {
                handle.displayHandle = glfwGetX11Display();
                handle.windowId = glfwGetX11Window(m_Window);
                handle.sessionType = sessionType;
                break;
            }
            case NativeHandle::SessionType::WAYLAND:
            {
                handle.displayHandle = glfwGetWaylandDisplay();
                handle.windowHandle = glfwGetWaylandWindow(m_Window);
                handle.sessionType = sessionType;
                break;
            }
        }

        return handle;
    }

}
