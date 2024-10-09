//
// Created by GuillaumeIsCoding on 7/26/2024.
//
#pragma once
#include "window_configuration.h"


namespace hive
{

    struct WindowNativeData
    {
        void* window_handle;
        enum WindowBackend
        {
            GLFW,
        };

        WindowBackend backend;
    };

    class Window {
    public:
        virtual ~Window() = default;

        [[nodiscard]] virtual int getHeight() const = 0;
        [[nodiscard]] virtual WindowNativeData getNativeWindowData() const = 0;
        [[nodiscard]] virtual int getWidth() const = 0;
        virtual void onUpdate() const = 0;
        virtual void setIcon(unsigned char* data, int width, int height) const = 0;
        [[nodiscard]] virtual bool shouldClose() const = 0;
        virtual void updateConfiguration(WindowConfiguration configuration) = 0;
        virtual WindowConfiguration getConfiguration() = 0;
    };
}
