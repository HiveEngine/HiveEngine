#include <terra/precomp.h>

#include <terra/window/window.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace terra
{
    Window::NativeHandle Window::GetNativeHandle() const
    {
        NativeHandle nativeHandle{};

        nativeHandle.windowHandle = glfwGetWin32Window(m_Window);
        nativeHandle.displayHandle = GetModuleHandle(nullptr);
        nativeHandle.sessionType = NativeHandle::SessionType::WINDOWS;

        return nativeHandle;
    }

}
