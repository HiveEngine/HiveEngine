//
// Created by samuel on 9/16/24.
//
#ifndef _KEYCODE_H_
#define _KEYCODE_H_


namespace hive {

    enum class KeyCode
    {
        KEY_SPACE,
        KEY_APOSTROPHE = 39,
        KEY_COMMA = 44,
        KEY_MINUS = 45,
        KEY_PERIOD = 46,
        KEY_SLASH = 47,

        KEY_0 = 48,
        KEY_1 = 49,
        KEY_2 = 50,
        KEY_3 = 51,
        KEY_4 = 52,
        KEY_5 = 53,
        KEY_6 = 54,
        KEY_7 = 55,
        KEY_8 = 56,
        KEY_9 = 57,
        KEY_SEMICOLON = 59 /* ; */,
        KEY_EQUAL = 61 /* = */,
        KEY_A = 65,
        KEY_B = 66,
        KEY_C = 67,
        KEY_D = 68,
        KEY_E = 69,
        KEY_F = 70,
        KEY_G = 71,
        KEY_H = 72,
        KEY_I = 73,
        KEY_J = 74,
        KEY_K = 75,
        KEY_L = 76,
        KEY_M = 77,
        KEY_N = 78,
        KEY_O = 79,
        KEY_P = 80,
        KEY_Q = 81,
        KEY_R = 82,
        KEY_S = 83,
        KEY_T = 84,
        KEY_U = 85,
        KEY_V = 86,
        KEY_W = 87,
        KEY_X = 88,
        KEY_Y = 89,
        KEY_Z = 90,
        KEY_LEFT_BRACKET = 91 /* [ */,
        KEY_BACKSLASH = 92 /* \ */,
        KEY_RIGHT_BRACKET = 93 /* ] */,
        KEY_GRAVE_ACCENT = 96 /* ` */,


        /* Function keys */
        KEY_ESCAPE = 256,
        KEY_ENTER = 257,
        KEY_TAB = 258,
        KEY_BACKSPACE = 259,
        KEY_INSERT = 260,
        KEY_DELETE = 261,
        KEY_RIGHT = 262,
        KEY_LEFT = 263,
        KEY_DOWN = 264,
        KEY_UP = 265,
        KEY_PAGE_UP = 266,
        KEY_PAGE_DOWN = 267,
        KEY_HOME = 268,
        KEY_END = 269,
        KEY_CAPS_LOCK = 280,
        KEY_SCROLL_LOCK = 281,
        KEY_NUM_LOCK = 282,
        KEY_PRINT_SCREEN = 283,
        KEY_PAUSE = 284,
        KEY_F1 = 290,
        KEY_F2 = 291,
        KEY_F3 = 292,
        KEY_F4 = 293,
        KEY_F5 = 294,
        KEY_F6 = 295,
        KEY_F7 = 296,
        KEY_F8 = 297,
        KEY_F9 = 298,
        KEY_F10 = 299,
        KEY_F11 = 300,
        KEY_F12 = 301,
        KEY_KP_0 = 320,
        KEY_KP_1 = 321,
        KEY_KP_2 = 322,
        KEY_KP_3 = 323,
        KEY_KP_4 = 324,
        KEY_KP_5 = 325,
        KEY_KP_6 = 326,
        KEY_KP_7 = 327,
        KEY_KP_8 = 328,
        KEY_KP_9 = 329,
        KEY_KP_DECIMAL = 330,
        KEY_KP_DIVIDE = 331,
        KEY_KP_MULTIPLY = 332,
        KEY_KP_SUBTRACT = 333,
        KEY_KP_ADD = 334,
        KEY_KP_ENTER = 335,
        KEY_KP_EQUAL = 336,
        KEY_LEFT_SHIFT = 340,
        KEY_LEFT_CONTROL = 341,
        KEY_LEFT_ALT = 342,
        KEY_LEFT_SUPER = 343,
        KEY_RIGHT_SHIFT = 344,
        KEY_RIGHT_CONTROL = 345,
        KEY_RIGHT_ALT = 346,
        KEY_RIGHT_SUPER = 347,
        KEY_MENU = 348,
    };

#ifdef HIVE_PLATFORM_GLFW
    int getBackendKey(KeyCode code) {
        switch (code)
        {
        case KeyCode::KEY_SPACE:
            return 32;
        case KeyCode::KEY_APOSTROPHE:
            return 39;
        case KeyCode::KEY_COMMA:
            return 44;
        case KeyCode::KEY_MINUS:
            return 45;
        case KeyCode::KEY_PERIOD:
            return 46;
        case KeyCode::KEY_SLASH:
            return 47;
        case KeyCode::KEY_0:
            return 48;
        case KeyCode::KEY_1:
            return 49;
        case KeyCode::KEY_2:
            return 50;
        case KeyCode::KEY_3:
            return 51;
        case KeyCode::KEY_4:
            return 52;
        case KeyCode::KEY_5:
            return 53;
        case KeyCode::KEY_6:
            return 54;
        case KeyCode::KEY_7:
            return 55;
        case KeyCode::KEY_8:
            return 56;
        case KeyCode::KEY_9:
            return 57;
        case KeyCode::KEY_SEMICOLON:
            return 59;
        case KeyCode::KEY_EQUAL:
            return 61;
        case KeyCode::KEY_A:
            return 65;
        case KeyCode::KEY_B:
            return 66;
        case KeyCode::KEY_C:
            return 67;
        case KeyCode::KEY_D:
            return 68;
        case KeyCode::KEY_E:
            return 69;
        case KeyCode::KEY_F:
            return 70;
        case KeyCode::KEY_G:
            return 71;
        case KeyCode::KEY_H:
            return 72;
        case KeyCode::KEY_I:
            return 73;
        case KeyCode::KEY_J:
            return 74;
        case KeyCode::KEY_K:
            return 75;
        case KeyCode::KEY_L:
            return 76;
        case KeyCode::KEY_M:
            return 77;
        case KeyCode::KEY_N:
            return 78;
        case KeyCode::KEY_O:
            return 79;
        case KeyCode::KEY_P:
            return 80;
        case KeyCode::KEY_Q:
            return 81;
        case KeyCode::KEY_R:
            return 82;
        case KeyCode::KEY_S:
            return 83;
        case KeyCode::KEY_T:
            return 84;
        case KeyCode::KEY_U:
            return 85;
        case KeyCode::KEY_V:
            return 86;
        case KeyCode::KEY_W:
            return 87;
        case KeyCode::KEY_X:
            return 88;
        case KeyCode::KEY_Y:
            return 89;
        case KeyCode::KEY_Z:
            return 90;
        case KeyCode::KEY_LEFT_BRACKET:
            return 91;
        case KeyCode::KEY_BACKSLASH:
            return 92;
        case KeyCode::KEY_RIGHT_BRACKET:
            return 93;
        case KeyCode::KEY_GRAVE_ACCENT:
            return 96;
        case KeyCode::KEY_ESCAPE:
            return 256;
        case KeyCode::KEY_ENTER:
            return 257;
        case KeyCode::KEY_TAB:
            return 258;
        case KeyCode::KEY_BACKSPACE:
            return 259;
        case KeyCode::KEY_INSERT:
            return 260;
        case KeyCode::KEY_DELETE:
            return 261;
        case KeyCode::KEY_RIGHT:
            return 262;
        case KeyCode::KEY_LEFT:
            return 263;
        case KeyCode::KEY_DOWN:
            return 264;
        case KeyCode::KEY_UP:
            return 265;
        case KeyCode::KEY_PAGE_UP:
            return 266;
        case KeyCode::KEY_PAGE_DOWN:
            return 267;
        case KeyCode::KEY_HOME:
            return 268;
        case KeyCode::KEY_END:
            return 269;
        case KeyCode::KEY_CAPS_LOCK:
            return 280;
        case KeyCode::KEY_SCROLL_LOCK:
            return 281;
        case KeyCode::KEY_NUM_LOCK:
            return 282;
        case KeyCode::KEY_PRINT_SCREEN:
            return 283;
        case KeyCode::KEY_PAUSE:
            return 284;
        case KeyCode::KEY_F1:
            return 290;
        case KeyCode::KEY_F2:
            return 291;
        case KeyCode::KEY_F3:
            return 292;
        case KeyCode::KEY_F4:
            return 293;
        case KeyCode::KEY_F5:
            return 294;
        case KeyCode::KEY_F6:
            return 295;
        case KeyCode::KEY_F7:
            return 296;
        case KeyCode::KEY_F8:
            return 297;
        case KeyCode::KEY_F9:
            return 298;
        case KeyCode::KEY_F10:
            return 299;
        case KeyCode::KEY_F11:
            return 300;
        case KeyCode::KEY_F12:
            return 301;

        case KeyCode::KEY_KP_0:
            return 320;
        case KeyCode::KEY_KP_1:
            return 321;
        case KeyCode::KEY_KP_2:
            return 322;
        case KeyCode::KEY_KP_3:
            return 323;
        case KeyCode::KEY_KP_4:
            return 324;
        case KeyCode::KEY_KP_5:
            return 325;
        case KeyCode::KEY_KP_6:
            return 326;
        case KeyCode::KEY_KP_7:
            return 327;
        case KeyCode::KEY_KP_8:
            return 328;
        case KeyCode::KEY_KP_9:
            return 329;
        case KeyCode::KEY_KP_DECIMAL:
            return 330;
        case KeyCode::KEY_KP_DIVIDE:
            return 331;
        case KeyCode::KEY_KP_MULTIPLY:
            return 332;
        case KeyCode::KEY_KP_SUBTRACT:
            return 333;
        case KeyCode::KEY_KP_ADD:
            return 334;
        case KeyCode::KEY_KP_ENTER:
            return 335;
        case KeyCode::KEY_KP_EQUAL:
            return 336;
        case KeyCode::KEY_LEFT_SHIFT:
            return 340;
        case KeyCode::KEY_LEFT_CONTROL:
            return 341;
        case KeyCode::KEY_LEFT_ALT:
            return 342;
        case KeyCode::KEY_LEFT_SUPER:
            return 343;
        case KeyCode::KEY_RIGHT_SHIFT:
            return 344;
        case KeyCode::KEY_RIGHT_CONTROL:
            return 345;
        case KeyCode::KEY_RIGHT_ALT:
            return 346;
        case KeyCode::KEY_RIGHT_SUPER:
            return 347;
        case KeyCode::KEY_MENU:
            return 348;
        }
    };
#endif

#ifdef HIVE_PLATFORM_RAYLIB
#include <raylib.h>
    int getBackendKey(KeyCode code)

    {
        switch (code)
        {
        case KeyCode::KEY_SPACE:
            return KeyboardKey::KEY_SPACE;
        case KeyCode::KEY_APOSTROPHE:
            return KeyboardKey::KEY_APOSTROPHE;
        case KeyCode::KEY_COMMA:
            return KeyboardKey::KEY_COMMA;
        case KeyCode::KEY_MINUS:
            return KeyboardKey::KEY_MINUS;
        case KeyCode::KEY_PERIOD:
            return KeyboardKey::KEY_PERIOD;
        case KeyCode::KEY_SLASH:
            return KeyboardKey::KEY_SLASH;
        case KeyCode::KEY_0:
            return KeyboardKey::KEY_ZERO;
        case KeyCode::KEY_1:
            return KeyboardKey::KEY_ONE;
        case KeyCode::KEY_2:
            return KeyboardKey::KEY_TWO;
        case KeyCode::KEY_3:
            return KeyboardKey::KEY_THREE;
        case KeyCode::KEY_4:
            return KeyboardKey::KEY_FOUR;
        case KeyCode::KEY_5:
            return KeyboardKey::KEY_FIVE;
        case KeyCode::KEY_6:
            return KeyboardKey::KEY_SIX;
        case KeyCode::KEY_7:
            return KeyboardKey::KEY_SEVEN;
        case KeyCode::KEY_8:
            return KeyboardKey::KEY_EIGHT;
        case KeyCode::KEY_9:
            return KeyboardKey::KEY_NINE;
        case KeyCode::KEY_SEMICOLON:
            return KeyboardKey::KEY_SEMICOLON;
        case KeyCode::KEY_EQUAL:
            return KeyboardKey::KEY_EQUAL;
        case KeyCode::KEY_A:
            return KeyboardKey::KEY_A;
        case KeyCode::KEY_B:
            return KeyboardKey::KEY_B;
        case KeyCode::KEY_C:
            return KeyboardKey::KEY_C;
        case KeyCode::KEY_D:
            return KeyboardKey::KEY_D;
        case KeyCode::KEY_E:
            return KeyboardKey::KEY_E;
        case KeyCode::KEY_F:
            return KeyboardKey::KEY_F;
        case KeyCode::KEY_G:
            return KeyboardKey::KEY_G;
        case KeyCode::KEY_H:
            return KeyboardKey::KEY_H;
        case KeyCode::KEY_I:
            return KeyboardKey::KEY_I;
        case KeyCode::KEY_J:
            return KeyboardKey::KEY_J;
        case KeyCode::KEY_K:
            return KeyboardKey::KEY_K;
        case KeyCode::KEY_L:
            return KeyboardKey::KEY_L;
        case KeyCode::KEY_M:
            return KeyboardKey::KEY_M;
        case KeyCode::KEY_N:
            return KeyboardKey::KEY_N;
        case KeyCode::KEY_O:
            return KeyboardKey::KEY_O;
        case KeyCode::KEY_P:
            return KeyboardKey::KEY_P;
        case KeyCode::KEY_Q:
            return KeyboardKey::KEY_Q;
        case KeyCode::KEY_R:
            return KeyboardKey::KEY_R;
        case KeyCode::KEY_S:
            return KeyboardKey::KEY_S;
        case KeyCode::KEY_T:
            return KeyboardKey::KEY_T;
        case KeyCode::KEY_U:
            return KeyboardKey::KEY_U;
        case KeyCode::KEY_V:
            return KeyboardKey::KEY_V;
        case KeyCode::KEY_W:
            return KeyboardKey::KEY_W;
        case KeyCode::KEY_X:
            return KeyboardKey::KEY_X;
        case KeyCode::KEY_Y:
            return KeyboardKey::KEY_Y;
        case KeyCode::KEY_Z:
            return KeyboardKey::KEY_Z;
        case KeyCode::KEY_LEFT_BRACKET:
            return KeyboardKey::KEY_LEFT_BRACKET;
        case KeyCode::KEY_BACKSLASH:
            return KeyboardKey::KEY_BACKSLASH;
        case KeyCode::KEY_RIGHT_BRACKET:
            return KeyboardKey::KEY_RIGHT_BRACKET;
        case KeyCode::KEY_GRAVE_ACCENT:
            return KeyboardKey::KEY_GRAVE;
        case KeyCode::KEY_ESCAPE:
            return KeyboardKey::KEY_ESCAPE;
        case KeyCode::KEY_ENTER:
            return KeyboardKey::KEY_ENTER;
        case KeyCode::KEY_TAB:
            return KeyboardKey::KEY_TAB;
        case KeyCode::KEY_BACKSPACE:
            return KeyboardKey::KEY_BACKSPACE;
        case KeyCode::KEY_INSERT:
            return KeyboardKey::KEY_INSERT;
        case KeyCode::KEY_DELETE:
            return KeyboardKey::KEY_DELETE;
        case KeyCode::KEY_RIGHT:
            return KeyboardKey::KEY_RIGHT;
        case KeyCode::KEY_LEFT:
            return KeyboardKey::KEY_LEFT;
        case KeyCode::KEY_DOWN:
            return KeyboardKey::KEY_DOWN;
        case KeyCode::KEY_UP:
            return KeyboardKey::KEY_UP;
        case KeyCode::KEY_PAGE_UP:
            return KeyboardKey::KEY_PAGE_UP;
        case KeyCode::KEY_PAGE_DOWN:
            return KeyboardKey::KEY_PAGE_DOWN;
        case KeyCode::KEY_HOME:
            return KeyboardKey::KEY_HOME;
        case KeyCode::KEY_END:
            return KeyboardKey::KEY_END;
        case KeyCode::KEY_CAPS_LOCK:
            return KeyboardKey::KEY_CAPS_LOCK;
        case KeyCode::KEY_SCROLL_LOCK:
            return KeyboardKey::KEY_SCROLL_LOCK;
        case KeyCode::KEY_NUM_LOCK:
            return KeyboardKey::KEY_NUM_LOCK;
        case KeyCode::KEY_PRINT_SCREEN:
            return KeyboardKey::KEY_PRINT_SCREEN;
        case KeyCode::KEY_PAUSE:
            return KeyboardKey::KEY_PAUSE;
        case KeyCode::KEY_F1:
            return KeyboardKey::KEY_F1;
        case KeyCode::KEY_F2:
            return KeyboardKey::KEY_F2;
        case KeyCode::KEY_F3:
            return KeyboardKey::KEY_F3;
        case KeyCode::KEY_F4:
            return KeyboardKey::KEY_F4;
        case KeyCode::KEY_F5:
            return KeyboardKey::KEY_F5;
        case KeyCode::KEY_F6:
            return KeyboardKey::KEY_F6;
        case KeyCode::KEY_F7:
            return KeyboardKey::KEY_F7;
        case KeyCode::KEY_F8:
            return KeyboardKey::KEY_F8;
        case KeyCode::KEY_F9:
            return KeyboardKey::KEY_F9;
        case KeyCode::KEY_F10:
            return KeyboardKey::KEY_F10;
        case KeyCode::KEY_F11:
            return KeyboardKey::KEY_F11;
        case KeyCode::KEY_F12:
            return KeyboardKey::KEY_F12;
        case KeyCode::KEY_KP_0:
            return KeyboardKey::KEY_KP_0;
        case KeyCode::KEY_KP_1:
            return KeyboardKey::KEY_KP_1;
        case KeyCode::KEY_KP_2:
            return KeyboardKey::KEY_KP_2;
        case KeyCode::KEY_KP_3:
            return KeyboardKey::KEY_KP_3;
        case KeyCode::KEY_KP_4:
            return KeyboardKey::KEY_KP_4;
        case KeyCode::KEY_KP_5:
            return KeyboardKey::KEY_KP_5;
        case KeyCode::KEY_KP_6:
            return KeyboardKey::KEY_KP_6;
        case KeyCode::KEY_KP_7:
            return KeyboardKey::KEY_KP_7;
        case KeyCode::KEY_KP_8:
            return KeyboardKey::KEY_KP_8;
        case KeyCode::KEY_KP_9:
            return KeyboardKey::KEY_KP_9;
        case KeyCode::KEY_KP_DECIMAL:
            return KeyboardKey::KEY_KP_DECIMAL;
        case KeyCode::KEY_KP_DIVIDE:
            return KeyboardKey::KEY_KP_DIVIDE;
        case KeyCode::KEY_KP_MULTIPLY:
            return KeyboardKey::KEY_KP_MULTIPLY;
        case KeyCode::KEY_KP_SUBTRACT:
            return KeyboardKey::KEY_KP_SUBTRACT;
        case KeyCode::KEY_KP_ADD:
            return KeyboardKey::KEY_KP_ADD;
        case KeyCode::KEY_KP_ENTER:
            return KeyboardKey::KEY_KP_ENTER;
        case KeyCode::KEY_KP_EQUAL:
            return KeyboardKey::KEY_KP_EQUAL;
        case KeyCode::KEY_LEFT_SHIFT:
            return KeyboardKey::KEY_LEFT_SHIFT;
        case KeyCode::KEY_LEFT_CONTROL:
            return KeyboardKey::KEY_LEFT_CONTROL;
        case KeyCode::KEY_LEFT_ALT:
            return KeyboardKey::KEY_LEFT_ALT;
        case KeyCode::KEY_LEFT_SUPER:
            return KeyboardKey::KEY_LEFT_SUPER;
        case KeyCode::KEY_RIGHT_SHIFT:
            return KeyboardKey::KEY_RIGHT_SHIFT;
        case KeyCode::KEY_RIGHT_CONTROL:
            return KeyboardKey::KEY_RIGHT_CONTROL;
        case KeyCode::KEY_RIGHT_ALT:
            return KeyboardKey::KEY_RIGHT_ALT;
        case KeyCode::KEY_RIGHT_SUPER:
            return KeyboardKey::KEY_RIGHT_SUPER;
        case KeyCode::KEY_MENU:
            return KeyboardKey::KEY_MENU;
        }
    }
#endif

}
// #endif
#endif