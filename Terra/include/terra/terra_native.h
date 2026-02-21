#pragma once
#include <terra/terra.h>

#if defined(TERRA_NATIVE_LINUX)
#include <wayland-client.h>
#include <X11/Xlib.h>
namespace terra
{
    enum class NativeWindowType
    {
        X11, WAYLAND
    };
    struct NativeWindow
    {
        wl_display *wlDisplay_{nullptr};
        wl_surface *wlSurface_{nullptr};

        Display *x11Display_{nullptr};
        Window x11Window_{0};

        NativeWindowType type_{NativeWindowType::WAYLAND};
    };
}
#elif defined(TERRA_NATIVE_WIN32)
namespace terra
{
    struct NativeWindow
    {

    };
}
#else
#error "Please define a TERRA_NATIVE_ in order to active the right platform definition
#endif

namespace terra
{
    NativeWindow GetNativeWindow(WindowContext *windowContext);
}
