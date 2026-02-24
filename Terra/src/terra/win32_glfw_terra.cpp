#include <terra/precomp.h>
#include <terra/terra.h>
#define TERRA_NATIVE_WIN32
#include <terra/terra_native.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "terra/platform/glfw_terra.h"

namespace terra
{
    NativeWindow GetNativeWindow(WindowContext *windowContext)
    {
        NativeWindow nativeWindow;

        nativeWindow.window_ = glfwGetWin32Window(windowContext->window_);
        nativeWindow.instance_ = nullptr;

        return nativeWindow;
    }
}
