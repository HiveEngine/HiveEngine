#pragma once
#include <hvpch.h>

namespace hive
{

    struct WindowConfig
    {
        enum class WindowType
        {
            Native, GLFW, Raylib, NONE
        };

        WindowType type;
        u16 width, height;
        const char* title;

    };

    class IWindow
    {
    public:
        virtual ~IWindow()= default;
        [[nodiscard]] virtual u64 getSizeof() const = 0;
        [[nodiscard]] virtual bool shouldClose() const = 0;
        virtual void pollEvents() = 0;

    };


    class Window
    {
    public:
        explicit Window(const WindowConfig &config);
        ~Window();

        [[nodiscard]] bool shouldClose() const;
        void pollEvents();

    private:
        IWindow* window_handle{};
    };
}