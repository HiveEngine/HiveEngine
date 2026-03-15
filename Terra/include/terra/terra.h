#pragma once

#include <terra/input/keys.h>

namespace terra
{
    // Opaque handle
    struct WindowContext;

    struct InputState
    {
        // Keyboard
        bool m_keys[512]{};

        // Mouse
        bool m_mouseButton[8]{};
        float m_mouseDeltaX{0.0f};
        float m_mouseDeltaY{0.0f}; // Delta from last frame
        float m_mouseX{0.0f};
        float m_mouseY{0.0f}; // Current mouse position on screen
    };

    // Windowing system init
    bool InitSystem();

    void ShutdownSystem();

    // Window operation
    WindowContext* CreateWindow(const char* title, int width, int height);
    void DestroyWindow(WindowContext* windowContext);


    bool ShouldWindowClose(WindowContext* windowContext);

    [[nodiscard]] const InputState* GetWindowInputState(const WindowContext* windowContext);

    [[nodiscard]] InputState* GetWindowInputState(WindowContext* windowContext);

    [[nodiscard]] bool IsKeyDown(const InputState* inputState, Key key);

    [[nodiscard]] bool IsMouseButtonDown(const InputState* inputState, MouseButton button);

    [[nodiscard]] float GetMouseDeltaX(const InputState* inputState);

    [[nodiscard]] float GetMouseDeltaY(const InputState* inputState);

    [[nodiscard]] float GetMouseX(const InputState* inputState);

    [[nodiscard]] float GetMouseY(const InputState* inputState);

    [[nodiscard]] int GetWindowWidth(const WindowContext* windowContext);

    [[nodiscard]] int GetWindowHeight(const WindowContext* windowContext);

    void SetWindowSize(WindowContext* windowContext, int width, int height);

    void SetWindowTitle(WindowContext* windowContext, const char* title);

    // Inputs
    void PollEvents(); // Called once per frame

    void PollEvents(WindowContext* windowContext); // Preferred when polling a specific window
} // namespace terra
