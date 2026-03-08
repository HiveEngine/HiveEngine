#include <terra/platform/glfw_terra.h>
#include <terra/precomp.h>

namespace terra
{
    bool InitSystem() {
        return glfwInit();
    }

    void ShutdownSystem() {
        glfwTerminate();
    }

    void GLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        WindowContext* windowContext = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));
        if (key >= 0 && key < 512)
        {
            windowContext->m_currentInputState.m_keys[key] = (action == GLFW_PRESS);
        }
    }

    bool InitWindowContext(WindowContext* windowContext) {
        glfwWindowHint(GLFW_CLIENT_API,
                       GLFW_NO_API); // Disable glfw to create the OpenGL context. We will manage that in swarm

        if (windowContext->m_width <= 0)
            windowContext->m_width = 1280;
        if (windowContext->m_height <= 0)
            windowContext->m_height = 720;
        if (!windowContext->m_title)
            windowContext->m_title = "Hive Engine";
        windowContext->m_window =
            glfwCreateWindow(windowContext->m_width, windowContext->m_height, windowContext->m_title, nullptr, nullptr);

        if (!windowContext->m_window)
        {
            return false;
        }

        glfwSetWindowUserPointer(windowContext->m_window, windowContext);
        glfwSetKeyCallback(windowContext->m_window, &GLFWKeyCallback);

        glfwMakeContextCurrent(windowContext->m_window);
        return true;
    }

    void ShutdownWindowContext(WindowContext* windowContext) {
        glfwDestroyWindow(windowContext->m_window);
    }

    bool ShouldWindowClose(WindowContext* windowContext) {
        return glfwWindowShouldClose(windowContext->m_window);
    }

    void PollEvents() {
        glfwPollEvents();
    }

    InputState* GetWindowInputState(WindowContext* windowContext) {
        return &windowContext->m_currentInputState;
    }
} // namespace terra
