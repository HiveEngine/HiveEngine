#pragma once
#include <terra/terra.h>

#if defined(TERRA_NATIVE_LINUX)
#include <X11/Xlib.h>
#include <wayland-client.h>
namespace terra
{
    enum class NativeWindowType
    {
        X11,
        WAYLAND
    };
    struct NativeWindow
    {
        wl_display* m_wlDisplay{nullptr};
        wl_surface* m_wlSurface{nullptr};

        Display* m_x11Display{nullptr};
        Window m_x11Window{0};

        NativeWindowType m_type{NativeWindowType::WAYLAND};
    };
} // namespace terra
#elif defined(TERRA_NATIVE_WIN32)
#include <Windows.h>
namespace terra
{
    struct NativeWindow
    {
        HINSTANCE m_instance{nullptr};
        HWND m_window{nullptr};
    };
} // namespace terra
#else
#error "Please define a TERRA_NATIVE_ in order to active the right platform definition
#endif

namespace terra
{
    HIVE_API NativeWindow GetNativeWindow(WindowContext* windowContext);
}
