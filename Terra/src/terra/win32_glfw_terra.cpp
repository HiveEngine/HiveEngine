#include <terra/precomp.h>
#include <terra/terra.h>
#define TERRA_NATIVE_WIN32
#include <terra/terra_native.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <terra/platform/glfw_terra.h>

#include <GLFW/glfw3native.h>

namespace terra
{
    NativeWindow GetNativeWindow(WindowContext* windowContext)
    {
        NativeWindow nativeWindow;

        nativeWindow.m_window = glfwGetWin32Window(GetGlfwWindow(windowContext));
        nativeWindow.m_instance = nullptr;

        return nativeWindow;
    }
} // namespace terra
