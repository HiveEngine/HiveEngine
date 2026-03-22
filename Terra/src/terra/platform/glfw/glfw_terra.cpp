#include <terra/platform/glfw_terra.h>
#include <terra/precomp.h>
#include <terra/terramodule.h>

namespace terra
{
    namespace
    {
        constexpr int kMaxKeys = 512;
        constexpr int kMaxMouseButtons = 8;

        void GLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
        {
            (void)scancode;
            (void)mods;

            WindowContext* windowContext = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));
            if (windowContext == nullptr || key < 0 || key >= kMaxKeys)
            {
                return;
            }

            windowContext->m_currentInputState.m_keys[key] = (action != GLFW_RELEASE);
        }

        void GLFWMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
        {
            (void)mods;

            WindowContext* windowContext = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));
            if (windowContext == nullptr || button < 0 || button >= kMaxMouseButtons)
            {
                return;
            }

            windowContext->m_currentInputState.m_mouseButton[button] = (action != GLFW_RELEASE);
        }

        void GLFWCursorPositionCallback(GLFWwindow* window, double x, double y)
        {
            WindowContext* windowContext = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));
            if (windowContext == nullptr)
            {
                return;
            }

            const float newX = static_cast<float>(x);
            const float newY = static_cast<float>(y);

            windowContext->m_currentInputState.m_mouseDeltaX += newX - windowContext->m_currentInputState.m_mouseX;
            windowContext->m_currentInputState.m_mouseDeltaY += newY - windowContext->m_currentInputState.m_mouseY;
            windowContext->m_currentInputState.m_mouseX = newX;
            windowContext->m_currentInputState.m_mouseY = newY;
        }

        void GLFWScrollCallback(GLFWwindow* window, double xOffset, double yOffset)
        {
            WindowContext* windowContext = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));
            if (windowContext == nullptr)
            {
                return;
            }

            windowContext->m_currentInputState.m_scrollDeltaX += static_cast<float>(xOffset);
            windowContext->m_currentInputState.m_scrollDeltaY += static_cast<float>(yOffset);
        }

        void GLFWWindowSizeCallback(GLFWwindow* window, int width, int height)
        {
            WindowContext* windowContext = static_cast<WindowContext*>(glfwGetWindowUserPointer(window));
            if (windowContext == nullptr)
            {
                return;
            }

            windowContext->m_width = width;
            windowContext->m_height = height;
        }

        bool InitWindowContext(WindowContext* windowContext)
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

            if (windowContext->m_width <= 0)
                windowContext->m_width = 1280;
            if (windowContext->m_height <= 0)
                windowContext->m_height = 720;
            if (!windowContext->m_title)
                windowContext->m_title = "HiveEngine";
            windowContext->m_window = glfwCreateWindow(windowContext->m_width, windowContext->m_height,
                                                       windowContext->m_title, nullptr, nullptr);

            if (!windowContext->m_window)
            {
                return false;
            }

            glfwSetWindowUserPointer(windowContext->m_window, windowContext);
            glfwSetKeyCallback(windowContext->m_window, &GLFWKeyCallback);
            glfwSetMouseButtonCallback(windowContext->m_window, &GLFWMouseButtonCallback);
            glfwSetCursorPosCallback(windowContext->m_window, &GLFWCursorPositionCallback);
            glfwSetScrollCallback(windowContext->m_window, &GLFWScrollCallback);
            glfwSetWindowSizeCallback(windowContext->m_window, &GLFWWindowSizeCallback);

            glfwGetWindowSize(windowContext->m_window, &windowContext->m_width, &windowContext->m_height);

            double cursorX = 0.0;
            double cursorY = 0.0;
            glfwGetCursorPos(windowContext->m_window, &cursorX, &cursorY);
            windowContext->m_currentInputState.m_mouseX = static_cast<float>(cursorX);
            windowContext->m_currentInputState.m_mouseY = static_cast<float>(cursorY);
            windowContext->m_lastInputState = windowContext->m_currentInputState;

            glfwMakeContextCurrent(windowContext->m_window);
            return true;
        }

        void ShutdownWindowContext(WindowContext* windowContext)
        {
            if (windowContext == nullptr || windowContext->m_window == nullptr)
            {
                return;
            }

            glfwDestroyWindow(windowContext->m_window);
            windowContext->m_window = nullptr;
        }
    } // namespace

    bool InitSystem()
    {
        return glfwInit();
    }

    void ShutdownSystem()
    {
        glfwTerminate();
    }

    WindowContext* CreateWindowContext(const char* title, int width, int height)
    {
        auto& allocator = TerraModule::GetInstance().GetAllocator();
        WindowContext* windowContext = comb::New<WindowContext>(allocator);
        hive::Assert(windowContext != nullptr, "Failed to allocate WindowContext");

        windowContext->m_title = title;
        windowContext->m_width = width;
        windowContext->m_height = height;

        if (!InitWindowContext(windowContext))
        {
            comb::Delete(allocator, windowContext);
            return nullptr;
        }

        return windowContext;
    }

    void DestroyWindowContext(WindowContext* windowContext)
    {
        if (windowContext == nullptr)
        {
            return;
        }

        ShutdownWindowContext(windowContext);
        auto& allocator = TerraModule::GetInstance().GetAllocator();
        comb::Delete(allocator, windowContext);
    }

    bool ShouldWindowClose(WindowContext* windowContext)
    {
        return glfwWindowShouldClose(windowContext->m_window);
    }

    void PollEvents()
    {
        glfwPollEvents();
    }

    void PollEvents(WindowContext* windowContext)
    {
        if (windowContext != nullptr)
        {
            windowContext->m_lastInputState = windowContext->m_currentInputState;
            windowContext->m_currentInputState.m_mouseDeltaX = 0.0f;
            windowContext->m_currentInputState.m_mouseDeltaY = 0.0f;
            windowContext->m_currentInputState.m_scrollDeltaX = 0.0f;
            windowContext->m_currentInputState.m_scrollDeltaY = 0.0f;
        }

        glfwPollEvents();
    }

    const InputState* GetWindowInputState(const WindowContext* windowContext)
    {
        if (windowContext == nullptr)
        {
            return nullptr;
        }

        return &windowContext->m_currentInputState;
    }

    InputState* GetWindowInputState(WindowContext* windowContext)
    {
        if (windowContext == nullptr)
        {
            return nullptr;
        }

        return &windowContext->m_currentInputState;
    }

    bool IsKeyDown(const InputState* inputState, Key key)
    {
        if (inputState == nullptr)
        {
            return false;
        }

        const int index = static_cast<int>(key);
        return index >= 0 && index < kMaxKeys && inputState->m_keys[index];
    }

    bool IsMouseButtonDown(const InputState* inputState, MouseButton button)
    {
        if (inputState == nullptr)
        {
            return false;
        }

        const int index = static_cast<int>(button);
        return index >= 0 && index < kMaxMouseButtons && inputState->m_mouseButton[index];
    }

    float GetMouseDeltaX(const InputState* inputState)
    {
        return inputState != nullptr ? inputState->m_mouseDeltaX : 0.0f;
    }

    float GetMouseDeltaY(const InputState* inputState)
    {
        return inputState != nullptr ? inputState->m_mouseDeltaY : 0.0f;
    }

    float GetMouseX(const InputState* inputState)
    {
        return inputState != nullptr ? inputState->m_mouseX : 0.0f;
    }

    float GetMouseY(const InputState* inputState)
    {
        return inputState != nullptr ? inputState->m_mouseY : 0.0f;
    }

    float GetScrollDeltaX(const InputState* inputState)
    {
        return inputState != nullptr ? inputState->m_scrollDeltaX : 0.0f;
    }

    float GetScrollDeltaY(const InputState* inputState)
    {
        return inputState != nullptr ? inputState->m_scrollDeltaY : 0.0f;
    }

    int GetWindowWidth(const WindowContext* windowContext)
    {
        return windowContext != nullptr ? windowContext->m_width : 0;
    }

    int GetWindowHeight(const WindowContext* windowContext)
    {
        return windowContext != nullptr ? windowContext->m_height : 0;
    }

    void SetWindowSize(WindowContext* windowContext, int width, int height)
    {
        if (windowContext == nullptr)
        {
            return;
        }

        windowContext->m_width = width;
        windowContext->m_height = height;

        if (windowContext->m_window != nullptr)
        {
            glfwSetWindowSize(windowContext->m_window, width, height);
        }
    }

    void SetWindowTitle(WindowContext* windowContext, const char* title)
    {
        if (windowContext == nullptr)
        {
            return;
        }

        windowContext->m_title = title;

        if (windowContext->m_window != nullptr && title != nullptr)
        {
            glfwSetWindowTitle(windowContext->m_window, title);
        }
    }

    void SetWindowVisible(WindowContext* windowContext, bool visible)
    {
        if (windowContext == nullptr || windowContext->m_window == nullptr)
            return;

        if (visible)
            glfwShowWindow(windowContext->m_window);
        else
            glfwHideWindow(windowContext->m_window);
    }

    GLFWwindow* GetGlfwWindow(WindowContext* windowContext)
    {
        return windowContext != nullptr ? windowContext->m_window : nullptr;
    }

    const GLFWwindow* GetGlfwWindow(const WindowContext* windowContext)
    {
        return windowContext != nullptr ? windowContext->m_window : nullptr;
    }
} // namespace terra
