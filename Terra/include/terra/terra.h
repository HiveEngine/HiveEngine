#pragma once

namespace terra
{
    // Opaque handle
    struct WindowContext;

    struct InputState
    {
        // Keyboard
        bool m_keys[512]; // TODO have some easy getter for specific KEYS

        // Mouse
        bool m_mouseButton[8];
        float m_mouseDeltaX;
        float m_mouseDeltaY; // Delta from last frame
        float m_mouseX;
        float m_mouseY; // Current mouse position on screen
    };

    // Windowing system init
    bool InitSystem();

    void ShutdownSystem();

    // Window operation
    bool InitWindowContext(WindowContext* windowContext);

    void ShutdownWindowContext(WindowContext* windowContext);

    bool ShouldWindowClose(WindowContext* windowContext);

    InputState* GetWindowInputState(WindowContext* windowContext);

    // Inputs
    void PollEvents(); // Called once per frame
} // namespace terra