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

        //TODO get this from the InitWindowContext parameter or something else
        windowContext->title_ = "Hive Engine";
        windowContext->width_ = 600;
        windowContext->height_ = 600;
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