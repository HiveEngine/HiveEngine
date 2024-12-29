#pragma once
#include <core/Window.h>
#include <GLFW/glfw3.h>

namespace hive
{

    class WindowGLFW final : public IWindow
    {
    public:
        ~WindowGLFW() override;
        WindowGLFW(const WindowConfig &config);



        [[nodiscard]] u64 getSizeof() const override;

        [[nodiscard]] bool shouldClose() const override;

        void pollEvents() override;

    private:
        GLFWwindow* window_;
    };
}
