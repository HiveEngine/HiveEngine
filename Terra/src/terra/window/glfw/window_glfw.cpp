#include <terra/precomp.h>

#include <terra/window/window.h>

namespace terra
{

    Window::Window(WindowDescription description) : m_Window(nullptr)
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_Window = glfwCreateWindow(static_cast<int>(description.width), static_cast<int>(description.height), description.title, nullptr, nullptr);
    }

    Window::~Window()
    {
        if (m_Window != nullptr)
            glfwDestroyWindow(m_Window);
    }

    bool Window::BackendInitialize()
    {
        return glfwInit();
    }

    void Window::BackendShutdown()
    {
        glfwTerminate();
    }

    void Window::PollEvents()
    {
        glfwPollEvents();
    }

    bool Window::ShouldClose() const
    {
        return glfwWindowShouldClose(m_Window);
    }
}
