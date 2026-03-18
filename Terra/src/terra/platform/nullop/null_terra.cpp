#include <terra/platform/null_terra.h>

namespace terra
{
    bool InitSystem()
    {
        return false;
    }

    void ShutdownSystem() {}


    WindowContext* CreateWindowContext(const char* /*title*/, int /*width*/, int /*height*/)
    {
        return nullptr;
    }

    void DestroyWindowContext(WindowContext* /*windowContext*/) {}

    void SetWindowVisible(WindowContext* /*windowContext*/, bool /*visible*/) {}

    bool ShouldWindowClose(WindowContext* /*windowContext*/)
    {
        return true;
    }

    void PollEvents() {}

    void PollEvents(WindowContext* /*windowContext*/) {}

    const InputState* GetWindowInputState(const WindowContext* windowContext)
    {
        return windowContext != nullptr ? &windowContext->m_currentInputState : nullptr;
    }

    InputState* GetWindowInputState(WindowContext* windowContext)
    {
        return windowContext != nullptr ? &windowContext->m_currentInputState : nullptr;
    }

    bool IsKeyDown(const InputState* /*inputState*/, Key /*key*/)
    {
        return false;
    }

    bool IsMouseButtonDown(const InputState* /*inputState*/, MouseButton /*button*/)
    {
        return false;
    }

    float GetMouseDeltaX(const InputState* /*inputState*/)
    {
        return 0.0f;
    }

    float GetMouseDeltaY(const InputState* /*inputState*/)
    {
        return 0.0f;
    }

    float GetMouseX(const InputState* /*inputState*/)
    {
        return 0.0f;
    }

    float GetMouseY(const InputState* /*inputState*/)
    {
        return 0.0f;
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
    }

    void SetWindowTitle(WindowContext* windowContext, const char* title)
    {
        if (windowContext == nullptr)
        {
            return;
        }

        windowContext->m_title = title;
    }
} // namespace terra
