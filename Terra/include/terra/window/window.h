#pragma once

namespace terra
{
    struct WindowDescription
    {
        const char* title;
        unsigned int width;
        unsigned int height;
    };

    class Window
    {
    public:
        explicit Window(WindowDescription description);
        ~Window();

        [[nodiscard]] static bool BackendInitialize();
        static void BackendShutdown();
        static void PollEvents();

        struct NativeHandle
        {
            //X11: windowId for Window XID, displayHandle for _XDisplay
            //Wayland: windowHandle for wl_surface, displayHandle for wl_surface
            //Win32: displayHandle for HINSTANCE, windowHandle for HWND

            void* windowHandle{nullptr};
            void* displayHandle{nullptr};
            unsigned long windowId{0};

            enum class SessionType
            {
                NONE, WAYLAND, X11, WINDOWS
            };
            SessionType sessionType = SessionType::NONE;

        };

        [[nodiscard]] NativeHandle GetNativeHandle() const;

        [[nodiscard]] bool ShouldClose() const;

    private:
        #include <terra/window/inl/windowbackend.inl>

    };
}