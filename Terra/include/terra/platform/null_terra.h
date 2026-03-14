#pragma once

#include <terra/terra.h>

namespace terra
{
    struct WindowContext
    {
        const char* m_title{nullptr};
        int m_width{0};
        int m_height{0};

        InputState m_currentInputState{};
    };
} // namespace terra
