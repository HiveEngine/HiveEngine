#include <terra/precomp.h>
#include <terra/platform/glfw_terra.h>

namespace terra
{
    bool InitSystem()
    {
        return glfwInit();
    }

    void ShutdownSystem()
    {
        glfwTerminate();
    }

    void GLFWKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
    {
        WindowContext *windowContext = static_cast<WindowContext *>(glfwGetWindowUserPointer(window));
        if (key >= 0 && key < 512)
        {
            windowContext->currentInputState_.keys_[key] = (action == GLFW_PRESS);
        }
    }

    bool InitWindowContext(WindowContext *windowContext)
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //Disable glfw to create the OpenGL context. We will manage that in swarm

        if (windowContext->width_ <= 0) windowContext->width_ = 1280;
        if (windowContext->height_ <= 0) windowContext->height_ = 720;
        if (!windowContext->title_) windowContext->title_ = "Hive Engine";
        windowContext->window_ = glfwCreateWindow(windowContext->width_, windowContext->height_, windowContext->title_,
                                                  nullptr, nullptr);

        if (!windowContext->window_)
        {
            return false;
        }

        glfwSetWindowUserPointer(windowContext->window_, windowContext);
        glfwSetKeyCallback(windowContext->window_, &GLFWKeyCallback);


        glfwMakeContextCurrent(windowContext->window_);
        return true;
    }

    void ShutdownWindowContext(WindowContext *windowContext)
    {
        glfwDestroyWindow(windowContext->window_);
    }

    bool ShouldWindowClose(WindowContext *windowContext)
    {
        return glfwWindowShouldClose(windowContext->window_);
    }

    void PollEvents()
    {
        glfwPollEvents();
    }


    InputState *GetWindowInputState(WindowContext *windowContext)
    {
        return &windowContext->currentInputState_;
    }
}