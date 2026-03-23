#pragma once

#include <hive/hive_config.h>

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
        float m_scrollDeltaX{0.0f};
        float m_scrollDeltaY{0.0f};
    };

    // Windowing system init
    HIVE_API bool InitSystem();

    HIVE_API void ShutdownSystem();

    // Window operation
    HIVE_API WindowContext* CreateWindowContext(const char* title, int width, int height);
    HIVE_API void DestroyWindowContext(WindowContext* windowContext);

    HIVE_API bool ShouldWindowClose(WindowContext* windowContext);

    [[nodiscard]] HIVE_API const InputState* GetWindowInputState(const WindowContext* windowContext);

    [[nodiscard]] HIVE_API InputState* GetWindowInputState(WindowContext* windowContext);

    [[nodiscard]] HIVE_API bool IsKeyDown(const InputState* inputState, Key key);

    [[nodiscard]] HIVE_API bool IsMouseButtonDown(const InputState* inputState, MouseButton button);

    [[nodiscard]] HIVE_API float GetMouseDeltaX(const InputState* inputState);

    [[nodiscard]] HIVE_API float GetMouseDeltaY(const InputState* inputState);

    [[nodiscard]] HIVE_API float GetMouseX(const InputState* inputState);

    [[nodiscard]] HIVE_API float GetMouseY(const InputState* inputState);

    [[nodiscard]] HIVE_API float GetScrollDeltaX(const InputState* inputState);

    [[nodiscard]] HIVE_API float GetScrollDeltaY(const InputState* inputState);

    [[nodiscard]] HIVE_API int GetWindowWidth(const WindowContext* windowContext);

    [[nodiscard]] HIVE_API int GetWindowHeight(const WindowContext* windowContext);

    HIVE_API void SetWindowSize(WindowContext* windowContext, int width, int height);

    HIVE_API void SetWindowVisible(WindowContext* windowContext, bool visible);

    HIVE_API void SetWindowTitle(WindowContext* windowContext, const char* title);

    // Inputs
    HIVE_API void PollEvents(); // Called once per frame

    HIVE_API void PollEvents(WindowContext* windowContext); // Preferred when polling a specific window
} // namespace terra
