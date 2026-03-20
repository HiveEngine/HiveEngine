#include <terra/platform/glfw_terra.h>
#include <terra/precomp.h>
#define TERRA_NATIVE_LINUX
#include <terra/terra_native.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>
namespace terra
{
    NativeWindow GetNativeWindow(WindowContext* windowContext)
    {
        NativeWindow nativeWindow{};
        GLFWwindow* glfwWindow = GetGlfwWindow(windowContext);

        nativeWindow.m_wlDisplay = glfwGetWaylandDisplay();
        nativeWindow.m_wlSurface = glfwGetWaylandWindow(glfwWindow);

        nativeWindow.m_x11Display = glfwGetX11Display();
        nativeWindow.m_x11Window = glfwGetX11Window(glfwWindow);

        if (nativeWindow.m_wlDisplay && nativeWindow.m_wlSurface)
        {
            nativeWindow.m_type = NativeWindowType::WAYLAND;
        }
        else if (nativeWindow.m_x11Display && nativeWindow.m_x11Window)
        {
            nativeWindow.m_type = NativeWindowType::X11;
        }

        return nativeWindow;
    }
} // namespace terra
