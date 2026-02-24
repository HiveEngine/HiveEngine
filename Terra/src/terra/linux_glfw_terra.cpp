#include <terra/precomp.h>
#include <terra/platform/glfw_terra.h>
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

        nativeWindow.wlDisplay_ = glfwGetWaylandDisplay();
        nativeWindow.wlSurface_ = glfwGetWaylandWindow(windowContext->window_);

        nativeWindow.x11Display_ = glfwGetX11Display();
        nativeWindow.x11Window_ = glfwGetX11Window(windowContext->window_);

        if (nativeWindow.wlDisplay_ && nativeWindow.wlSurface_)
        {
            nativeWindow.type_ = NativeWindowType::WAYLAND;
        }
        else if (nativeWindow.x11Display_ && nativeWindow.x11Window_)
        {
            nativeWindow.type_ = NativeWindowType::X11;
        }


        return nativeWindow;
    }
}