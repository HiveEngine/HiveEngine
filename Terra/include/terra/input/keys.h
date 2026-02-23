#pragma once

#include <cstdint>

namespace terra
{
    // Values match GLFW key codes for trivial backend mapping
    enum class Key : int32_t
    {
        Space        = 32,
        Apostrophe   = 39,
        Comma        = 44,
        Minus        = 45,
        Period       = 46,
        Slash        = 47,

        D0 = 48, D1, D2, D3, D4, D5, D6, D7, D8, D9,

        Semicolon    = 59,
        Equal        = 61,

        A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        LeftBracket  = 91,
        Backslash    = 92,
        RightBracket = 93,
        GraveAccent  = 96,

        Escape       = 256,
        Enter        = 257,
        Tab          = 258,
        Backspace    = 259,
        Insert       = 260,
        Delete       = 261,
        Right        = 262,
        Left         = 263,
        Down         = 264,
        Up           = 265,
        PageUp       = 266,
        PageDown     = 267,
        Home         = 268,
        End          = 269,

        CapsLock     = 280,
        ScrollLock   = 281,
        NumLock      = 282,
        PrintScreen  = 283,
        Pause        = 284,

        F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

        LeftShift    = 340,
        LeftControl  = 341,
        LeftAlt      = 342,
        LeftSuper    = 343,
        RightShift   = 344,
        RightControl = 345,
        RightAlt     = 346,
        RightSuper   = 347,

        COUNT        = 349
    };

    enum class MouseButton : int32_t
    {
        Left   = 0,
        Right  = 1,
        Middle = 2,
        COUNT  = 8
    };

    enum class CursorMode : int32_t
    {
        Normal,   // visible, free movement
        Disabled  // hidden, infinite movement (FPS-style)
    };
}
